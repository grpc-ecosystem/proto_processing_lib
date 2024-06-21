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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "proto_processing_lib/interface_util.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/proto_scrubber_enums.h"
#include "google/protobuf/io/coded_stream.h"

namespace proto_processing_lib::proto_scrubber {

// A FieldMask based proto scrubber. The scrubber takes a serialized proto
// message and filters out any fields or sub-fields that do not satisfy all of
// the FieldChecker instances.
// Instances of this class are immutable, thus thread safe.
class ProtoScrubber : public ProtoScrubberInterface {
 public:
  // 'type' is a pointer to the google::protobuf::Type object of the proto
  // messages that this scrubber will be applied to. Caller should retain the
  // ownership of 'type' and makes sure it out-lives the ProtoScrubber instance.
  // 'type_finder' is a function that, given a URL to a protobuf type, finds the
  // pointer to its google::protobuf::Type object. 'field_checkers' is a list
  // of FieldChecker instances that will be used to check if a fields should be
  // included in the scrubbing result. 'scrubber_context' is information
  // describing when this scrubber is being invoked, and is used for monitoring
  // purposes. When 'field_check_only' is set to true, the scrubber only perform
  // field checks, i.e., an error is returned when a field is excluded.
  ProtoScrubber(const google::protobuf::Type* type,
                std::function<const google::protobuf::Type*(const std::string&)>
                    type_finder,
                const std::vector<const FieldCheckerInterface*>& field_checkers,
                ScrubberContext scrubber_context, bool field_check_only);

  ProtoScrubber(const google::protobuf::Type* type,
                std::function<const google::protobuf::Type*(const std::string&)>
                    type_finder,
                const std::vector<const FieldCheckerInterface*>& field_checkers,
                ScrubberContext scrubber_context);

  // This type is neither copyable nor movable.
  ProtoScrubber(const ProtoScrubber&) = delete;
  ProtoScrubber& operator=(const ProtoScrubber&) = delete;

  virtual ~ProtoScrubber() {}

  // Scrubs a serialized (as Cord) proto message. The result only contains
  // (sub-)fields that match one of the FieldMask paths. 'message' is changed
  // only when the result is OK. Otherwise, it will be kept intact. Caller
  // retains ownership to 'message'.
  virtual absl::Status Scrub(
      google::protobuf::field_extraction::MessageData* message) const override;

  // Scrubs the proto message same as Scrub, but also returns the checksum
  // for all scrubbed fields.
  virtual absl::StatusOr<uint32_t> ScrubWithChecksum(
      google::protobuf::field_extraction::MessageData* message) const override;

  using ProtoScrubberInterface::CheckField;
  // Checks if a field should be kept by calling field_checkers_ and returns the
  // aggregated result. If one of them returns EXCLUDE, the aggregated result is
  // EXCLUDE. If all of them return INCLUDE, the aggregated result is INCLUDE.
  // Otherwise, the aggregated result is PARTIAL. Caller retains the ownership
  // of 'field'. If the field is excluded, also update the excluded_by_filter.
  FieldCheckResults CheckField(const std::vector<std::string>& path,
                               const google::protobuf::Field* field,
                               FieldFilters* excluded_by_filter) const override;

  // An overload that allows passing the parent type.
  FieldCheckResults CheckField(const std::vector<std::string>& path,
                               const google::protobuf::Field* field,
                               const google::protobuf::Type* parent_type,
                               FieldFilters* excluded_by_filter) const override;

  // Add a FieldChecker instance that will be used to check if a fields should
  // be included in the scrubbing result.
  void AddFieldFilter(const FieldCheckerInterface* checker) override;

  const std::vector<const FieldCheckerInterface*>& field_filters() override {
    return field_checkers_;
  }

  void IncreaseUnknownFieldDroppedCount(
      absl::string_view type_name, int32_t field_number,
      ScrubberContext scrubber_context) const override;

  void IncreaseFieldDroppedCount(
      const std::string& type_name, int32_t field_number,
      const ScrubberContext& scrubber_context,
      const FieldFilters& excluded_by_filter) const override;

 private:
  // A struct that holds either tag number and length that should be inserted
  // into the scrubbing result (along with the scrubbed_message if it is not
  // empty), or the position and length of a segment of the input that should be
  // copied into the result.
  struct TagCopy {
    uint32_t tag;
    uint32_t length;
    int copy_start;
    uint32_t copy_length;
    std::unique_ptr<google::protobuf::field_extraction::CordMessageData>
        scrubbed_message;
  };

