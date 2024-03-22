// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "proto_processing_lib/proto_scrubber/proto_scrubber.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/casts.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_log.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "proto_processing_lib/proto_scrubber/checksummer.h"
#include "proto_processing_lib/proto_scrubber/constants.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/proto_scrubber_enums.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/wire_format_lite.h"
#include "ocpdiag/core/compat/status_macros.h"

namespace proto_processing_lib::proto_scrubber {
namespace {

using ::google::protobuf::field_extraction::CodedInputStreamWrapper;
using google::protobuf::field_extraction::CordMessageData;
using google::protobuf::field_extraction::MessageData;
using google::protobuf::internal::WireFormatLite;
using google::protobuf::io::CodedInputStream;
using google::protobuf::io::CodedOutputStream;

// Set storing type_urls of Google Public Fields, which should not be scrubbed
// by all filters.
const absl::flat_hash_set<std::string>& GooglePublicFieldsTypeUrl() {
  static absl::flat_hash_set<std::string>* google_public_fields_type_url =
      []() {
        return new absl::flat_hash_set<std::string>(
            {absl::StrCat(kTypeServiceBaseUrl, "/", kGdataErrorsTypeName),
             absl::StrCat(kTypeServiceBaseUrl, "/", kGdataMediaTypeName),
             absl::StrCat(kTypeServiceBaseUrl, "/", kMediaRequestInfoTypeName),
             absl::StrCat(kTypeServiceBaseUrl, "/", kMediaResponseInfoTypeName),
             absl::StrCat(kTypeServiceBaseUrl, "/", kGdataTraceTypeName)});
      }();
  return *google_public_fields_type_url;
}

// Gets whether the given field_type_url matches the type_url of any
// Google Public Fields.
bool IsGooglePublicField(absl::string_view field_type_url) {
  return GooglePublicFieldsTypeUrl().contains(field_type_url);
}

}  // namespace

ProtoScrubber::ProtoScrubber(
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder,
    const std::vector<const FieldCheckerInterface*>& field_checkers,
    ScrubberContext scrubber_context, bool field_check_only,
    const int initial_message_depth)
    : type_(type),
      type_finder_(std::move(type_finder)),
      field_checkers_(field_checkers),
      scrubber_context_(scrubber_context),
      field_check_only_(field_check_only),
      initial_message_depth_(initial_message_depth) {
  for (const auto* checker : field_checkers) {
    if (checker->SupportAny()) {
      any_field_checkers_.push_back(checker);
    }
  }
}

ProtoScrubber::ProtoScrubber(
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder,
    const std::vector<const FieldCheckerInterface*>& field_checkers,
    ScrubberContext scrubber_context, bool field_check_only)
    : ProtoScrubber(type, std::move(type_finder), field_checkers,
                    scrubber_context, field_check_only,
                    /*initial_message_depth=*/0) {}

void ProtoScrubber::AddFieldFilter(const FieldCheckerInterface* checker) {
  field_checkers_.push_back(checker);
  if (checker->SupportAny()) {
    any_field_checkers_.push_back(checker);
  }
}

absl::Status ProtoScrubber::Scrub(MessageData* message) const {
  return Scrub(message, /*parent_tags=*/nullptr, /*is_checksum_enabled=*/false)
      .status();
}

absl::StatusOr<uint32_t> ProtoScrubber::ScrubWithChecksum(
    MessageData* message) const {
  std::vector<std::string> parent_tags;
  return Scrub(message, &parent_tags, /*is_checksum_enabled=*/true);
}

absl::StatusOr<uint32_t> ProtoScrubber::Scrub(
    MessageData* message, std::vector<std::string>* parent_tags,
    const bool is_checksum_enabled) const {
  if (message == nullptr || field_checkers_.empty()) {
    // Empty input or no FieldCheckers, so there is no scrubbing to be done.
    return 0;
  }

  std::unique_ptr<CodedInputStreamWrapper> input_stream_wrapper =
      message->CreateCodedInputStreamWrapper();
  std::vector<std::string> path;
  std::vector<TagCopy> tag_insert;
  uint32_t scrubbed_fields_checksum = 0;
  // Walks through the input and filters fields we want into an intermediate
  // buffer. Also collects tags and lengths that need to be inserted into the
  // final result.
  absl::StatusOr<uint32_t> result = ScanField(
      type_, /*tag=*/0, &input_stream_wrapper->Get(), &path, parent_tags,
      &tag_insert, &scrubbed_fields_checksum, is_checksum_enabled);
  if (!result.ok()) {
    return result.status();
  }

  // This will ensure the path is properly reduced on stack unwind.
  ABSL_LOG_IF(FATAL, !path.empty())
      << "After successfully scrubbing the message, path "
         "should be empty but was size "
      << path.size() << " with value: " << absl::StrJoin(path, ".");

  if (!field_check_only_) {
    absl::Cord output;
    RETURN_IF_ERROR(CopyAndInsertTag(tag_insert, message, &output));
    message->CopyFrom(output);
  }

  return scrubbed_fields_checksum;
}

ProtoScrubber::ProtoScrubber(
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder,
    const std::vector<const FieldCheckerInterface*>& field_checkers,
    ScrubberContext scrubber_context)
    : ProtoScrubber(type, std::move(type_finder), field_checkers,
                    scrubber_context, false) {}

bool ProtoScrubber::AddTagCopyOrRollToPrevious(
    int copy_start, uint32_t copy_length,
    std::vector<TagCopy>* tag_insert) const {
  // Checks if the current child field can be combined into the previous
  // TagInsert (i.e. nothing is skipped between them), so they can be copied
  // together.
  bool roll_to_previous = false;
  if (!tag_insert->empty()) {
    TagCopy* previous = &tag_insert->back();
    if (previous->copy_length > 0 &&
        previous->copy_start + previous->copy_length == copy_start) {
      previous->copy_length += copy_length;
      roll_to_previous = true;
    } else if (previous->copy_start == copy_start &&
               previous->copy_length == copy_length) {
      roll_to_previous = true;
    }
  }
  if (!roll_to_previous) {
    tag_insert->push_back(MakeTagCopyForCopying(copy_start, copy_length));
  }
  return roll_to_previous;
}

absl::StatusOr<uint32_t> ProtoScrubber::ScanField(
    const google::protobuf::Type* type, uint32_t tag,
    google::protobuf::io::CodedInputStream* input_stream, std::vector<std::string>* path,
    std::vector<std::string>* parent_tags, std::vector<TagCopy>* tag_insert,
    uint32_t* scrubbed_fields_checksum, const bool is_checksum_enabled) const {
  // For non-root messages, we need to remember the original limit of
  // input_stream before setting it to the length of the current message.
  CodedInputStream::Limit old_limit = 0;
  int tag_insert_position = 0;
  if (tag != 0) {
    uint32_t message_length;
    input_stream->ReadVarint32(&message_length);
    // Sets the limit of input_stream to be the length of the current message,
    // so we can do a sanity check at the end of this method to make sure we
    // have consumed the whole message. Also remembers the original limit so
    // we can set it back after the check.
    old_limit = input_stream->PushLimit(message_length);
    // Creates a new TagCopy in tag_insert. Also, needs to remember the
    // position of the newly created TagCopy instance within tag_insert so we
    // can update its length after scanning through all the fields of the
    // current message.
    tag_insert->push_back(MakeTagCopyForInserting(tag, 0));
    tag_insert_position = tag_insert->size() - 1;
  }

  uint32_t result = 0;
  int position = input_stream->CurrentPosition();
  uint32_t child_tag = input_stream->ReadTag();
  while (child_tag != 0) {
    const int32_t field_number = WireFormatLite::GetTagFieldNumber(child_tag);
    const google::protobuf::Field* field = FindField(type, field_number);
    FieldCheckResults check_result = FieldCheckResults::kInclude;
    FieldFilters excluded_by_filter = FieldFilters::Unspecified;
    if (field != nullptr) {
      if (type->name() != kStructValueType) {
        // Struct value field names are already included.
        path->push_back(field->name());
      }
      // Checks Field only if it is not a Google Public fields. We don't want
      // Google Public fields to get scrubbed.
      if (!IsGooglePublicField(field->type_url()))
        check_result = CheckField(*path, field, &excluded_by_filter);
    } else {
      // Checks field even if it is NULL.
      check_result = CheckField(*path, field, &excluded_by_filter);
      if (type != nullptr && check_result == FieldCheckResults::kExclude) {
        IncreaseUnknownFieldDroppedCount(type->name(), field_number,
                                         scrubber_context_);
      }
    }

    // Handles CheckField results.
    if (check_result == FieldCheckResults::kExclude) {
      const std::string type_name =
          type == nullptr ? "Unknown type" : type->name();
      IncreaseFieldDroppedCount(type_name, field_number, scrubber_context_,
                                excluded_by_filter);
      if (field_check_only_) {
return absl::Status(
    absl::StatusCode::kInvalidArgument,
    absl::StrCat("Unknown field '", absl::StrJoin(*path, "."),
                 "' in Type '", type_name, "' is being excluded."));
      }
      // Skip checksum for unknown field.
      if (field != nullptr && is_checksum_enabled) {
        RETURN_IF_ERROR(CalculateScrubbedFieldsChecksum(
            input_stream, field, parent_tags, &child_tag, &position,
            scrubbed_fields_checksum));
      } else {
        // Skips this field.
        WireFormatLite::SkipField(input_stream, child_tag);
        position = input_stream->CurrentPosition();
        child_tag = input_stream->ReadTag();
      }
    } else if (check_result == FieldCheckResults::kInclude) {
      // The current field should be included wholly, save its starting
      // position and length so it can be copied to the result.
      WireFormatLite::SkipField(input_stream, child_tag);
      uint32_t length = input_stream->CurrentPosition() - position;
      AddTagCopyOrRollToPrevious(position, length, tag_insert);
      result += length;
      position = input_stream->CurrentPosition();
      child_tag = input_stream->ReadTag();
    } else if (check_result == FieldCheckResults::kPartial) {
      if (field == nullptr) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrCat(
            "Unknown field ", field_number, " in Type '",
            type == nullptr ? "Unknown type" : type->name(),
            "' cannot be partially included in the scrubbing result."));
      }

      const google::protobuf::Type* child_type = GetFieldType(field);
      ASSIGN_OR_RETURN(
          uint32_t child_length,
          ScanChildField(type, child_type, *field, child_tag, position,
                         is_checksum_enabled, input_stream, path, parent_tags,
                         tag_insert, scrubbed_fields_checksum));

      result += child_length;
      position = input_stream->CurrentPosition();
      child_tag = input_stream->ReadTag();
    } else {
return absl::Status(
  absl::StatusCode::kUnimplemented, absl::StrCat("Field checking result '",
static_cast<int>(check_result),
           "' is not supported."));
    }
    if (field != nullptr && type != nullptr &&
        type->name() != kStructValueType) {
      path->pop_back();
    }
  }

  // Checks if the whole message (limited by its length set at the beginning
  // of this method) has been consumed.
  if (!input_stream->ConsumedEntireMessage()) {
return absl::Status(absl::StatusCode::kInvalidArgument,
                "Nested protocol message not parsed in its entirety.");
  }
  if (tag != 0) {
    input_stream->PopLimit(old_limit);
    if (result == 0 && type != nullptr &&
        (type->name() == kStructType || type->name() == kStructValueType ||
         type->name() == kStructListValueType)) {
      // Skip Struct type and List type field if no entry is matched.
      tag_insert->erase(tag_insert->begin() + tag_insert_position,
                        tag_insert->end());
    } else {
      (*tag_insert)[tag_insert_position].length = result;
      result += CodedOutputStream::VarintSize32(result);
      result += CodedOutputStream::VarintSize32(tag);
    }
  }
  return result;
}

