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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_PATH_CHECKER_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_PATH_CHECKER_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/status/status.h"
#include "proto_processing_lib/interface_util.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"

namespace proto_processing_lib::proto_scrubber {

// This class wraps FieldMaskTreeInterface and provides CheckField()
// implementation used for ProtoScrubber which will be used to scrub API request
// and response proto for Cloud Audit Logging purpose.
//
// The implementation does not support field mask path with map keys (e.g.
// "message.primitive_map[\"123\"].message_embedded") because the field mask
// paths generated for Cloud Audit Logging will not contain map keys.
class FieldMaskPathChecker
    : public proto_processing_lib::FieldMaskPathCheckerInterface {
 public:
  // See FieldMaskTreeInterface class for description of the parameters.
  FieldMaskPathChecker(
      const google::protobuf::Type* type,
      std::function<const google::protobuf::Type*(const std::string&)>
          type_finder);
  FieldMaskPathChecker(const FieldMaskPathChecker&) = delete;
  FieldMaskPathChecker& operator=(const FieldMaskPathChecker&) =
      delete;
  // Adds a list of auditing field paths to this field checker. For example,
  // {"a.b", "c"} indicates that the root message can include only fields "a"
  // and "c", and the message "a" can include only field "b".
  absl::Status AddOrIntersectFieldPaths(
      const std::vector<std::string>& paths) override;

  using FieldMaskPathCheckerInterface::CheckField;
  // Decides whether the given field path is included, partially included, or
  // excluded.
  //
  // If a field is a cyclic message, the cycle need only be represented once
  // in the paths given to AddOrIntersectFieldPaths. It will be applied to an
  // arbitrary number of nesting levels. If map key is included in path, the
  // result is undefined.
  FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const override;
  bool SupportAny() const override { return false; }
  FieldCheckResults CheckType(
      const google::protobuf::Type* /*type*/) const override {
    return FieldCheckResults::kInclude;
  }
  FieldFilters FilterName() const override {
    return FieldFilters::CloudAuditFieldFilter;
  }

 private:
  std::unique_ptr<FieldMaskTreeInterface> field_mask_tree_;
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_PATH_CHECKER_H_
