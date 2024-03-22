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

#include "proto_processing_lib/proto_scrubber/field_mask_tree.h"

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
#include "absl/strings/string_view.h"
#include "proto_field_extraction/field_extractor/field_extractor_util.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace proto_processing_lib::proto_scrubber {
namespace testing {

using ::google::protobuf::Type;
using ::google::protobuf::field_extraction::FindField;
using ::google::protobuf::field_extraction::testing::TypeHelper;

class FieldMaskTreeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/field_mask_test_proto_descriptor.pb");

    auto status = TypeHelper::Create(descriptor_path);
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());

    root_type_ = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
    tree_ = std::make_unique<FieldMaskTree>(
        root_type_,
        std::bind(&FieldMaskTreeTest::FindType, this, std::placeholders::_1));
  }

  absl::Status AddOrIntersectPaths(const std::vector<std::string>& paths) {
    return tree_->AddOrIntersectFieldPaths(paths);
  }

  // A helper function to call `CheckField`, where the field comes from the root
  // type.
  FieldCheckResults CheckField(const std::vector<std::string>& path,
                               absl::string_view field_name) {
    return CheckField(
        path, field_name,
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  }

  // A helper function to call `CheckField`, where the field comes from the type
  // provided.
  FieldCheckResults CheckField(const std::vector<std::string>& path,
                               absl::string_view field_name,
                               absl::string_view parent_type_url) {
    const Type* type = FindType(std::string(parent_type_url));
    EXPECT_NE(type, nullptr)
        << "Cannot find type for type url " << parent_type_url;
    const google::protobuf::Field* field = FindField(*type, field_name);
    EXPECT_NE(field, nullptr)
        << "Cannot find field " << field_name << " under type " << type->name();
    return tree_->CheckField(path, field);
  }

  // A helper function to find a pointer to the type of a type url.
  const Type* FindType(const std::string& type_url) {
    return type_helper_->ResolveTypeUrl(type_url);
  }

  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  // A FieldMaskNode for testing.
  std::unique_ptr<FieldMaskTree> tree_;
  // A field variable to pass for testing.
  const Type* root_type_;
};

TEST_F(FieldMaskTreeTest, CheckFieldYieldCorrectResult) {
  // common_typos_disable
  ASSERT_OK(AddOrIntersectPaths(
      {"message_embedded.embedded2.embedded3", "message_int32"}));

  // Fields not in the path return kExclude.
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded"}, "message_int32"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_int64"}, "message_int32"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "message_int32"}, "message_int32"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "embedded2"}, "message_int32"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "embedded2", "message_int32"},
                       "message_int32"));

  // Non-leaf nodes return kPartial.
  EXPECT_EQ(
      FieldCheckResults::kPartial,
      CheckField(
          {"message_embedded"}, "embedded2",
          "type.googleapis.com/"
          "proto_processing_lib.scrubber.testing.ScrubberTestEmbeddedMessage"));
  EXPECT_EQ(
      FieldCheckResults::kPartial,
      CheckField(
          {"message_embedded", "embedded2"}, "embedded3",
          "type.googleapis.com/"
          "proto_processing_lib.scrubber.testing.SecondLevelEmbeddedMessage"));

  // Leaf node returns kInclude.
  EXPECT_EQ(
      FieldCheckResults::kInclude,
      CheckField(
          {"message_embedded", "embedded2", "embedded3"}, "message_int32",
          "type.googleapis.com/"
          "proto_processing_lib.scrubber.testing.ThirdLevelEmbeddedMessage"));

  // Leaf nodes stop traversal to the field. This means an invalid field will be
  // allowed, it is not validated.
  EXPECT_EQ(FieldCheckResults::kInclude,
            CheckField({"message_int32"}, "repeated_string"));
  EXPECT_EQ(FieldCheckResults::kInclude,
            CheckField({"message_embedded", "embedded2", "embedded3"},
                       "repeated_string"));

  // Leaf nodes stop traversal of nodes in path. This means invalid field names
  // in the path will be allowed, they will not be validated.
  EXPECT_EQ(FieldCheckResults::kInclude,
            CheckField({"message_embedded", "embedded2", "embedded3",
                        "repeated_string"},
                       "message_int32"));
}

TEST_F(FieldMaskTreeTest, OnlyIntersectionIsLeftAfterTwoListOfPathsAdded) {
  ASSERT_OK(AddOrIntersectPaths(
      {"message_embedded.embedded2.embedded3", "message_int32"}));
  ASSERT_OK(AddOrIntersectPaths(
      {"message_embedded.embedded2.embedded3.message_int32", "message_int64"}));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_int32"}, "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded"}, "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_int64"}, "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "message_int32"}, "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "embedded2"}, "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "embedded2", "message_int32"},
                       "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "embedded2", "embedded3"},
                       "message_int64"));
  EXPECT_EQ(FieldCheckResults::kExclude,
            CheckField({"message_embedded", "embedded2", "embedded3",
                        "repeated_string"},
                       "message_int64"));
  EXPECT_EQ(FieldCheckResults::kInclude,
            CheckField(
                {"message_embedded", "embedded2", "embedded3", "message_int32"},
                "message_int64"));
}

TEST_F(FieldMaskTreeTest, WrongPathCannotBeAddedToTree) {
  EXPECT_FALSE(AddOrIntersectPaths({"message_embedded.nonexist"}).ok());
}

TEST_F(FieldMaskTreeTest, UnknownFieldAlwaysIncluded) {
  EXPECT_EQ(FieldCheckResults::kInclude,
            tree_->CheckField({"message_int32"}, nullptr));
}

}  // namespace testing
}  // namespace proto_processing_lib::proto_scrubber