absl::StatusOr<uint32_t> ProtoScrubber::ScanAnyField(
    const google::protobuf::Field& field, uint32_t tag_of_any_field,
    int tag_position, google::protobuf::io::CodedInputStream* input_stream,
    const std::vector<std::string>* path, std::vector<std::string>* parent_tags,
    std::vector<TagCopy>* tag_insert, uint32_t* scrubbed_fields_checksum,
    const bool is_checksum_enabled) const {
  uint32_t field_length;
  input_stream->ReadVarint32(&field_length);  // length of the "Any" field.
  CodedInputStream::Limit old_limit = input_stream->PushLimit(field_length);

  uint32_t tag;
  std::string type_url;
  auto value = std::make_unique<CordMessageData>();
  // First read out the type_url and value from the proto stream
  auto current_position = input_stream->CurrentPosition();
  TagCopy type_url_tag_copy;
  uint32_t value_tag = 0;
  uint32_t type_url_tag = 0;
  for (tag = input_stream->ReadTag(); tag != 0; tag = input_stream->ReadTag()) {
    const int32_t field_number = WireFormatLite::GetTagFieldNumber(tag);
    // 'type_url' has field number of 1 and 'value' has field number 2
    // //google/protobuf/any.proto
    if (field_number == 1) {
      type_url_tag = tag;
      // read type_url
      uint32_t type_url_size;
      input_stream->ReadVarint32(&type_url_size);
      input_stream->ReadString(&type_url, type_url_size);
      type_url_tag_copy = MakeTagCopyForCopying(
          current_position, input_stream->CurrentPosition() - current_position);
    } else if (field_number == 2) {
      value_tag = tag;
      // read value
      uint32_t value_size;
      input_stream->ReadVarint32(&value_size);
      input_stream->ReadCord(&value->Cord(), value_size);
    } else {
      // Any message should only have field 1 and 2. Ignore any other fields.
      IncreaseUnknownFieldDroppedCount(kAnyType, field_number,
                                       scrubber_context_);
      IncreaseFieldDroppedCount(kAnyType, field_number, scrubber_context_,
                                FieldFilters::UnknownFieldFilter);
      WireFormatLite::SkipField(input_stream, tag);
    }
    current_position = input_stream->CurrentPosition();
  }

  if (!input_stream->ConsumedEntireMessage()) {
return absl::Status(absl::StatusCode::kInvalidArgument,
                "Nested protocol message not parsed in its entirety.");
  }
  input_stream->PopLimit(old_limit);
  FieldCheckResults type_check_result = FieldCheckResults::kInclude;
  const google::protobuf::Type* resolved_type = nullptr;
  FieldFilters excluded_by_filter = FieldFilters::Unspecified;
  if (!type_url.empty()) {
    resolved_type = type_finder_(type_url);
    if (resolved_type) {
      for (const auto* checker : any_field_checkers_) {
        auto check_result = checker->CheckType(resolved_type);
        if (check_result == FieldCheckResults::kExclude ||
            check_result == FieldCheckResults::kPartial) {
          type_check_result = check_result;
          if (check_result == FieldCheckResults::kExclude) {
            excluded_by_filter = checker->FilterName();
            break;
          }
        }
      }
    }
  } else {
    LOG_EVERY_N(WARNING, 10000)
        << "Invalid google.protobuf.Any field, the type_url is missing.";
  }

  if (type_check_result == FieldCheckResults::kInclude) {
    uint32_t length = input_stream->CurrentPosition() - tag_position;
    AddTagCopyOrRollToPrevious(tag_position, length, tag_insert);
    return length;
  } else if (type_check_result == FieldCheckResults::kExclude) {
    IncreaseFieldDroppedCount(resolved_type->name(), field.number(),
                              scrubber_context_, excluded_by_filter);

    if (field_check_only_) {
return absl::Status(
  absl::StatusCode::kInvalidArgument,
  absl::StrCat("Unknown type'", type_url, "' is being excluded."));
    }

    // The whole Any field is excluded, needs to calculate its checksum.
    if (is_checksum_enabled) {
      // Checksum for the value field.
      uint32_t any_field_checksum = 0;

      if (value_tag != 0) {
        parent_tags->push_back(std::to_string(value_tag));
        ASSIGN_OR_RETURN(
            any_field_checksum,
            Checksummer::GetDefault()->CalculateChecksum(
                &value->Cord(), resolved_type, type_finder_, parent_tags));
        parent_tags->pop_back();
      }

      // Checksum for the type url.
      parent_tags->push_back(std::to_string(type_url_tag));
      Checksummer::GetDefault()->AddToChecksum(absl::StrJoin(*parent_tags, "."),
                                               type_url, &any_field_checksum);
      parent_tags->pop_back();

      *scrubbed_fields_checksum =
          *scrubbed_fields_checksum ^ any_field_checksum;
    }
    return 0;
  }

  // Partial of the Any field is included, needs to further scrub the value.
  if (value_tag != 0) {
    const int any_field_depth = initial_message_depth_ + path->size();
    ProtoScrubber any_scrubber(resolved_type, type_finder_, any_field_checkers_,
                               scrubber_context_, field_check_only_,
                               any_field_depth);
    if (is_checksum_enabled) {
      parent_tags->push_back(std::to_string(value_tag));
    }
    auto scrub_result =
        any_scrubber.Scrub(value.get(), parent_tags, is_checksum_enabled);
    if (!scrub_result.ok()) {
      return scrub_result;
    }
    if (is_checksum_enabled) {
      *scrubbed_fields_checksum =
          *scrubbed_fields_checksum ^ scrub_result.value();
      parent_tags->pop_back();
    }
  }

  // When scrubbing the type URL in the response, this will catch an empty
  // value either in the initial response or the response after scrubbing.
  if ((value_tag == 0 || value->IsEmpty())
  ) {
    // Remove type URL from checksum.
    if (is_checksum_enabled) {
      uint32_t type_url_checksum = 0;

      parent_tags->push_back(std::to_string(type_url_tag));
      Checksummer::GetDefault()->AddToChecksum(absl::StrJoin(*parent_tags, "."),
                                               type_url, &type_url_checksum);
      parent_tags->pop_back();

      *scrubbed_fields_checksum = *scrubbed_fields_checksum ^ type_url_checksum;
    }

    return 0;
  }

  uint32_t result_length = type_url_tag_copy.copy_length;
  if (value_tag != 0) {
    result_length += CodedOutputStream::VarintSize32(value_tag);
    result_length += CodedOutputStream::VarintSize32(value->Size());
    result_length += value->Size();
  }

  tag_insert->push_back(
      MakeTagCopyForInserting(tag_of_any_field, result_length));
  tag_insert->push_back(std::move(type_url_tag_copy));
  if (value_tag != 0) {
    TagCopy value_tag_copy{value_tag, static_cast<uint32_t>(value->Size()), 0,
                           0, std::move(value)};
    tag_insert->push_back(std::move(value_tag_copy));
  }

  // Add the length of the serialized tag&length of the Any field to the
  // result.
  result_length += CodedOutputStream::VarintSize32(result_length);
  result_length += CodedOutputStream::VarintSize32(tag_of_any_field);

  return result_length;
}

