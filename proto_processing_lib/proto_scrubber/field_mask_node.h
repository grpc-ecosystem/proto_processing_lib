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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_NODE_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_NODE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"

namespace proto_processing_lib::proto_scrubber {

// FieldMaskNode represents a node in a compiled FieldMask tree. For example,
// a FieldMask like "a.b,a.c.d,a.c.e" can be compiled to a tree structure:
//
//   a----b
//    |
//    +---c----d
//         |
//         +---e
//
// A FieldMask tree is an "union" of a list of FieldMask paths, where more
// inclusive paths override less inclusive ones. i.e., a path to a parent node
// (like "a.b") overrides paths to its children (like "a.b.c" and "a.b.d").
//
// More information about FieldMask can be found in:
//   //depot/google3/google/protobuf/field_mask.proto
// Note that the restriction on repeated fields described in field_mask.proto is
// not checked/enforced here. That restriction is mainly to reduce the
// complexity of using FieldMasks for updating. FieldMaskNode is only used for
// projection/scrubbing. It would be too restrictive and confusing to forbid
// repeated fields on non-leaf nodes here.
class FieldMaskNode {
 public:
  // Creates a FieldMaskNode. 'type' is a pointer to the Type of the field
  // corresponding to this node. it should be nullptr when the field is not of
  // message type. 'is_leaf' indicates of this node is a leaf of its tree.
  // 'type_finder' is a function object that finds a pointer to a Type given its
  // URL.
  // 'name_lookup' is a function object that finds the proto field name given
  // the unnormalized camel-case name.
  FieldMaskNode(const google::protobuf::Type* type, bool is_leaf, bool is_map,
                bool all_keys_included,
                std::function<const google::protobuf::Type*(const std::string&)>
                    type_finder);

  // This type is neither copyable nor movable.
  FieldMaskNode(const FieldMaskNode&) = delete;
  FieldMaskNode& operator=(const FieldMaskNode&) = delete;

  virtual ~FieldMaskNode() {}

  // Inserts a field path into the sub-tree with this node as the root. After
  // the call, this node represents the "union" of the tree before the call and
  // the newly added field (i.e., paths to parents override paths to their
  // children). 'current' is the start of the list of fields on the path, and
  // 'end' points to the end of it.
  absl::Status InsertField(
      const std::vector<std::string>::const_iterator current,
      const std::vector<std::string>::const_iterator end);

  // Intersects the FieldMask tree rooted on this node with another tree rooted
  // on 'other'. Returns a FieldMask tree containing paths that are on both
  // trees.
  std::unique_ptr<FieldMaskNode> Intersect(const FieldMaskNode& other) const;

  // Whether there is a child node with given field name.
  const bool HasChild(absl::string_view field_name) const;

  // Finds the child node for the given field name. Returns nullptr if no
  // children exist for the field name.
  const FieldMaskNode* FindChild(absl::string_view field_name) const;

  // Returns a string representation of the FieldMask tree for debugging/testing
  // purposes.
  std::string DebugString() const;

  // Returns true if this node is a leaf node of its FieldMask tree. Leaf nodes
  // cover their own field number and any nested field numbers
  bool is_leaf() const { return is_leaf_; }

  // Returns a pointer to the message type of this node's field, or nullptr if
  // the node's field is not a message. Owned by EnvelopeServer.
  const google::protobuf::Type* type() const { return type_; }

  // Returns true if this node is for a map field and all the map's key should
  // be included.
  bool all_keys_included() const { return all_keys_included_; }

 private:
  using ChildMap =
      absl::flat_hash_map<std::string, std::unique_ptr<FieldMaskNode>>;
  using FieldNameMap =
      absl::flat_hash_map<absl::string_view, const google::protobuf::Field*>;

  // Clones the current node and its sub-nodes recursively.
  std::unique_ptr<FieldMaskNode> Clone() const;

  absl::Status InsertValueField(
      absl::string_view value,
      const std::vector<std::string>::const_iterator next,
      const std::vector<std::string>::const_iterator end);

  // Returns the value type of a map given the Type of the map entry. Returns
  // nullptr if the value type is not a message type or the type cannot be found
  // using type_finder_.
  const google::protobuf::Type* GetMapValueType(
      const google::protobuf::Type& found_type);

  // Adds a new child to the current node.
  absl::Status AddChild(const absl::string_view name,
                        const google::protobuf::Type* child_type,
                        absl::string_view map_key, bool all_keys_included,
                        const std::vector<std::string>::const_iterator next,
                        const std::vector<std::string>::const_iterator end);

  // Finds and returns a pointer to a child field based on its name.
  const google::protobuf::Field* FindChildField(absl::string_view field_name);

  // Parses a path segment to get the field name and the un-escaped map key if
  // there is one in the segment. The path segment is formatted like this:
  // field["escaped_map_key"], where "field" is the field_name and
  // "escaped_map_key" is the map_key and is escaped with C/C++ style. The part
  // starting from '[' can be omitted (and in that case 'map_key' will be set to
  // an empty string).
  absl::Status ParseFieldNameAndMapKey(const std::string& segment,
                                       std::string* field_name,
                                       std::string* map_key);

  // A pointer to the message type of this node's field, or nullptr if the
  // node's field is not a message. Owned by EnvelopeServer.
  const google::protobuf::Type* type_;
  // Whether this is a leaf node of the tree.
  bool is_leaf_;
  // Whether this node is for a map field.
  bool is_map_;
  // Whether all keys should be included when the current node is for a map
  // field.
  bool all_keys_included_;
  // A map between child names (field name or map key value) and their
  // corresponding child nodes.
  ChildMap children_;
  // A map between field name and their corresponding field within the type_.
  // This is for internal use only to speed up FieldMask compilation.
  FieldNameMap child_name_child_map_;
  // A function object that finds the pointer to a Type object given its URL.
  std::function<const google::protobuf::Type*(const std::string&)> type_finder_;
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_NODE_H_
