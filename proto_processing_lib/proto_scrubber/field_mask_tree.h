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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_TREE_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_TREE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/status/status.h"
#include "proto_processing_lib/interface_util.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/field_mask_node.h"

namespace proto_processing_lib::proto_scrubber {

extern const char* const kWildcard;

// A tree representation of a list of FieldMask paths. Each node on the tree
// represents a field or a key of a map field that is reachable by at least one
// of the paths. For example, a FieldMask like "a.b["key"],a.c.d,a.c.e" ("b" is
// map field) can be compiled to a tree structure:
//
//   a----b----key
//    |
//    +---c----d
//         |
//         +---e
//
// Note that all map keys are strings escaped with C/C++ style. Even map keys
// of a integral type are represented as strings.
//
// More information about FieldMask can be found in:
//   //depot/google3/google/protobuf/field_mask.proto
class FieldMaskTree : public FieldMaskTreeInterface {
 public:
  // 'type' is a pointer to the protobuf Type that this tree is applied to.
  // 'type_finder' is a function object that finds a pointer to a Type given its
  // URL.
  // 'name_lookup' is a function object that finds the proto field name given
  // the unnormalized camel-case name.
  FieldMaskTree(
      const google::protobuf::Type* type,
      std::function<const google::protobuf::Type*(const std::string&)>
          type_finder);

  // This type is neither copyable nor movable.
  FieldMaskTree(const FieldMaskTree&) = delete;
  FieldMaskTree& operator=(const FieldMaskTree&) = delete;

  ~FieldMaskTree() override {}

  // Adds a list of FieldMask paths to the tree if no paths have been added. The
  // resulting tree will be the union of "paths" (i.e. a paths to a parent field
  // override paths to its child fields). For subsequent calls, the resulting
  // tree represents an intersection between 'paths' and the previously added
  // paths.
  // Returns OK if all "paths" are legal, otherwise returns INVALID_ARGUMENT and
  // the tree will be left in a broken state. Calling this method again on a
  // broken FieldMaskTree will yield an error of FAILED_PRECONDITION.
  absl::Status AddOrIntersectFieldPaths(const std::vector<std::string>& paths);

  // Checks if a field is on this tree. Returns "kInclude" if the field is on a
  // leaf node of the tree, "kPartial" if it's on a non-leaf node and "kExclude"
  // if the field is not on the tree. The field is represented by the path (a
  // list of field numbers) from the root message type to the field. Returns
  // "kExclude" if this FieldMaskTree is broken instance (see comments of
  // AddOrIntersectFieldPaths()).
  FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const override;

  bool SupportAny() const override { return false; }

  FieldCheckResults CheckType(
      const google::protobuf::Type* type) const override {
    return FieldCheckResults::kInclude;
  }

  FieldFilters FilterName() const override {
    return FieldFilters::FieldMaskFilter;
  }

  // Returns the status of the tree.
  absl::Status status() { return status_; }

  // Returns root node of the tree.
  const FieldMaskNode* root() const { return root_.get(); }

 private:
  // The root node of the FieldMask tree.
  std::unique_ptr<FieldMaskNode> root_;
  // The Type of the proto messages that the scrubber applies to.
  const google::protobuf::Type* type_;
  // A function object that finds the pointer to a Type object given its URL.
  std::function<const google::protobuf::Type*(const std::string&)> type_finder_;
  // The result of compiling the field paths into a tree.
  absl::Status status_;
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_MASK_TREE_H_