void ProtoScrubber::IncreaseUnknownFieldDroppedCount(
    absl::string_view type_name, int32_t field_number,
    ScrubberContext scrubber_context) const {
  // Not Implemented
}

void ProtoScrubber::IncreaseFieldDroppedCount(
    const std::string& type_name, int32_t field_number,
    const ScrubberContext& scrubber_context,
    const FieldFilters& excluded_by_filter) const {
  // Not Implemented
}

FieldCheckResults ProtoScrubber::CheckEnumValue(
    const google::protobuf::Field* field, std::vector<std::string>* path,
    google::protobuf::io::CodedInputStream* input_stream,
    std::vector<std::string>* parent_tags, uint32_t* scrubbed_fields_checksum,
    FieldFilters* excluded_by_filter, const bool is_checksum_enabled) const {
  // Adds the current value of the enum field to the end of the 'path' and
  // calls CheckField().
  uint32_t enum_value;
  input_stream->ReadVarint32(&enum_value);
  path->push_back(absl::StrCat(enum_value));
  FieldCheckResults result = CheckField(*path, field, excluded_by_filter);
  // Enum value has been read, directly adds to checksum if it is Excluded.
  if (is_checksum_enabled && result == FieldCheckResults::kExclude) {
    Checksummer::GetDefault()->AddToChecksum(absl::StrJoin(*parent_tags, "."),
                                             std::to_string(enum_value),
                                             scrubbed_fields_checksum);
  }
  path->pop_back();
  return result;
}

