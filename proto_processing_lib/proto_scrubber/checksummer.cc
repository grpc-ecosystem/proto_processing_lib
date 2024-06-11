// Copyright 2024 Google LLC
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

#include "proto_processing_lib/proto_scrubber/checksummer.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/call_once.h"
#include "absl/container/flat_hash_map.h"
#include "absl/crc/crc32c.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_log.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "src/google/protobuf/util/converter/constants.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"
#include "google/protobuf/wire_format_lite.h"
#undef RETURN_IF_ERROR
#include "ocpdiag/core/compat/status_macros.h"

namespace proto_processing_lib::proto_scrubber {

using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CordInputStream;

absl::once_flag once;

Checksummer::Checksummer(const int max_depth) : max_depth_(max_depth) {
  absl::call_once(once, [&]() { InitRendererMap(); });
}

namespace {

using ::google::protobuf::internal::WireFormatLite;
using ::google::protobuf::io::CordInputStream;

// Returns a pointer to the Field based on its field number in 'type'. Returns
// nullptr if not found.
const google::protobuf::Field* FindAndVerifyField(
    const google::protobuf::Type& type, uint32_t tag);

// Returns true if the field is packable.
bool IsPackable(const google::protobuf::Field& field);

// PreCheck for CalculateChecksum.
absl::Status CalculateChecksumPreCheck(
    google::protobuf::field_extraction::MessageData* data_buffer,
    absl::Cord* data, const google::protobuf::Type* type,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder);

static absl::flat_hash_map<std::string, Checksummer::TypeRenderer>* renderers;

}  // namespace

const Checksummer* Checksummer::GetDefault() {
  static const Checksummer* default_checksummer = new Checksummer(
      -1
  );
  return default_checksummer;
}

const Checksummer* Checksummer::GetDefault(const int max_depth) {
  static const Checksummer* default_checksummer = new Checksummer(max_depth);
  return default_checksummer;
}

void Checksummer::InitRendererMap() {
  renderers = new absl::flat_hash_map<std::string, Checksummer::TypeRenderer>();
  (*renderers)["type.googleapis.com/google.protobuf.Timestamp"] =
      &Checksummer::RenderTimestampAndDuration;
  (*renderers)["type.googleapis.com/google.protobuf.Duration"] =
      &Checksummer::RenderTimestampAndDuration;
  (*renderers)["type.googleapis.com/google.protobuf.DoubleValue"] =
      &Checksummer::RenderWrapperValue;
  (*renderers)["type.googleapis.com/google.protobuf.FloatValue"] =
      &Checksummer::RenderWrapperValue;
  (*renderers)["type.googleapis.com/google.protobuf.Int64Value"] =
      &Checksummer::RenderWrapperValue;
  (*renderers)["type.googleapis.com/google.protobuf.UInt64Value"] =
      &Checksummer::RenderWrapperValue;
  (*renderers)["type.googleapis.com/google.protobuf.Int32Value"] =
      &Checksummer::RenderWrapperValue;
  (*renderers)["type.googleapis.com/google.protobuf.UInt32Value"] =
      &Checksummer::RenderWrapperValue;
  (*renderers)["type.googleapis.com/google.protobuf.BoolValue"] =
      &Checksummer::RenderWrapperValue;
}

absl::StatusOr<uint32_t> Checksummer::CalculateChecksum(
    google::protobuf::field_extraction::MessageData* data_buffer,
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder) const {
  std::vector<std::string> parent_tags;
  return CalculateChecksum(data_buffer, type, std::move(type_finder),
                           &parent_tags);
}

absl::StatusOr<uint32_t> Checksummer::CalculateChecksum(
    google::protobuf::field_extraction::MessageData* data_buffer,
    const google::protobuf::Type* type,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    std::vector<std::string>* parent_tags) const {
  if (!data_buffer || data_buffer->IsEmpty()) {
    return 0;
  }
  auto status = CalculateChecksumPreCheck(data_buffer, /*data=*/nullptr, type,
                                          type_finder);
  if (!status.ok()) {
    return status;
  }

  auto input_stream_wrapper = data_buffer->CreateCodedInputStreamWrapper();

  uint32_t checksum = 0;
  RETURN_IF_ERROR(ScanMessage(type_finder, &input_stream_wrapper->Get(), *type,
                              parent_tags, &checksum));
  ABSL_LOG(INFO) << "Calculate Checksum succeed, got checksum:" << checksum;
  return checksum;
}

absl::StatusOr<uint32_t> Checksummer::CalculateChecksum(
    absl::Cord* data, const google::protobuf::Type* type,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    std::vector<std::string>* parent_tags) const {
  if (!data || data->empty()) {
    return 0;
  }
  auto status = CalculateChecksumPreCheck(/*data_buffer=*/nullptr, data, type,
                                          type_finder);
  if (!status.ok()) {
    return status;
  }
  CordInputStream cin_stream(data);
  CodedInputStream in_stream(&cin_stream);

  uint32_t checksum = 0;
  RETURN_IF_ERROR(
      ScanMessage(type_finder, &in_stream, *type, parent_tags, &checksum));
  ABSL_LOG(INFO) << "Calculate Checksum succeed, got checksum:" << checksum;
  return checksum;
}

absl::StatusOr<uint32_t> Checksummer::CalculateChecksum(
    absl::Cord* data, const google::protobuf::Type* type,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder) const {
  std::vector<std::string> parent_tags;
  return CalculateChecksum(data, type, std::move(type_finder), &parent_tags);
}

absl::StatusOr<uint32_t> Checksummer::CalculateSingleFieldChecksum(
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field* field, std::vector<std::string>* parent_tags,
    uint32_t* tag, int32_t* position) const {
  // No need to calculate checksum if any of these are null.
  if (!input_stream || !field || !parent_tags || !tag || !position) {
    return 0;
  }
  uint32_t checksum = 0;
  if (field->cardinality() == google::protobuf::Field::CARDINALITY_REPEATED) {
    ASSIGN_OR_RETURN(
        *tag, ScanRepeatedField(type_finder, input_stream, *field, parent_tags,
                                tag, position, &checksum));
  } else {
    RETURN_IF_ERROR(ScanNonRepeatedField(type_finder, input_stream, *field,
                                         parent_tags, tag, &checksum));
    *position = input_stream->CurrentPosition();
    *tag = input_stream->ReadTag();
  }
  return checksum;
}

absl::Status Checksummer::ScanMessage(
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Type& type, std::vector<std::string>* parent_tags,
    uint32_t* checksum) const {
  const google::protobuf::Field* field = nullptr;
  uint32_t tag = input_stream->ReadTag(), last_tag = 0;
  while (tag != 0) {
    // When tag is changed, need to call FindAndVerifyField to get the new
    // field.
    if (tag != last_tag) {
      last_tag = tag;
      field = FindAndVerifyField(type, tag);
    }
    if (field == nullptr) {
      WireFormatLite::SkipField(input_stream, tag);
      tag = input_stream->ReadTag();
      continue;
    }

    if (field->cardinality() == google::protobuf::Field::CARDINALITY_REPEATED) {
      // Treats map same as repeated field, since we only need to get
      // the key and value to calculate the checksum.
      int32_t position = 0;
      ASSIGN_OR_RETURN(
          tag, ScanRepeatedField(type_finder, input_stream, *field, parent_tags,
                                 &tag, &position, checksum));
    } else {
      RETURN_IF_ERROR(ScanNonRepeatedField(type_finder, input_stream, *field,
                                           parent_tags, &tag, checksum));
      tag = input_stream->ReadTag();
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<uint32_t> Checksummer::ScanRepeatedField(
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field& field, std::vector<std::string>* parent_tags,
    uint32_t* tag, int32_t* position, uint32_t* checksum) const {
  uint32_t tag_to_return = 0;
  // If the message nested depth exceeds the limit, skip calculating the child
  // checksum.
  if (max_depth_ >= 0 && parent_tags->size() > max_depth_) {
    return tag_to_return;
  }

  if (IsPackable(field) &&
      *tag == WireFormatLite::MakeTag(
                  field.number(), WireFormatLite::WIRETYPE_LENGTH_DELIMITED)) {
    // For Packed repeated primitive field, use its unpacked tag number instead
    // for checksum calculation. For more information about packed field, see
    // https://developers.google.com/protocol-buffers/docs/encoding.html#packed
    *tag = *tag - (WireFormatLite::WIRETYPE_LENGTH_DELIMITED -
                   WireFormatLite::WireTypeForFieldType(
                       static_cast<WireFormatLite::FieldType>(field.kind())));

    RETURN_IF_ERROR(ScanPacked(type_finder, input_stream, field, parent_tags,
                               tag, checksum));
    *position = input_stream->CurrentPosition();
    tag_to_return = input_stream->ReadTag();
  } else {
    do {
      RETURN_IF_ERROR(ScanNonRepeatedField(type_finder, input_stream, field,
                                           parent_tags, tag, checksum));
      *position = input_stream->CurrentPosition();
    } while ((tag_to_return = input_stream->ReadTag()) == *tag);
  }
  return tag_to_return;
}

absl::Status Checksummer::ScanNonRepeatedField(
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field& field, std::vector<std::string>* parent_tags,
    uint32_t* tag, uint32_t* checksum) const {
  // If the message nested depth exceeds the limit, skip calculating the child
  // checksum.
  if (max_depth_ >= 0 && parent_tags->size() > max_depth_) {
    return absl::OkStatus();
  }

  if (field.kind() != google::protobuf::Field::TYPE_MESSAGE) {
    ScanPrimitiveField(input_stream, field, parent_tags, tag, checksum,
                       /*ignore_zero_value=*/false);
    return absl::OkStatus();
  }

  uint32_t buffer32 = 0;
  input_stream->ReadVarint32(&buffer32);
  int old_limit = input_stream->PushLimit(buffer32);

  const TypeRenderer* type_renderer = FindTypeRenderer(field.type_url());
  if (type_renderer != nullptr) {
    parent_tags->push_back(absl::StrCat(*tag));
    (*type_renderer)(this, type_finder, input_stream, field, parent_tags,
                     checksum);
    parent_tags->pop_back();

    input_stream->PopLimit(old_limit);
    return absl::OkStatus();
  }

  const google::protobuf::Type* child_type = type_finder(field.type_url());
  if (child_type == nullptr) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrCat("fail to find field type for field ", field.name()));
  }

  parent_tags->push_back(absl::StrCat(*tag));
  absl::Status child_result;
  if (child_type->name() == google::protobuf::util::converter::kAnyType) {
    child_result = ScanAnyMessage(type_finder, input_stream, *child_type,
                                  parent_tags, checksum);
  } else {
    child_result = ScanMessage(type_finder, input_stream, *child_type,
                               parent_tags, checksum);
  }
  parent_tags->pop_back();
  if (!child_result.ok()) {
    return child_result;
  }
  input_stream->PopLimit(old_limit);
  return absl::OkStatus();
}

absl::Status Checksummer::ScanAnyMessage(
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Type& type, std::vector<std::string>* parent_tags,
    uint32_t* checksum) const {
  // An Any is of the form { string type_url = 1; bytes value = 2; }
  std::string value;
  std::string type_url;

  const google::protobuf::Field* child_field = nullptr;
  uint32_t child_tag, type_url_tag, value_tag;

  for (child_tag = input_stream->ReadTag(); child_tag != 0;
       child_tag = input_stream->ReadTag()) {
    child_field = FindAndVerifyField(type, child_tag);
    if (child_field == nullptr) {
      WireFormatLite::SkipField(input_stream, child_tag);
      continue;
    }
    if (child_field->number() == 1) {
      // Read type_url
      uint32_t type_url_size;
      input_stream->ReadVarint32(&type_url_size);
      input_stream->ReadString(&type_url, type_url_size);
      type_url_tag = child_tag;
    } else if (child_field->number() == 2) {
      // Read value
      uint32_t value_size;
      input_stream->ReadVarint32(&value_size);
      input_stream->ReadString(&value, value_size);
      value_tag = child_tag;
    }
  }

  if (type_url.empty()) {
    return absl::Status(absl::StatusCode::kInternal,
                        "Invalid Any, the type_url is missing.");
  }
  parent_tags->push_back(absl::StrCat(type_url_tag));
  AddToChecksum(absl::StrJoin(*parent_tags, "."), type_url, checksum);
  parent_tags->pop_back();

  if (value.empty()) {
    return absl::OkStatus();
  }

  const google::protobuf::Type* nested_type = type_finder(type_url);
  if (nested_type == nullptr) {
    return absl::Status(
        absl::StatusCode::kInvalidArgument,
        absl::StrCat("fail to find field type for type ", type_url));
  }
  google::protobuf::io::ArrayInputStream zero_copy_stream(value.data(), value.size());
  google::protobuf::io::CodedInputStream in_stream(&zero_copy_stream);
  parent_tags->push_back(absl::StrCat(value_tag));
  absl::Status status =
      ScanMessage(type_finder, &in_stream, *nested_type, parent_tags, checksum);
  parent_tags->pop_back();
  return status;
}

absl::Status Checksummer::ScanPacked(
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field& field, std::vector<std::string>* parent_tags,
    uint32_t* tag, uint32_t* checksum) const {
  uint32_t length;
  input_stream->ReadVarint32(&length);
  int old_limit = input_stream->PushLimit(length);
  while (input_stream->BytesUntilLimit() > 0) {
    RETURN_IF_ERROR(ScanNonRepeatedField(type_finder, input_stream, field,
                                         parent_tags, tag, checksum));
  }
  input_stream->PopLimit(old_limit);
  return absl::Status();
}

// static
Checksummer::TypeRenderer* Checksummer::FindTypeRenderer(
    absl::string_view type_url) {
  auto it = renderers->find(type_url);
  if (it == renderers->end()) return nullptr;
  return &(it->second);
}

void Checksummer::RenderTimestampAndDuration(
    const Checksummer* checksummer,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field& field, std::vector<std::string>* parent_tags,
    uint32_t* checksum) {
  uint64_t seconds = 0;
  uint32_t nanos = 0;
  uint32_t child_tag = input_stream->ReadTag();

  while (child_tag != 0) {
    const int32_t field_number = WireFormatLite::GetTagFieldNumber(child_tag);
    switch (field_number) {
      case 1:
        // read seconds
        input_stream->ReadVarint64(&seconds);
        if (seconds != 0) {
          parent_tags->push_back(absl::StrCat(child_tag));
          checksummer->AddToChecksum(
              absl::StrJoin(*parent_tags, "."),
              absl::StrCat(absl::bit_cast<uint64_t>(seconds)), checksum);
          parent_tags->pop_back();
        }
        break;
      case 2:
        // read nanos
        input_stream->ReadVarint32(&nanos);
        if (nanos != 0) {
          parent_tags->push_back(absl::StrCat(child_tag));
          checksummer->AddToChecksum(
              absl::StrJoin(*parent_tags, "."),
              absl::StrCat(absl::bit_cast<int32_t>(nanos)), checksum);
          parent_tags->pop_back();
        }
        break;
    }
    child_tag = input_stream->ReadTag();
  }
}

// static
void Checksummer::RenderWrapperValue(
    const Checksummer* checksummer,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder,
    google::protobuf::io::CodedInputStream* input_stream,
    const google::protobuf::Field& field, std::vector<std::string>* parent_tags,
    uint32_t* checksum) {
  const google::protobuf::Type* child_type = type_finder(field.type_url());
  if (child_type == nullptr) {
    LOG(WARNING) << "fail to find field type for field: " << field.name();
    return;
  }

  uint32_t child_tag = input_stream->ReadTag();
  if (child_tag != 0) {
    const google::protobuf::Field* child_field =
        FindAndVerifyField(*child_type, child_tag);
    if (child_field == nullptr) {
      LOG(WARNING) << "fail to find tag '" << child_tag
                   << "' for type: " << child_type->name();
      WireFormatLite::SkipField(input_stream, child_tag);
      return;
    }

    checksummer->ScanPrimitiveField(input_stream, *child_field, parent_tags,
                                    &child_tag, checksum, true);
  }
}

void Checksummer::ScanPrimitiveField(google::protobuf::io::CodedInputStream* input_stream,
                                     const google::protobuf::Field& field,
                                     std::vector<std::string>* parent_tags,
                                     uint32_t* tag, uint32_t* checksum,
                                     bool ignore_zero_value) const {
  uint64_t buffer64 = 0;
  uint32_t buffer32 = 0;
  std::string result;
  switch (field.kind()) {
    case google::protobuf::Field::TYPE_UNKNOWN:
      // Skip unknown field.
      WireFormatLite::SkipField(input_stream, *tag);
      break;
    case google::protobuf::Field::TYPE_DOUBLE:
      input_stream->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<double>(buffer64));
      break;
    case google::protobuf::Field::TYPE_FLOAT:
      input_stream->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<float>(buffer32));
      break;
    case google::protobuf::Field::TYPE_INT64:
      input_stream->ReadVarint64(&buffer64);
      result = absl::StrCat(absl::bit_cast<int64_t>(buffer64));
      break;
    case google::protobuf::Field::TYPE_UINT64:
      input_stream->ReadVarint64(&buffer64);
      result = absl::StrCat(absl::bit_cast<uint64_t>(buffer64));
      break;
    case google::protobuf::Field::TYPE_INT32:
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<int32_t>(buffer32));
      break;
    case google::protobuf::Field::TYPE_FIXED64:
      input_stream->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<uint64_t>(buffer64));
      break;
    case google::protobuf::Field::TYPE_FIXED32:
      input_stream->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    case google::protobuf::Field::TYPE_BOOL:
      input_stream->ReadVarint64(&buffer64);
      if (ignore_zero_value && buffer64 == 0) {
        return;
      }
      result = buffer64 != 0 ? "true" : "false";
      break;
    case google::protobuf::Field::TYPE_STRING:
      input_stream->ReadVarint32(&buffer32);  // string size.
      input_stream->ReadString(&result, buffer32);
      break;
    case google::protobuf::Field::TYPE_GROUP:
      WireFormatLite::SkipField(input_stream, *tag);
      break;
    case google::protobuf::Field::TYPE_BYTES: {
      absl::Cord cord;
      input_stream->ReadVarint32(&buffer32);
      input_stream->ReadCord(&cord, buffer32);
      result = std::string(cord);
      break;
    }
    case google::protobuf::Field::TYPE_UINT32:
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    case google::protobuf::Field::TYPE_ENUM:
      // Just use uint32 to represent ENUM.
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(absl::bit_cast<uint32_t>(buffer32));
      break;
    case google::protobuf::Field::TYPE_SINT32:
      input_stream->ReadVarint32(&buffer32);
      result = absl::StrCat(WireFormatLite::ZigZagDecode32(buffer32));
      break;
    case google::protobuf::Field::TYPE_SINT64:
      input_stream->ReadVarint64(&buffer64);
      result = absl::StrCat(WireFormatLite::ZigZagDecode64(buffer64));
      break;
    case google::protobuf::Field::TYPE_SFIXED32:
      input_stream->ReadLittleEndian32(&buffer32);
      result = absl::StrCat(absl::bit_cast<int32_t>(buffer32));
      break;
    case google::protobuf::Field::TYPE_SFIXED64:
      input_stream->ReadLittleEndian64(&buffer64);
      result = absl::StrCat(absl::bit_cast<int64_t>(buffer64));
      break;
    default:
      break;
  }
  if (ignore_zero_value && result == "0") {
    return;
  }

  parent_tags->push_back(absl::StrCat(*tag));
  AddToChecksum(absl::StrJoin(*parent_tags, "."), result, checksum);
  parent_tags->pop_back();
}

