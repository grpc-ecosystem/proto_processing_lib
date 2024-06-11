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

#include "proto_processing_lib/proto_scrubber/field_mask_node.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace proto_processing_lib::proto_scrubber {
namespace testing {

using ::google::protobuf::Type;
using ::google::protobuf::field_extraction::testing::TypeHelper;
using ::testing::ContainsRegex;
using ::ocpdiag::testing::StatusIs;

class FieldMaskNodeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/field_mask_test_proto_descriptor.pb");

    absl::StatusOr<std::unique_ptr<TypeHelper>> status;
    ASSERT_OK_AND_ASSIGN(status, TypeHelper::Create(descriptor_path));
    type_helper_ = std::move(status.value());

    const Type* root_type = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
    EXPECT_NE(root_type, nullptr) << "Cannot find type for type url.";

    node_ = std::make_unique<FieldMaskNode>(
        root_type, false /* is_leaf */, false /* is_map */,
        false /* all_keys_included */,
        std::bind(&FieldMaskNodeTest::FindType, this, std::placeholders::_1));
  }

  FieldMaskNode* node() { return node_.get(); }

 private:
  // A helper function to find a pointer to the type of a type url.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  // A FieldMaskNode for testing.
  std::unique_ptr<FieldMaskNode> node_;
};