absl::StatusOr<uint32_t> ProtoScrubber::ScanEnumField(
    const google::protobuf::Type* type, const google::protobuf::Field& field,
    uint32_t tag, int tag_position, google::protobuf::io::CodedInputStream* input_stream,
    std::vector<std::string>* path, std::vector<std::string>* parent_tags,
    std::vector<TagCopy>* tag_insert, uint32_t* scrubbed_fields_checksum,
    const bool is_checksum_enabled) const {
  int result_length = 0;
  // Checks if the enum field is packed repeated.
  if (tag == WireFormatLite::MakeTag(
                 field.number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED)) {
    // Use unpacked tag number for checksum calculation.
    if (is_checksum_enabled) {
      parent_tags->push_back(
          std::to_string(tag - WireFormatLite::WIRETYPE_LENGTH_DELIMITED));
    }
    tag_insert->push_back(MakeTagCopyForInserting(tag, 0));
    int tag_insert_position = tag_insert->size() - 1;
    uint32_t message_length;
    input_stream->ReadVarint32(&message_length);
    // Sets the limit of input_stream to be the length of the current field,
    // so we can do a sanity check at the end of this method to make sure we
    // have consumed the whole field.
    CodedInputStream::Limit old_limit = input_stream->PushLimit(message_length);
    // Loops through all packed enum values.
    while (input_stream->BytesUntilLimit() > 0) {
      int start_position = input_stream->CurrentPosition();
      FieldFilters excluded_by_filter = FieldFilters::Unspecified;
      auto check_result = CheckEnumValue(
          &field, path, input_stream, parent_tags, scrubbed_fields_checksum,
          &excluded_by_filter, is_checksum_enabled);
      if (check_result == FieldCheckResults::kInclude) {
        // CheckEnumValue() has already advanced the position of
        // 'input_stream'.
        uint32_t length = input_stream->CurrentPosition() - start_position;
        AddTagCopyOrRollToPrevious(start_position, length, tag_insert);
        result_length += length;
      } else if (check_result == FieldCheckResults::kExclude) {
        IncreaseFieldDroppedCount(type->name(), field.number(),
                                  scrubber_context_, excluded_by_filter);
      }
    }
    input_stream->PopLimit(old_limit);
    (*tag_insert)[tag_insert_position].length = result_length;
    result_length += CodedOutputStream::VarintSize32(result_length);
    result_length += CodedOutputStream::VarintSize32(tag);
  } else {
    if (is_checksum_enabled) {
      parent_tags->push_back(std::to_string(tag));
    }
    FieldFilters excluded_by_filter = FieldFilters::Unspecified;
    auto check_result = CheckEnumValue(
        &field, path, input_stream, parent_tags, scrubbed_fields_checksum,
        &excluded_by_filter, is_checksum_enabled);
    if (check_result == FieldCheckResults::kExclude) {
      IncreaseFieldDroppedCount(type->name(), field.number(), scrubber_context_,
                                excluded_by_filter);
      if (field_check_only_) {
return absl::Status(
    absl::StatusCode::kInvalidArgument,
    absl::StrCat("Unknown enum value '", absl::StrJoin(*path, "."),
                 "' in Type '", type_->name(), "' is being excluded."));
      }
    } else if (check_result == FieldCheckResults::kInclude) {
      uint32_t length = input_stream->CurrentPosition() - tag_position;
      AddTagCopyOrRollToPrevious(tag_position, length, tag_insert);
      result_length += length;
    }
  }
  if (is_checksum_enabled) {
    parent_tags->pop_back();
  }
  return result_length;
}

