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

#include "proto_processing_lib/proto_scrubber/field_mask_tree.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/rpc/error_details.pb.h"
#include "absl/base/attributes.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_log.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/field_mask_node.h"

using ::google::rpc::BadRequest;

namespace proto_processing_lib::proto_scrubber {

ABSL_CONST_INIT const char* const kWildcard = "*";

FieldMaskTree::FieldMaskTree(
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder)
    : root_(nullptr),
      type_(type),
      type_finder_(std::move(type_finder)),
      status_(::absl::OkStatus()) {}

absl::Status FieldMaskTree::AddOrIntersectFieldPaths(
    const std::vector<std::string>& paths) {
  if (!status_.ok()) {
    ABSL_LOG(FATAL) << "The FieldMask tree is broken. Error: " << status_;
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "The FieldMask tree is not in a OK status.");
  }

  // Creates a new root node based on 'paths'.
  auto node = std::make_unique<FieldMaskNode>(
      type_, /*is_leaf=*/paths.empty(), /*is_map=*/false,
      /*all_keys_included=*/false, type_finder_);
  for (const std::string& path : paths) {
    const std::vector<std::string> field_path =
        absl::StrSplit(path, '.', absl::SkipEmpty());
    const absl::Status add_result =
        node->InsertField(field_path.begin(), field_path.end());
    if (!add_result.ok()) {
      std::string debug_message = absl::StrCat(
          "Failed adding path '", path,
          "' to the FieldMask tree. Error: ", add_result.ToString());

      BadRequest bad_request_detail;
      BadRequest::FieldViolation* violation =
          bad_request_detail.add_field_violations();
      violation->set_field(path);
      violation->set_description(std::string(add_result.message()));

      status_ = absl::Status(absl::StatusCode::kInvalidArgument,
                             absl::StrCat(debug_message, bad_request_detail));

      return status_;
    }
  }

  if (root_ == nullptr) {
    // If no paths have been added, use the newly created node as the root of
    // this tree.
    root_ = std::move(node);
  } else {
    // If there is already a root node (i.e. some paths have been added to this
    // tree), intersects the current root with the newly created node and uses
    // the result as the new root.
    root_ = root_->Intersect(*node);
  }
  return ::absl::OkStatus();
}

FieldCheckResults FieldMaskTree::CheckField(
    const std::vector<std::string>& path,
    const google::protobuf::Field* field) const {
  // If field is null, then return kInclude.
  if (field == nullptr) {
    return FieldCheckResults::kInclude;
  }
  if (!status_.ok()) {
    ABSL_LOG(FATAL)
        << "The FieldMask tree is broken and cannot be used to check "
           "fields. All fields will be excluded.";
    return FieldCheckResults::kExclude;
  }

  if (root_ == nullptr) {
    return FieldCheckResults::kInclude;
  }
  const FieldMaskNode* current_node = root_.get();
  bool skip_once = false;
  for (const std::string& field_name : path) {
    if (skip_once) {
      skip_once = false;
      continue;
    }
    if (current_node->is_leaf()) {
      // If one of the parent fields is a leaf node on the tree, the whole field
      // should be included.
      return FieldCheckResults::kInclude;
    }
    // TODO(dchakarwarti): Move this flag up to the FieldMaskNode class's
    // callers and utilise it as the params of this class.
    // Ideally, the flag should be introduced only in main.
      if (current_node->HasChild(field_name)) {
        current_node = current_node->FindChild(field_name);
      } else if (current_node->HasChild(kWildcard)) {
        current_node = current_node->FindChild(kWildcard);
      } else {
        return FieldCheckResults::kExclude;
      }
    // Skips the next field when the current field node represents a map field
    // all the map keys are included.
    skip_once = current_node->all_keys_included();
  }
  if (current_node->is_leaf()) {
    return FieldCheckResults::kInclude;
  }
    // Only return partial match for message and enum field. Primitive type can
    // not be partial matched.
    return (field->kind() == google::protobuf::Field::TYPE_MESSAGE ||
            field->kind() == google::protobuf::Field::TYPE_ENUM)
               ? FieldCheckResults::kPartial
               : FieldCheckResults::kExclude;
}

}  // namespace proto_processing_lib::proto_scrubber