TEST_F(FieldMaskNodeTest, CanAddExistingFields) {
  std::vector<std::string> path;
  path = {"message_int32"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  path = {"repeated_int32"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  path = {"message_embedded", "message_embedded"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  path = {"message_embedded", "message_int32"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  path = {"message_embedded", "message_embedded", "message_int32"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));
}

TEST_F(FieldMaskNodeTest, AddingNonExistingFieldsYieldError) {
  std::vector<std::string> path;
  path = {"nonexist"};
  EXPECT_FALSE(node()->InsertField(path.begin(), path.end()).ok());

  path = {"message_embedded", "nonexist"};
  EXPECT_FALSE(node()->InsertField(path.begin(), path.end()).ok());

  path = {"message_embedded", "message_embedded", "nonexist"};
  EXPECT_FALSE(node()->InsertField(path.begin(), path.end()).ok());
}

TEST_F(FieldMaskNodeTest, CanAddPathWithMapKey) {
  std::vector<std::string> path;

  path = {"message_map", "properties[\"abcd\"]", "value"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  path = {"message_map", "properties[\"天地\"]"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  path = {"message_map", "properties[\"天地\"]", "value"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  // Map key can have `/` and `"` characters, so they are escaped.
  path = {"message_map", "properties[\"ab\\\"c\n\td\"]", "value"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  // TODO(b/171267241): Map key is not quoted properly, but is still accepted.
  // Note the ending is incorrectly truncated.
  path = {"message_map", "properties[\"test]", "value"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  // TODO(b/171267241): Map key is not quoted properly, but is still accepted.
  // Note the beginning is incorrectly truncated.
  path = {"message_map", "properties[test\"]", "value"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  EXPECT_EQ(
      "message_map(properties(ab\"c\n\td(value),abcd(value),"
      "est(value),tes(value),天地))",
      node()->DebugString());
}

TEST_F(FieldMaskNodeTest, AddingInvalidFieldsFails) {
  std::vector<std::string> path;

  // Only map field can have values in FieldMask paths.
  path = {"message_embedded[\"abc\"]"};
  EXPECT_THAT(node()->InsertField(path.begin(), path.end()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       ContainsRegex("is not a map field")));

  // Field name is empty.
  path = {"message_map", "[\"abc\"]"};
  EXPECT_THAT(node()->InsertField(path.begin(), path.end()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       ContainsRegex("Cannot find field")));

  // Map key cannot be un-escaped due to ending in `\\`.
  path = {"message_map", "properties[\"test \\\"]", "value"};
  EXPECT_THAT(node()->InsertField(path.begin(), path.end()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       ContainsRegex("Cannot un-escape value")));

  // Invalid field after map key node.
  path = {"message_map", "properties[\"abc\"]", "repeated_int32"};
  EXPECT_THAT(node()->InsertField(path.begin(), path.end()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       ContainsRegex("Cannot find field")));
}

TEST_F(FieldMaskNodeTest, AddingStructsWithNestedSearching) {
  std::vector<std::string> path;

  // google.protobuf.Struct does not have a `values` field as a direct child,
  // but nested searching finds it.
  path = {"message_struct", "values"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  // google.protobuf.Value does not have a `fields` field as a direct child,
  // but nested searching finds it.
  path = {"struct_value", "fields"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  // google.protobuf.ListValue does not have a `fields` field as a direct child,
  // but nested searching finds it.
  path = {"struct_list_value", "fields[\"key\"]"};
  EXPECT_OK(node()->InsertField(path.begin(), path.end()));

  EXPECT_EQ(
      "message_struct(values),struct_list_value(fields(key)),struct_value("
      "fields)",
      node()->DebugString());
}

class FieldMaskIntersectionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/field_mask_test_proto_descriptor.pb");

    absl::StatusOr<std::unique_ptr<TypeHelper>> status;
    ASSERT_OK_AND_ASSIGN(status, TypeHelper::Create(descriptor_path));
    type_helper_ = std::move(status.value());
  }

  // Builds and intersects FieldMask trees based on 'mask1' and 'mask2'. And
  // checks if the result of the intersection is equal to 'expected'.
  void CheckIntersectResult(absl::Span<const std::string> mask1,
                            absl::Span<const std::string> mask2,
                            absl::string_view expected) {
    std::unique_ptr<FieldMaskNode> node1 = BuildNode(mask1);
    std::unique_ptr<FieldMaskNode> node2 = BuildNode(mask2);
    EXPECT_EQ(expected, node1->Intersect(*node2)->DebugString());
  }

 private:
  // A helper function to find a pointer to the type of a type url.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  // Builds and returns a FieldMaskNode based on FieldMask paths in 'paths'.
  std::unique_ptr<FieldMaskNode> BuildNode(
      absl::Span<const std::string> paths) {
    const Type* root_type = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
    EXPECT_NE(root_type, nullptr) << "Cannot find type for type url.";

    std::unique_ptr<FieldMaskNode> result(
        new FieldMaskNode(root_type, false /* is_leaf */, false /* is_map */,
                          false /* all_keys_included */,
                          std::bind(&FieldMaskIntersectionTest::FindType, this,
                                    std::placeholders::_1)));

    for (const std::string& path : paths) {
      std::vector<std::string> segments = absl::StrSplit(path, '.');
      EXPECT_OK(result->InsertField(segments.begin(), segments.end()));
    }
    return result;
  }

  // The TypeHelper for the testing service.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
};

TEST_F(FieldMaskIntersectionTest, IdenticalMaskYieldSameResult) {
  CheckIntersectResult({"message_int32"}, {"message_int32"}, "message_int32");
  CheckIntersectResult({"message_int32", "message_embedded.message_int32"},
                       {"message_int32", "message_embedded.message_int32"},
                       "message_embedded(message_int32),message_int32");
  // A path to a parent node should override paths to its children. So these 2
  // lists of paths are essentially the same.
  CheckIntersectResult(
      {"message_int32", "message_embedded.message_int32", "message_embedded"},
      {"message_int32", "message_embedded"}, "message_embedded,message_int32");
}

TEST_F(FieldMaskIntersectionTest, NonOverlappingMasksYieldEmptyResult) {
  CheckIntersectResult({}, {}, "");
  CheckIntersectResult({"message_int32"}, {}, "");
  CheckIntersectResult({}, {"message_int32"}, "");
  CheckIntersectResult({"message_int32"}, {"message_int64"}, "");
  CheckIntersectResult({"message_int64"}, {"message_int32"}, "");
  CheckIntersectResult({"message_int64", "message_embedded.message_int32"},
                       {"message_int32", "message_embedded.message_int64"}, "");
}

TEST_F(FieldMaskIntersectionTest, ResultOnlyHasPathsInBothMasks) {
  CheckIntersectResult({"message_embedded"}, {"message_embedded.message_int32"},
                       "message_embedded(message_int32)");
  CheckIntersectResult({"message_embedded.message_int32"}, {"message_embedded"},
                       "message_embedded(message_int32)");
  CheckIntersectResult(
      {"message_embedded"},
      {"message_embedded.message_int32", "message_embedded.message_int64"},
      "message_embedded(message_int32,message_int64)");
  CheckIntersectResult(
      {"message_embedded.message_int32", "message_embedded.message_int64"},
      {"message_embedded"}, "message_embedded(message_int32,message_int64)");
  CheckIntersectResult(
      {"message_embedded.message_int32", "message_embedded.message_int64"},
      {"message_embedded.message_int64"}, "message_embedded(message_int64)");
}

TEST_F(FieldMaskIntersectionTest, IntersectMapFieldWorks) {
  CheckIntersectResult(
      {"message_embedded.maps.message_map.message_int32"},
      {"message_embedded.maps.message_map[\"key\"].message_int32"},
      "message_embedded(maps(message_map(key(message_int32))))");
  CheckIntersectResult(
      {"message_embedded.maps.message_map[\"key\"].message_int32"},
      {"message_embedded.maps.message_map.message_int32"},
      "message_embedded(maps(message_map(key(message_int32))))");
  CheckIntersectResult(
      {"message_embedded.maps.message_map[\"key\"].message_int32"},
      {"message_embedded.maps.message_map[\"key\"].message_int32"},
      "message_embedded(maps(message_map(key(message_int32))))");
  CheckIntersectResult(
      {"message_embedded.maps.message_map[\"key1\"].message_int32"},
      {"message_embedded.maps.message_map[\"key2\"].message_int32"}, "");
  CheckIntersectResult({"message_embedded.maps.message_map.message_int32"},
                       {"message_embedded.maps.message_map.message_int32"},
                       "message_embedded(maps(message_map(message_int32)))");
}

}  // namespace testing
}  // namespace proto_processing_lib::proto_scrubber