absl::StatusOr<uint32_t> ProtoScrubber::ScanMapField(
    const google::protobuf::Type& parent_type,
    const google::protobuf::Type& type, const google::protobuf::Field& field,
    uint32_t tag, google::protobuf::io::CodedInputStream* input_stream,
    std::vector<std::string>* path, std::vector<std::string>* parent_tags,
    std::vector<TagCopy>* tag_insert, uint32_t* scrubbed_fields_checksum,
    const bool is_checksum_enabled) const {
  tag_insert->push_back(MakeTagCopyForInserting(tag, 0));
  int tag_insert_position = tag_insert->size() - 1;

  // Get the key and value fields of the map.
  // These are from the descriptor, not the serialized wire data.
  const google::protobuf::Field* key_field = nullptr;
  const google::protobuf::Field* value_field = nullptr;
  for (int i = 0; i < type.fields_size(); ++i) {
    if (type.fields(i).number() == 1) {
      key_field = &type.fields(i);
    } else if (type.fields(i).number() == 2) {
      value_field = &type.fields(i);
    }
  }
  CHECK(key_field != nullptr && value_field != nullptr);

  const google::protobuf::Type* value_type = GetFieldType(value_field);

  uint32_t total_length = 0;
  // Checks whether the map field is packed.
  if (tag == WireFormatLite::MakeTag(
                 field.number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED) &&
      google::protobuf::FieldDescriptor::IsTypePackable(
          static_cast<google::protobuf::FieldDescriptor::Type>(field.kind()))) {
    // If the map field is packed, we need to scan through all packed entries.
    // And for each entry, we need to add a TagCopy to tag_insert before
    // scanning. The TagCopy for each entry is created with tag = 0, and only
    // the "length" will be inserted into the output stream.
    uint32_t length;
    input_stream->ReadVarint32(&length);
    CodedInputStream::Limit old_limit = input_stream->PushLimit(length);
    while (input_stream->BytesUntilLimit() > 0) {
      // We need to create one "length only" TagCopy for each entry.
      tag_insert->push_back(MakeTagCopyForInserting(0, 0));
      int entry_tag_insert_position = tag_insert->size() - 1;

      absl::StatusOr<uint32_t> entry_result =
          ScanMapEntry(parent_type, type, field, *key_field, *value_field,
                       value_type, input_stream, path, parent_tags, tag_insert,
                       scrubbed_fields_checksum, is_checksum_enabled);
      if (!entry_result.ok()) {
        return entry_result;
      }
      uint32_t entry_length = entry_result.value();
      if (entry_length == 0) {
        tag_insert->pop_back();
      } else {
        (*tag_insert)[entry_tag_insert_position].length = entry_length;
        total_length += entry_length;
        total_length += CodedOutputStream::VarintSize32(entry_length);
      }
    }
    input_stream->PopLimit(old_limit);
  } else {
    absl::StatusOr<uint32_t> entry_result =
        ScanMapEntry(parent_type, type, field, *key_field, *value_field,
                     value_type, input_stream, path, parent_tags, tag_insert,
                     scrubbed_fields_checksum, is_checksum_enabled);
    if (!entry_result.ok()) {
      return entry_result;
    }
    total_length += entry_result.value();
    if (total_length == 0) {
      tag_insert->pop_back();
      return total_length;
    }
  }
  (*tag_insert)[tag_insert_position].length = total_length;
  total_length += CodedOutputStream::VarintSize32(total_length);
  total_length += CodedOutputStream::VarintSize32(tag);
  return total_length;
}

