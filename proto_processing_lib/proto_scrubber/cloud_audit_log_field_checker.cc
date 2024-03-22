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

#include "proto_processing_lib/proto_scrubber/cloud_audit_log_field_checker.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "proto_processing_lib/factory_helper.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/field_mask_node.h"
#include "ortools/base/map_util.h"

namespace proto_processing_lib::proto_scrubber {
namespace {

// Returns pointer to message type name, or nullptr if field isn't a message or
// has an unknown type. Caller does not own returned pointer.
const std::string* GetMessageTypeName(const FieldMaskNode* node);
}  // namespace

CloudAuditLogFieldChecker::CloudAuditLogFieldChecker(
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder)
    : field_mask_tree_(
          FactoryCreateFieldMaskTree(type, std::move(type_finder))) {}

absl::Status CloudAuditLogFieldChecker::AddOrIntersectFieldPaths(
    const std::vector<std::string>& paths) {
  return field_mask_tree_->AddOrIntersectFieldPaths(paths);
}

FieldCheckResults CloudAuditLogFieldChecker::CheckField(
    const std::vector<std::string>& path,
    const google::protobuf::Field* field) const {
  if (!field_mask_tree_->status().ok()) {
    LOG(DFATAL)
        << "The Cloud Audit Log field checker is broken and cannot be used to"
           "check fields. All fields will be excluded.";
    return FieldCheckResults::kExclude;
  }

  if (field_mask_tree_->root() == nullptr) {
    return FieldCheckResults::kInclude;
  }

  // Keep tracking the message type so that we can detect cyclic messages.
  absl::flat_hash_map<std::string, const FieldMaskNode*> type_to_node;
  const FieldMaskNode* current_node = field_mask_tree_->root();
  for (int i = 0; i <= path.size(); ++i) {
    // Track and check possible cyclic message in the path.
    const std::string* type_name = GetMessageTypeName(current_node);

    if (type_name != nullptr) {
      const FieldMaskNode* found_node =
          gtl::FindPtrOrNull(type_to_node, *type_name);
      if (found_node == nullptr) {
        // Keep track of this message type in case it's cyclic.
        type_to_node.emplace(*type_name, current_node);
      } else {
        // Found a cyclic message. Reset current_node pointer to first instance
        // of message type.
        current_node = found_node;
      }
    }

    // If one of the parent fields is a leaf node on the tree, the whole field
    // should be included.
    if (current_node->is_leaf()) {
      return FieldCheckResults::kInclude;
    }

    // Current node is not leaf but it has no children with current field name,
    // which means given path should be excluded.
    if (i < path.size()) {
      const std::string& field_name = path[i];
      current_node = current_node->FindChild(field_name);
      if (current_node == nullptr) {
        return FieldCheckResults::kExclude;
      }

      // When the current node represents a map field and all of its keys should
      // be included in the result, the next segment on the path (which is a
      // map key) should be skipped.
      if (current_node->all_keys_included()) {
        ++i;
      }
    }
  }  // end for-range loop

  return FieldCheckResults::kPartial;
}

namespace {

const std::string* GetMessageTypeName(const FieldMaskNode* node) {
  return (node != nullptr && node->type() != nullptr &&
          !node->type()->name().empty())
             ? &node->type()->name()
             : nullptr;
}
}  // namespace
}  // namespace proto_processing_lib::proto_scrubber