  // Private constructor that exposes the initial message depth.
  // Useful to support stack overflow protection when recursively constructing
  // a ProtoScrubber for sub-messages.
  ProtoScrubber(const google::protobuf::Type* type,
                std::function<const google::protobuf::Type*(const std::string&)>
                    type_finder,
                const std::vector<const FieldCheckerInterface*>& field_checkers,
                ScrubberContext scrubber_context, bool field_check_only,
                const int initial_message_depth);

  // Scans through the field at the current position of 'input_stream' and
  // yields a list of TagCopy instances recording the tags and lengths that
  // should be inserted into the scrubbing result or the starting positions and
  // lengths of segments in input_stream that should be copied into the result.
  // For non-root messages, "input_stream" should be positioned right after its
  // tag. For root message, "input_stream" should be positioned at the very
  // beginning. 'type' is the Type of the message that is currently being
  // scanned. 'tag' is the tag of the current field and 0 should be used for the
  // root message (at the very beginning of the 'input_stream'). 'path' is the
  // path (as a list of field numbers) that leads from the root message to the
  // current message. Returns the number of bytes represented by this field
  // after filtering and including the number of bytes in the tag and length
  // varints. Caller retains ownership of all pointer parameters.
  absl::StatusOr<uint32_t> ScanField(const google::protobuf::Type* type,
                                     uint32_t tag,
                                     google::protobuf::io::CodedInputStream* input_stream,
                                     std::vector<std::string>* path,
                                     std::vector<std::string>* parent_tags,
                                     std::vector<TagCopy>* tag_insert,
                                     uint32_t* scrubbed_fields_checksum,
                                     const bool is_checksum_enabled) const;

  // Loops through all entries of the map field at the current position of
  // 'input_stream' and yields a list of TagCopy instances recording which
  // entries (and maybe children of their value fields) should be included in
  // the scrubbing result.
  absl::StatusOr<uint32_t> ScanMapField(
      const google::protobuf::Type& parent_type,
      const google::protobuf::Type& type, const google::protobuf::Field& field,
      uint32_t tag, google::protobuf::io::CodedInputStream* input_stream,
      std::vector<std::string>* path, std::vector<std::string>* parent_tags,
      std::vector<TagCopy>* tag_insert, uint32_t* scrubbed_fields_checksum,
      const bool is_checksum_enabled) const;

  // Scans an enum field and checks if the field should be included in the
  // result based on its value. Note that the field could also be a packed
  // repeated field.
  absl::StatusOr<uint32_t> ScanEnumField(
      const google::protobuf::Type* type, const google::protobuf::Field& field,
      uint32_t tag, int tag_position,
      google::protobuf::io::CodedInputStream* input_stream,
      std::vector<std::string>* path, std::vector<std::string>* parent_tags,
      std::vector<TagCopy>* tag_insert, uint32_t* scrubbed_fields_checksum,
      const bool is_checksum_enabled) const;

  // Scans the entry of a map field at the current position of 'input_stream'
  // and checks if the entry should be included in the scrubbing result based on
  // it's the value of its key. If the entry should be (partially) included, a
  // list of TagCopy instances will be added to 'tag_insert' for the key and
  // value fields of the entry.
  absl::StatusOr<uint32_t> ScanMapEntry(
      const google::protobuf::Type& parent_type,
      const google::protobuf::Type& map_type,
      const google::protobuf::Field& map_field,
      const google::protobuf::Field& key_field,
      const google::protobuf::Field& value_field,
      const google::protobuf::Type* value_type,
      google::protobuf::io::CodedInputStream* input_stream,
      std::vector<std::string>* path, std::vector<std::string>* parent_tags,
      std::vector<TagCopy>* tag_insert, uint32_t* scrubbed_fields_checksum,
      const bool is_checksum_enabled) const;

  // Scans a "Any" field and checks if the field should be included in the
  // result based on its value.
  absl::StatusOr<uint32_t> ScanAnyField(
      const google::protobuf::Field& field, uint32_t tag_of_any_field,
      int tag_position, google::protobuf::io::CodedInputStream* input_stream,
      const std::vector<std::string>* path,
      std::vector<std::string>* parent_tags, std::vector<TagCopy>* tag_insert,
      uint32_t* scrubbed_fields_checksum, const bool is_checksum_enabled) const;

  // For a child field, determines which of the other helps to call.
  // Handles children enums, maps, Anys, maps, or messages.
  absl::StatusOr<uint32_t> ScanChildField(
      const google::protobuf::Type* type,
      const google::protobuf::Type* child_type,
      const google::protobuf::Field& field, uint32_t child_tag, int position,
      bool is_checksum_enabled, google::protobuf::io::CodedInputStream* input_stream,
      std::vector<std::string>* path, std::vector<std::string>* parent_tags,
      std::vector<TagCopy>* tag_insert,
      uint32_t* scrubbed_fields_checksum) const;