absl::StatusOr<uint32_t> ProtoScrubber::ScanMapEntry(
    const google::protobuf::Type& parent_type,
    const google::protobuf::Type& map_type,
    const google::protobuf::Field& field,
    const google::protobuf::Field& key_field,
    const google::protobuf::Field& value_field,
    const google::protobuf::Type* value_type,
    google::protobuf::io::CodedInputStream* input_stream, std::vector<std::string>* path,
    std::vector<std::string>* parent_tags, std::vector<TagCopy>* tag_insert,
    uint32_t* scrubbed_fields_checksum, const bool is_checksum_enabled) const {
  uint32_t entry_length;
  input_stream->ReadVarint32(&entry_length);  // message length
  CodedInputStream::Limit old_limit = input_stream->PushLimit(entry_length);

  uint32_t result = 0;
  std::string map_key;
  FieldCheckResults check_result = FieldCheckResults::kInclude;
  int start_position = input_stream->CurrentPosition();
  int key_position = start_position;
  uint32_t key_length = 0;
  int current_position = start_position;
  bool is_key_found = false;
  bool is_value_added = false;
  bool is_value_null = true;
  FieldFilters excluded_by_filter = FieldFilters::Unspecified;
  uint32_t tag = input_stream->ReadTag();
  while (tag != 0) {
    const int32_t field_number = WireFormatLite::GetTagFieldNumber(tag);
    // Map field numbers are key = 1 and value = 2
    if (field_number == 1) {
      map_key = ReadMapKeyAsString(key_field, input_stream);
      if (is_key_found) {
        // Duplicate field 1 in map, ignore the previous one. This is spec
        // compliant: "For numeric types and strings, if the same field
        // appears multiple times, the parser accepts the last value it sees."
        path->pop_back();
      }
      is_key_found = true;
      path->push_back(map_key);
      check_result =
          CheckField(*path, &value_field, &map_type, &excluded_by_filter);
      // Key field may not always be the first coming data.
      key_position = current_position;
      key_length = input_stream->CurrentPosition() - current_position;
      if (check_result == FieldCheckResults::kExclude) {
        IncreaseFieldDroppedCount(map_type.name(), field_number,
                                  scrubber_context_, excluded_by_filter);
        // Adds the map key field into scrubbed_fields_checksum, if excluded.
        if (is_checksum_enabled && !field_check_only_) {
          parent_tags->push_back(std::to_string(tag));
          Checksummer::GetDefault()->AddToChecksum(
              absl::StrJoin(*parent_tags, "."), map_key,
              scrubbed_fields_checksum);
          parent_tags->pop_back();
        }
      }
    } else if (field_number == 2) {
      is_value_null = false;
      if (check_result == FieldCheckResults::kExclude) {
        IncreaseFieldDroppedCount(map_type.name(), field_number,
                                  scrubber_context_, excluded_by_filter);
        // Skips this field.
        if (field_check_only_) {
  return absl::Status(
      absl::StatusCode::kInvalidArgument,
      absl::StrCat("Unknown field '", absl::StrJoin(*path, "."),
                   "' in Type '", type_->name(),
                   "' is being excluded."));
        }
        if (is_checksum_enabled) {
          RETURN_IF_ERROR(CalculateScrubbedFieldsChecksum(
              input_stream, &value_field, parent_tags, &tag, &current_position,
              scrubbed_fields_checksum));
        } else {
          WireFormatLite::SkipField(input_stream, tag);
        }
      } else if (check_result == FieldCheckResults::kInclude) {
        // The current entry should be included wholly. Saves the starting
        // position of its key field and length of both key and value fields
        // so the entry can be copied to the result.
        WireFormatLite::SkipField(input_stream, tag);
        uint32_t length = input_stream->CurrentPosition() - current_position;
        if (is_value_added) {
          // Duplicate field 2 in map, ignore the previous one. This is spec
          // compliant: "If the same field appears multiple times, the parser
          // accepts the last value it sees."
          tag_insert->pop_back();
        }

        if (is_key_found && length > 0) {
          // Include the key because value is included.
          AddTagCopyOrRollToPrevious(key_position, key_length, tag_insert);
          result += key_length;
        }
        is_value_added = true;
        // Do not roll to previous so that when another value field comes, we
        // can pop it out.
        tag_insert->push_back(MakeTagCopyForCopying(current_position, length));
        result += length;
      } else if (check_result == FieldCheckResults::kPartial) {
        // For map entry, if the value field should only be partially
        // included, only the key field needs to be copied.
        bool rolled_to_previous =
            AddTagCopyOrRollToPrevious(start_position, key_length, tag_insert);

        ASSIGN_OR_RETURN(uint32_t child_length,
                         ScanChildField(&map_type, value_type, value_field, tag,
                                        current_position, is_checksum_enabled,
                                        input_stream, path, parent_tags,
                                        tag_insert, scrubbed_fields_checksum));

        if (child_length == 0 && map_type.name() == kStructFieldsEntryType) {
          // If Struct value is empty, key should not be included.
          if (rolled_to_previous) {
            TagCopy* previous = &tag_insert->back();
            previous->copy_length -= key_length;
          } else if (!tag_insert->empty()) {
            tag_insert->pop_back();
          }
          key_length = 0;
        }
        result += key_length + child_length;
      } else {
        return absl::Status(absl::StatusCode::kUnimplemented,
                            absl::StrCat("Field checking result '",
                                         static_cast<int>(check_result),
                                         "' is not supported."));
      }
    } else {
      // Map entries should only have field 1 and 2. Ignore any other fields.
      IncreaseUnknownFieldDroppedCount(map_type.name(), field_number,
                                       scrubber_context_);
      IncreaseFieldDroppedCount(map_type.name(), field_number,
                                scrubber_context_,
                                FieldFilters::UnknownFieldFilter);
      WireFormatLite::SkipField(input_stream, tag);
    }

    // Always read from input stream on every loop iteration to prevent
    // infinite loops on malformed tags.
    current_position = input_stream->CurrentPosition();
    tag = input_stream->ReadTag();
  }
  if (is_key_found) {
    path->pop_back();
  }

  input_stream->PopLimit(old_limit);
  return result;
}

