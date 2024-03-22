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

#include "proto_processing_lib/proto_scrubber/field_mask_node.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "src/google/protobuf/util/converter/utility.h"
#include "proto_processing_lib/proto_scrubber/constants.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "ortools/base/map_util.h"
#include "ocpdiag/core/compat/status_macros.h"

namespace proto_processing_lib::proto_scrubber {

using google::protobuf::util::converter::ToSnakeCase;

FieldMaskNode::FieldMaskNode(
    const google::protobuf::Type* type, bool is_leaf, bool is_map,
    bool all_keys_included,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder)
    : type_(type),
      is_leaf_(is_leaf),
      is_map_(is_map),
      all_keys_included_(all_keys_included),
      type_finder_(std::move(type_finder)) {}

absl::Status FieldMaskNode::AddChild(
    const absl::string_view name, const google::protobuf::Type* child_type,
    absl::string_view map_key, bool all_keys_included,
    const std::vector<std::string>::const_iterator next,
    const std::vector<std::string>::const_iterator end) {
  // Note that all_keys_included is true only when the current node represents
  // a Map.
  const std::pair<ChildMap::iterator, bool> emplace_result = children_.emplace(
      std::string(name), std::make_unique<FieldMaskNode>(
                             child_type, (map_key.empty() && next == end),
                             (!map_key.empty() || all_keys_included),
                             all_keys_included, type_finder_));
  if (!emplace_result.second) {
    const std::string message = absl::StrCat(
        "Error compiling FieldMask.  Failed to create a FieldMaskNode for field"
        " '",
        name, "'.");
    VLOG(3) << message;
    return absl::Status(absl::StatusCode::kInternal, message);
  }
  if (map_key.empty()) {
    return emplace_result.first->second->InsertField(next, end);
  }
  return emplace_result.first->second->InsertValueField(map_key, next, end);
}

const google::protobuf::Field* FieldMaskNode::FindChildField(
    absl::string_view field_name) {
  // Fills the field name to field number map if it's empty (the first time this
  // method is called).
  if (child_name_child_map_.empty()) {
    for (const google::protobuf::Field& field : type_->fields()) {
      gtl::InsertIfNotPresent(&child_name_child_map_, field.name(), &field);
    }
      if (type_->name() == kStructType || type_->name() == kStructValueType ||
          type_->name() == kStructListValueType) {
        // Add Struct type fields and List type fields for nested searching.
        // A Struct value can be List. A List value can be Struct.
        for (const google::protobuf::Field& field :
             type_finder_(kStructTypeUrl)->fields()) {
          gtl::InsertIfNotPresent(&child_name_child_map_, field.name(), &field);
        }
        for (const google::protobuf::Field& field :
             type_finder_(kStructListValueTypeUrl)->fields()) {
          gtl::InsertIfNotPresent(&child_name_child_map_, field.name(), &field);
        }
      }

  }
  return gtl::FindWithDefault(child_name_child_map_, field_name);
}

absl::Status FieldMaskNode::ParseFieldNameAndMapKey(const std::string& segment,
                                                    std::string* field_name,
                                                    std::string* map_key) {
  auto key_start = segment.find('[');
  *field_name = segment.substr(0, key_start);
  const std::string escaped_map_key =
      (key_start == std::string::npos
           ? ""
           : segment.substr(key_start + 2, segment.length() - key_start - 4));
  std::string error;
  if (!absl::CUnescape(escaped_map_key, map_key, &error)) {
    const std::string message =
        absl::StrCat("Error compiling FieldMask. Cannot un-escape value \"",
                     escaped_map_key, "\". Error: ", error);
    VLOG(3) << message;
    return absl::Status(absl::StatusCode::kInvalidArgument, message);
  }
  return ::absl::OkStatus();
}

absl::Status FieldMaskNode::InsertField(
    const std::vector<std::string>::const_iterator current,
    const std::vector<std::string>::const_iterator end) {
  if (current == end) {
    // Reaches the end of the field path, which means this node is a leaf.
    is_leaf_ = true;
    return ::absl::OkStatus();
  }
  if (type_ == nullptr) {
    const std::string message =
        "Error compiling FieldMask. Non-message types must be leaf nodes.";
    VLOG(3) << message;
    return absl::Status(absl::StatusCode::kInvalidArgument, message);
  }

  // The current node is a leaf node already. There is no need to keep creating
  // child nodes. For example, if "a.b" has already been added to a FieldMask
  // tree, all the sub-fields of "b" are already included and there is no need
  // to add masks like "a.b.c".
  if (is_leaf_) {
    return ::absl::OkStatus();
  }

  std::string field_name;
  std::string map_key;
  RETURN_IF_ERROR(ParseFieldNameAndMapKey(*current, &field_name, &map_key));

  if (field_name.empty()) {
    const std::string message =
        absl::StrCat("Error compiling FieldMask. Cannot find field '",
                     ToSnakeCase(*current), "'.");
    VLOG(3) << message;
    return absl::Status(absl::StatusCode::kInvalidArgument, message);
  }

  const google::protobuf::Field* field = FindChildField(field_name);
  if (field == nullptr) {
    const std::string message = absl::StrCat(
        "Error compiling FieldMask. Cannot find field '", field_name, "'.");
    VLOG(3) << message;
    return absl::Status(absl::StatusCode::kInvalidArgument, message);
  }

  const std::vector<std::string>::const_iterator next = current + 1;
  ChildMap::const_iterator child = children_.find(field_name);

  // Found a child node for the field, calls its InsertField with the remaining
  // list.
  if (child != children_.end()) {
    if (map_key.empty()) {
      return child->second->InsertField(next, end);
    }
    return child->second->InsertValueField(map_key, next, end);
  }

  // Creates a new child node if none has been created for the current field.
  const google::protobuf::Type* child_type = nullptr;
  if (field->kind() == google::protobuf::Field::TYPE_MESSAGE) {
    child_type = type_finder_(field->type_url());
    if (child_type == nullptr) {
      const std::string message =
          absl::StrCat("Error compiling FieldMask. Cannot resolve type '",
                       field->type_url(), "'.");
      VLOG(3) << message;
      return absl::Status(absl::StatusCode::kInternal, message);
    }

    if (IsMap(*field, *child_type)) {
      return AddChild(field_name, GetMapValueType(*child_type), map_key,
                      map_key.empty(), next, end);
    } else if (!map_key.empty()) {
      const std::string message = absl::StrCat(
          "Error compiling FieldMask. '", field_name,
          "' is not a map field. Only map key values are supported in "
          "FieldMask paths.");
      VLOG(3) << message;
      return absl::Status(absl::StatusCode::kInvalidArgument, message);
    }
  }
  return AddChild(field_name, child_type, map_key, false, next, end);
}

absl::Status FieldMaskNode::InsertValueField(
    absl::string_view value,
    const std::vector<std::string>::const_iterator next,
    const std::vector<std::string>::const_iterator end) {
  // The current node is a leaf node already. There is no need to keep creating
  // child nodes. For example, if "a.b" has already been added to a FieldMask
  // tree, all the subfields of "b" are already included and there is no need
  // to add masks like "a.b.c".
  if (is_leaf_) {
    return ::absl::OkStatus();
  }
  if (all_keys_included_) {
    return InsertField(next, end);
  }
  ChildMap::const_iterator child = children_.find(value);

  // Found a child node for the field, calls its InsertField with the remaining
  // list.
  if (child != children_.end()) {
    return child->second->InsertField(next, end);
  }
  return AddChild(value, type_, "", false, next, end);
}

const bool FieldMaskNode::HasChild(absl::string_view field_name) const {
  return children_.contains(field_name);
}

const FieldMaskNode* FieldMaskNode::FindChild(
    absl::string_view field_name) const {
  ChildMap::const_iterator found = children_.find(field_name);
  if (found == children_.end()) {
    return nullptr;
  }
  return found->second.get();
}

std::unique_ptr<FieldMaskNode> FieldMaskNode::Intersect(
    const FieldMaskNode& other) const {
  // When one of the node is a leaf, then the other one must be the
  // intersection of them.
  if (is_leaf_) {
    return other.Clone();
  }
  if (other.is_leaf()) {
    return this->Clone();
  }

  // Intersects the nodes normally if one of the nodes is not a map field. Or
  // if they are map fields and their is_all_key_included_ is the same, they can
  // be treated as if they are not map fields.
  if (!is_map_ || !other.is_map_ ||
      all_keys_included_ == other.all_keys_included_) {
    auto result = std::make_unique<FieldMaskNode>(
        type_, /*is_leaf=*/false, (is_map_ && other.is_map_),
        all_keys_included_, type_finder_);

    // Finds the fields contained in both 'this' and 'other'.
    for (const auto& child : children_) {
      const FieldMaskNode* other_child = other.FindChild(child.first);
      if (other_child == nullptr) {
        continue;
      }
      result->children_.emplace(child.first,
                                child.second->Intersect(*other_child));
    }
    return result;
  }

  // Intersects 2 map nodes with one of them has all keys included and the other
  // one doesn't.
  auto result = std::make_unique<FieldMaskNode>(
      type_, /*is_leaf=*/false, /*is_map=*/true,
      /*all_keys_included=*/false, type_finder_);
  const ChildMap* map_keys = all_keys_included_ ? &other.children_ : &children_;
  const FieldMaskNode* node_with_all_keys = all_keys_included_ ? this : &other;
  for (const auto& child : *map_keys) {
    result->children_.emplace(child.first,
                              child.second->Intersect(*node_with_all_keys));
  }
  return result;
}

std::unique_ptr<FieldMaskNode> FieldMaskNode::Clone() const {
  auto result = std::make_unique<FieldMaskNode>(
      type_, is_leaf_, is_map_, all_keys_included_, type_finder_);
  if (!is_leaf_) {
    for (const auto& child : children_) {
      result->children_.emplace(child.first, child.second->Clone());
    }
  }
  return result;
}

const google::protobuf::Type* FieldMaskNode::GetMapValueType(
    const google::protobuf::Type& found_type) {
  // If this field is a map, we should use the type of its "Value" as
  // the type of the child node.
  for (const google::protobuf::Field& sub_field : found_type.fields()) {
    // Looks for the map's value field, whose field number is always 2.
    if (sub_field.number() != 2) {
      continue;
    }
    if (sub_field.kind() != google::protobuf::Field::TYPE_MESSAGE) {
      // This map's value type is not a message type. We don't need to
      // get the field_type in this case.
      break;
    }
    return type_finder_(sub_field.type_url());
  }
  return nullptr;
}

std::string FieldMaskNode::DebugString() const {
  std::vector<std::string> child_results;
  for (const auto& child : children_) {
    std::string child_result = child.second->DebugString();
    if (!child_result.empty()) {
      child_results.push_back(
          absl::StrCat(child.first, "(", child_result, ")"));
    } else if (child.second->is_leaf()) {
      child_results.push_back(absl::StrCat(child.first));
    }
  }
  // Since children_ is an unordered_map, child_results has to be sorted to make
  // the resulting string consistent.
  std::sort(child_results.begin(), child_results.end());
  return absl::StrJoin(child_results, ",");
}

}  // namespace proto_processing_lib::proto_scrubber