  // Reads a map key field value according to Field spec in 'field' and returns
  // the read value as string. This only works for primitive datatypes that can
  // be used as map keys.
  static const std::string ReadMapKeyAsString(
      const google::protobuf::Field& field,
      google::protobuf::io::CodedInputStream* input_stream);

  // Checks if the enum value at the current position of input_stream should be
  // kept in the result. After calling this method, the position of
  // 'input_stream' will be advanced pass the current enum value.
  FieldCheckResults CheckEnumValue(const google::protobuf::Field* field,
                                   std::vector<std::string>* path,
                                   google::protobuf::io::CodedInputStream* input_stream,
                                   std::vector<std::string>* parent_tags,
                                   uint32_t* scrubbed_fields_checksum,
                                   FieldFilters* excluded_by_filter,
                                   const bool is_checksum_enabled) const;

  absl::Status CalculateScrubbedFieldsChecksum(
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field* field,
      std::vector<std::string>* parent_tags, uint32_t* tag, int* position,
      uint32_t* scrubbed_fields_checksum) const;

  // Returns a pointer to the Field based on its field number in 'type'. Returns
  // nullptr if not found.
  const google::protobuf::Field* FindField(const google::protobuf::Type* type,
                                           int32_t field_number) const;

  // Scrubs the proto message same as Scrub, but also returns the checksum
  // for all scrubbed fields if checksum is enabled, otherwise, returns 0.
  absl::StatusOr<uint32_t> Scrub(
      google::protobuf::field_extraction::MessageData* message,
      std::vector<std::string>* parent_tags, bool is_checksum_enabled) const;

  // Based on tag_copy, either copies segments of the input to the output when
  // copy_length > 0, or inserts the tag+length to the output. Caller
  // retains ownership to 'input' and 'output'.
  absl::Status CopyAndInsertTag(
      const std::vector<TagCopy>& tag_copy,
      google::protobuf::field_extraction::MessageData* input,
      absl::Cord* output) const;

  // Returns a TagCopy instance that will be used to copy a segment represented
  // by the starting position and length of the input to the scrubbing result.
  TagCopy MakeTagCopyForCopying(int start_position,
                                uint32_t copy_length) const {
    return {0, 0, start_position, copy_length, nullptr};
  }

  // Returns a TagCopy instance that will be used to insert the tag and length
  // of a field to the scrubbing result.
  TagCopy MakeTagCopyForInserting(uint32_t tag, uint32_t length) const {
    return {tag, length, 0, 0, nullptr};
  }

  // If copy_start is at the end of the last TagCopy of tag_insert, add
  // copy_length to that TagCopy's copy_length. Otherwise adds a new TagCopy
  // with copy_start and copy_length to tag_insert.
  bool AddTagCopyOrRollToPrevious(int copy_start, uint32_t copy_length,
                                  std::vector<TagCopy>* tag_insert) const;

  // Returns a pointer to the proto Type of a Field.
  const google::protobuf::Type* GetFieldType(
      const google::protobuf::Field* field) const;

  // The Type of the proto messages that the scrubber applies to.
  const google::protobuf::Type* type_;
  // A function object that finds the pointer to a Type object given its URL.
  std::function<const google::protobuf::Type*(const std::string&)> type_finder_;
  // A list of FieldChecker instances. Their aggregated result determines if a
  // field should be kept after scrubbing.
  std::vector<const FieldCheckerInterface*> field_checkers_;
  // A list of FieldChecker instances that supports checking fields of "Any"
  // type.
  std::vector<const FieldCheckerInterface*> any_field_checkers_;

  // A lock only used for locking writing `field_type_map_`.
  mutable absl::Mutex lock_;

  // A map between a Field pointer and its type.
  //
  // Guarded by `lock_`, since ProtoScrubber can be initialized at per method
  // level, which potentially makes the functions in ProtoScrubber be executed
  // concurrently.
  mutable absl::flat_hash_map<const google::protobuf::Field*,
                              const google::protobuf::Type*>
      field_type_map_ ABSL_GUARDED_BY(lock_);
  // Indicates in which context the scrubber is operating. Used for monitoring
  // purposes.
  ScrubberContext scrubber_context_;
  // When set to true, the scrubber only perform field checks, i.e., an error is
  // returned when a field is excluded.
  bool field_check_only_;

  // Keeps track of the starting messaged depth for this instance of the
  // scrubber.
  const int initial_message_depth_;
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_H_