FieldCheckResults ProtoScrubber::CheckField(
    const std::vector<std::string>& path, const google::protobuf::Field* field,
    FieldFilters* excluded_by_filter) const {
  return CheckField(path, field, nullptr, excluded_by_filter);
}

FieldCheckResults ProtoScrubber::CheckField(
    const std::vector<std::string>& path, const google::protobuf::Field* field,
    const google::protobuf::Type* parent_type,
    FieldFilters* excluded_by_filter) const {
  const int field_depth = initial_message_depth_ + path.size() - 1;
  FieldCheckResults result = FieldCheckResults::kInclude;
  for (const FieldCheckerInterface* checker : field_checkers_) {
    FieldCheckResults current_result =
        checker->CheckField(path, field, field_depth, parent_type);
    if (current_result == FieldCheckResults::kExclude) {
      *excluded_by_filter = checker->FilterName();
      return FieldCheckResults::kExclude;
    }
    if (current_result == FieldCheckResults::kPartial) {
      result = FieldCheckResults::kPartial;
    }
  }
  return result;
}

// static
const std::string ProtoScrubber::ReadMapKeyAsString(
    const google::protobuf::Field& field,
    google::protobuf::io::CodedInputStream* input_stream) {
  std::string result;
  switch (field.kind()) {
    case google::protobuf::Field::TYPE_BOOL: {
      uint64_t buffer64;
      input_stream->ReadVarint64(&buffer64);
      result = buffer64 != 0 ? "true" : "false";
      break;
    }
    case google::protobuf::Field::TYPE_INT32: {
      uint32_t buffer32;
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<int32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_INT64: {
      uint64_t buffer64;
      input_stream->ReadVarint64(&buffer64);
      result = absl::StrCat(absl::bit_cast<int64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_UINT32: {
      uint32_t buffer32;
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_UINT64: {
      uint64_t buffer64;
      input_stream->ReadVarint64(&buffer64);
      result = absl::StrCat(absl::bit_cast<uint64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_SINT32: {
      uint32_t buffer32;
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(WireFormatLite::ZigZagDecode32(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_SINT64: {
      uint64_t buffer64;
      input_stream->ReadVarint64(&buffer64);
      result = absl::StrCat(WireFormatLite::ZigZagDecode64(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_SFIXED32: {
      uint32_t buffer32;
      input_stream->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<int32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_SFIXED64: {
      uint64_t buffer64;
      input_stream->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<int64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_FIXED32: {
      uint32_t buffer32;
      input_stream->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    }
    case google::protobuf::Field::TYPE_FIXED64: {
      uint64_t buffer64;
      input_stream->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<uint64_t>(buffer64));
      break;
    }
    case google::protobuf::Field::TYPE_STRING: {
      uint32_t buffer32;
      input_stream->ReadVarint32(&buffer32);  // string size.
      input_stream->ReadString(&result, buffer32);
      break;
    }
    case google::protobuf::Field::TYPE_BYTES: {
      uint32_t buffer32;
      absl::Cord cord;
      input_stream->ReadVarint32(&buffer32);  // cord size.
      input_stream->ReadCord(&cord, buffer32);
      result = std::string(cord);
      break;
    }
    default:
      break;
  }
  return result;
}

const google::protobuf::Field* ProtoScrubber::FindField(
    const google::protobuf::Type* type, int32_t field_number) const {
  if (type == nullptr) {
    LOG(ERROR) << "Proto message type cannot be nullptr.";
    return nullptr;
  }

  for (const google::protobuf::Field& field : type->fields()) {
    if (field.number() == field_number) {
      return &field;
    }
  }
  return nullptr;
}

const google::protobuf::Type* ProtoScrubber::GetFieldType(
    const google::protobuf::Field* field) const {
  absl::MutexLock lock(&lock_);

  const auto it = field_type_map_.find(field);
  if (it != field_type_map_.end()) {
    return it->second;
  }

  const google::protobuf::Type* result =
      field->kind() == google::protobuf::Field::TYPE_MESSAGE
          ? type_finder_(field->type_url())
          : nullptr;
  field_type_map_[field] = result;
  return result;
}

absl::Status ProtoScrubber::CopyAndInsertTag(
    const std::vector<TagCopy>& tag_copy, MessageData* input,
    absl::Cord* output) const {
  int curr_pos = 0;

  for (const TagCopy& insert : tag_copy) {
    if (insert.copy_length > 0) {
      // Copies copy_length bytes starting from copy_start from the input to
      // the output. const int advance_bytes = insert.copy_start - curr_pos;
      // const int total_bytes_limit = advance_bytes + insert.copy_length;
      // const int next_pos = curr_pos + insert.copy_start +
      // insert.copy_length;
      if (input->Size() < insert.copy_start + insert.copy_length) {
        // Occurs when wire encoding's message lengths are invalid.
        // Return error, otherwise reading from the data buffer will crash.
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrFormat("Nested protocol message not parsed correctly. "
                        "Tried to read '%d' bytes from the input, but it "
                        "only has '%d' bytes.",
                        insert.copy_start + insert.copy_length - curr_pos,
                        input->Size() - curr_pos));
      }

      output->Append(input->SubData(insert.copy_start, insert.copy_length));
      curr_pos = insert.copy_start + insert.copy_length;
    } else {
      // Otherwise, the tag and length in the current TagCopy should be
      // inserted into the output. Varint32 occupies at most 10 bytes and we
      // need to write 2 of them.
      uint8_t insert_buffer[20];
      uint8_t* insert_buffer_pos = nullptr;
      if (insert.tag == 0) {
        // This is a "length only" TagCopy, only its length needs to be
        // written to the output.
        insert_buffer_pos = CodedOutputStream::WriteVarint32ToArray(
            insert.length, insert_buffer);
      } else {
        insert_buffer_pos =
            CodedOutputStream::WriteTagToArray(insert.tag, insert_buffer);
        insert_buffer_pos = CodedOutputStream::WriteVarint32ToArray(
            insert.length, insert_buffer_pos);
      }
      absl::Cord insert_cord =
          absl::Cord(absl::string_view(reinterpret_cast<char*>(insert_buffer),
                                       insert_buffer_pos - insert_buffer));
      output->Append(insert_cord);
      // Copies the non-empty scrubbed_message to the result.
      if (insert.scrubbed_message && !insert.scrubbed_message->IsEmpty()) {
        ABSL_LOG_IF(FATAL, insert.scrubbed_message->Size() != insert.length)
            << "When inserting child scrubbed message, the scrubbed message "
               "length "
            << insert.scrubbed_message->Size()
            << " mismatches from the tag insert length " << insert.length;
        output->Append(insert.scrubbed_message->Cord());
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ProtoScrubber::CalculateScrubbedFieldsChecksum(
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field* field, std::vector<std::string>* parent_tags,
    uint32_t* tag, int* position, uint32_t* scrubbed_fields_checksum) const {
  absl::StatusOr<uint32_t> checksum_or_status =
      Checksummer::GetDefault()->CalculateSingleFieldChecksum(
          type_finder_, input_stream, field, parent_tags, tag, position);
  if (!checksum_or_status.ok()) {
    LOG(WARNING)
        << "fail to calculate checksum for message inside ESF Scrubber.";
    return checksum_or_status.status();
  }
  *scrubbed_fields_checksum =
      *scrubbed_fields_checksum ^ checksum_or_status.value();
  return absl::OkStatus();
}

absl::StatusOr<uint32_t> ProtoScrubber::ScanChildField(
    const google::protobuf::Type* type,
    const google::protobuf::Type* child_type,
    const google::protobuf::Field& field, uint32_t child_tag, int position,
    const bool is_checksum_enabled, google::protobuf::io::CodedInputStream* input_stream,
    std::vector<std::string>* path, std::vector<std::string>* parent_tags,
    std::vector<TagCopy>* tag_insert,
    uint32_t* scrubbed_fields_checksum) const {
  if (field.kind() == google::protobuf::Field::TYPE_ENUM) {
    // Special handling of Enum fields.
    return ScanEnumField(type, field, child_tag, position, input_stream, path,
                         parent_tags, tag_insert, scrubbed_fields_checksum,
                         is_checksum_enabled);
  }

  if (child_type == nullptr) {
return absl::Status(
    absl::StatusCode::kInvalidArgument,
    absl::StrCat("Cannot find the Type of field '", field.name(), "'."));
  }

  absl::StatusOr<uint32_t> child_result = 0;
  if (is_checksum_enabled) {
    parent_tags->push_back(absl::StrCat(child_tag));
  }
  if (!any_field_checkers_.empty() && child_type->name() == kAnyType) {
    child_result = ScanAnyField(field, child_tag, position, input_stream, path,
                                parent_tags, tag_insert,
                                scrubbed_fields_checksum, is_checksum_enabled);
  } else if (IsMap(field, *child_type)) {
    child_result = ScanMapField(*type, *child_type, field, child_tag,
                                input_stream, path, parent_tags, tag_insert,
                                scrubbed_fields_checksum, is_checksum_enabled);
  } else {
    child_result =
        ScanField(child_type, child_tag, input_stream, path, parent_tags,
                  tag_insert, scrubbed_fields_checksum, is_checksum_enabled);
  }
  if (is_checksum_enabled) {
    parent_tags->pop_back();
  }
  return child_result;
}

}  // namespace proto_processing_lib::proto_scrubber
