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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_INTERFACE_UTIL_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_INTERFACE_UTIL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/field_mask_node.h"
#include "proto_processing_lib/proto_scrubber/proto_scrubber_enums.h"

namespace proto_processing_lib {

// FieldMaskTreeInterface is an abstract class. Proto Processing Library
// provides its own implementation.
class FieldMaskTreeInterface : public proto_scrubber::FieldCheckerInterface {
 public:
  // Adds a list of FieldMask paths to the tree if no paths have been added. The
  // resulting tree will be the union of "paths" (i.e. a paths to a parent field
  // override paths to its child fields). For subsequent calls, the resulting
  // tree represents an intersection between 'paths' and the previously added
  // paths.
  // Returns OK if all "paths" are legal, otherwise returns INVALID_ARGUMENT and
  // the tree will be left in a broken state. Calling this method again on a
  // broken FieldMaskTree will yield an error of FAILED_PRECONDITION.
  virtual absl::Status AddOrIntersectFieldPaths(
      const std::vector<std::string>& paths) = 0;

  // Returns the status of the tree.
  virtual absl::Status status() = 0;

  // Returns root node of the tree.
  virtual const proto_scrubber::FieldMaskNode* root() const = 0;
};

// CloudAuditLogFieldCheckerInterface is an abstract class. Proto Processing
// Library provides its own implementation.
class CloudAuditLogFieldCheckerInterface
    : public proto_scrubber::FieldCheckerInterface {
 public:
  // Adds a list of auditing field paths to this field checker. For example,
  // {"a.b", "c"} indicates that the root message can include only fields "a"
  // and "c", and the message "a" can include only field "b".
  virtual absl::Status AddOrIntersectFieldPaths(
      const std::vector<std::string>& paths) = 0;
};

// ProtoScrubberInterface is an abstract class. Proto Processing Library
// provides its own implementation.
class ProtoScrubberInterface {
 public:
  virtual ~ProtoScrubberInterface() {}

  // Scrubs a serialized (as Cord) proto message. The result only contains
  // (sub-)fields that match one of the FieldMask paths. 'message' is changed
  // only when the result is OK. Otherwise, it will be kept intact. Caller
  // retains ownership to 'message'.
  virtual absl::Status Scrub(
      google::protobuf::field_extraction::MessageData* message) const = 0;

  // Scrubs the proto message same as Scrub, but also returns the checksum
  // for all scrubbed fields.
  virtual absl::StatusOr<uint32_t> ScrubWithChecksum(
      google::protobuf::field_extraction::MessageData* message) const = 0;

  // Checks if a field should be kept by calling field_checkers_ and returns the
  // aggregated result. If one of them returns EXCLUDE, the aggregated result is
  // EXCLUDE. If all of them return INCLUDE, the aggregated result is INCLUDE.
  // Otherwise, the aggregated result is PARTIAL. Caller retains the ownership
  // of 'field'. If the field is excluded, also update the excluded_by_filter.
  virtual proto_scrubber::FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field,
      proto_scrubber::FieldFilters* excluded_by_filter) const = 0;

  // An overload that allows passing the parent type.
  virtual proto_scrubber::FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field,
      const google::protobuf::Type* parent_type,
      proto_scrubber::FieldFilters* excluded_by_filter) const = 0;

  // Add a FieldChecker instance that will be used to check if a fields should
  // be included in the scrubbing result.
  virtual void AddFieldFilter(
      const proto_scrubber::FieldCheckerInterface* checker) = 0;

  virtual const std::vector<const proto_scrubber::FieldCheckerInterface*>&
  field_filters() = 0;

  virtual void IncreaseFieldDroppedCount(
      const std::string& type_name, int32_t field_number,
      const proto_scrubber::ScrubberContext& scrubber_context,
      const proto_scrubber::FieldFilters& excluded_by_filter) const = 0;

  virtual void IncreaseUnknownFieldDroppedCount(
      absl::string_view type_name, int32_t field_number,
      proto_scrubber::ScrubberContext scrubber_context) const = 0;
};

}  // namespace proto_processing_lib

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_INTERFACE_UTIL_H_