void Checksummer::AddToChecksum(absl::string_view tags, absl::string_view value,
                                uint32_t* checksum) const {
  if (tags.empty() || value.empty()) {
    return;
  }
  uint32_t new_checksum = static_cast<uint32_t>(absl::ComputeCrc32c(tags));
  new_checksum = static_cast<uint32_t>(
      absl::ExtendCrc32c(absl::crc32c_t{new_checksum}, value));
  *checksum = *checksum ^ new_checksum;
}

namespace {
const google::protobuf::Field* FindField(const google::protobuf::Type& type,
                                         uint32_t tag) {
  const int32_t field_number = WireFormatLite::GetTagFieldNumber(tag);
  for (const google::protobuf::Field& field : type.fields()) {
    if (field.number() == field_number) {
      return &field;
    }
  }
  return nullptr;
}

const google::protobuf::Field* FindAndVerifyField(
    const google::protobuf::Type& type, uint32_t tag) {
  const google::protobuf::Field* field = FindField(type, tag);
  // Verify if the field corresponds to the wire type in tag.
  // If there is any discrepancy, mark the field as not found.
  if (field == nullptr) return nullptr;

  WireFormatLite::WireType expected_type = WireFormatLite::WireTypeForFieldType(
      static_cast<WireFormatLite::FieldType>(field->kind()));
  WireFormatLite::WireType actual_type = WireFormatLite::GetTagWireType(tag);
  if (actual_type != expected_type &&
      (!IsPackable(*field) ||
       actual_type != WireFormatLite::WIRETYPE_LENGTH_DELIMITED)) {
    field = nullptr;
  }
  return field;
}

bool IsPackable(const google::protobuf::Field& field) {
  return field.cardinality() == google::protobuf::Field::CARDINALITY_REPEATED &&
         google::protobuf::FieldDescriptor::IsTypePackable(
             static_cast<google::protobuf::FieldDescriptor::Type>(field.kind()));
}

absl::Status CalculateChecksumPreCheck(
    google::protobuf::field_extraction::MessageData* data_buffer,
    absl::Cord* data, const google::protobuf::Type* type,
    const std::function<const google::protobuf::Type*(const std::string&)>&
        type_finder) {
  if (!type) {
    return absl::InternalError("Unexpected nullptr for message type.");
  }
  if (!type_finder) {
    return absl::InternalError("Unexpected nullptr for type_finder.");
  }

  return absl::OkStatus();
}

}  // namespace

}  // namespace proto_processing_lib::proto_scrubber
