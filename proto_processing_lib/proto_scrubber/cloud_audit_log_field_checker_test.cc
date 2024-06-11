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

#include "proto_processing_lib/proto_scrubber/cloud_audit_log_field_checker.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace proto_processing_lib::proto_scrubber {
namespace testing {

using ::absl::bind_front;
using ::google::protobuf::Type;
using ::google::protobuf::field_extraction::testing::TypeHelper;
using ::proto_processing_lib::proto_scrubber::CloudAuditLogFieldChecker;
using proto_processing_lib::proto_scrubber::FieldCheckResults;
using ::ocpdiag::testing::StatusIs;

class CloudAuditLogFieldCheckerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/"
        "cloud_audit_log_field_checker_test_proto_descriptor.pb");

    auto status = TypeHelper::Create(descriptor_path);
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());

    type_finder_ = bind_front(&CloudAuditLogFieldCheckerTest::FindType, this);

    // Resolve the pointer to the type of root message.
    root_type_ = type_finder_(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.log.testing.TestMessage");

    // Create CloudAuditLogFieldChecker instance.
    cloud_audit_log_field_checker_ =
        std::make_unique<CloudAuditLogFieldChecker>(root_type_, type_finder_);
  }

  absl::Status AddOrIntersectPaths(const std::vector<std::string>& paths) {
    return cloud_audit_log_field_checker_->AddOrIntersectFieldPaths(paths);
  }

  FieldCheckResults CheckField(const std::vector<std::string>& path) {
    return cloud_audit_log_field_checker_->CheckField(path, nullptr);
  }

  CloudAuditLogFieldChecker* cloud_audit_log_field_checker() {
    return cloud_audit_log_field_checker_.get();
  }

  const Type* root_type() { return root_type_; }

 private:
  // Tries to find the Type for `type_url`.
  const Type* FindType(absl::string_view type_url) {
    absl::StatusOr<const Type*> type_status_or =
        type_helper_->ResolveTypeUrl(type_url);
    if (type_status_or.ok()) {
      return type_status_or.value();
    }

    return nullptr;
  }

  const Type* root_type_;
  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  std::function<const Type*(const std::string&)> type_finder_;
  std::unique_ptr<CloudAuditLogFieldChecker> cloud_audit_log_field_checker_;
};

TEST_F(CloudAuditLogFieldCheckerTest, NormalPathTests) {
  ASSERT_OK(AddOrIntersectPaths({"singular_int32", "repeated_enum",
                                 "message_embedded.embedded2.embedded3"}));

  // Include
  EXPECT_EQ(CheckField({"singular_int32"}), FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"repeated_enum"}), FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3",
                        "repeated_string"}),
            FieldCheckResults::kInclude);

  // Partial
  EXPECT_EQ(CheckField({"message_embedded"}), FieldCheckResults::kPartial);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2"}),
            FieldCheckResults::kPartial);

  // Exclude
  EXPECT_EQ(CheckField({"singular_float"}), FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"wkt_double"}), FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"message_embedded", "duration"}),
            FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "singular_string"}),
            FieldCheckResults::kExclude);
}

TEST_F(CloudAuditLogFieldCheckerTest, NormalPathTestsLeafAsMap) {
  ASSERT_OK(AddOrIntersectPaths({"message_embedded.message_map.properties"}));

  EXPECT_EQ(CheckField({"message_embedded", "message_map", "properties"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(
      CheckField({"message_embedded", "message_map", "properties", "map_id"}),
      FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "message_map", "properties",
                        "repeated_value"}),
            FieldCheckResults::kInclude);
}

TEST_F(CloudAuditLogFieldCheckerTest, NormalPathTestsNonLeafAsMap) {
  ASSERT_OK(AddOrIntersectPaths(
      {"message_embedded.message_map.properties.repeated_value"}));

  EXPECT_EQ(CheckField({"message_embedded", "message_map", "properties",
                        "repeated_value"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(CheckField({"message_embedded", "message_map", "properties"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(
      CheckField({"message_embedded", "message_map", "properties", "name"}),
      FieldCheckResults::kPartial);
}

TEST_F(CloudAuditLogFieldCheckerTest, CyclicPath) {
  // Corresponding field mask tree:
  //  message_embedded ------------ embedded2 -- embedded3 -- message_embedded
  //         |                          |
  //         +-- message_embedded       +-- singular_int64
  //         |
  //         +-- singular_int64
  ASSERT_OK(AddOrIntersectPaths(
      {"message_embedded.message_embedded", "message_embedded.singular_int64",
       "message_embedded.embedded2.embedded3.message_embedded",
       "message_embedded.embedded2.singular_string"}));

  // Include
  EXPECT_EQ(CheckField({"message_embedded", "singular_int64"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(
      CheckField({"message_embedded", "message_embedded", "singular_int64"}),
      FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "message_embedded",
                        "message_embedded", "singular_int64"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3",
                        "message_embedded", "singular_int64"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3",
                        "message_embedded", "embedded2", "singular_string"}),
            FieldCheckResults::kInclude);

  // Partial
  EXPECT_EQ(CheckField({"message_embedded"}), FieldCheckResults::kPartial);

  EXPECT_EQ(CheckField({"message_embedded", "message_embedded"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(
      CheckField({"message_embedded", "message_embedded", "message_embedded"}),
      FieldCheckResults::kPartial);

  EXPECT_EQ(CheckField({"message_embedded", "message_embedded", "embedded2"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(CheckField({"message_embedded", "message_embedded", "embedded2",
                        "embedded3"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3",
                        "message_embedded"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(CheckField({"message_embedded", "message_embedded", "embedded2",
                        "embedded3", "message_embedded"}),
            FieldCheckResults::kPartial);

  // Exclude
  EXPECT_EQ(
      CheckField({"message_embedded", "message_embedded", "singular_float"}),
      FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"message_embedded", "message_embedded",
                        "message_embedded", "singular_float"}),
            FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3",
                        "message_embedded", "singular_double"}),
            FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"message_embedded", "embedded2", "embedded3",
                        "message_embedded", "embedded2", "wkt_uint32"}),
            FieldCheckResults::kExclude);
}

TEST_F(CloudAuditLogFieldCheckerTest, CyclicPathTestsLeafAsMap) {
  ASSERT_OK(AddOrIntersectPaths({"message_embedded.map_embedded2"}));

  EXPECT_EQ(CheckField({"message_embedded", "map_embedded2", "embedded3"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded"}), FieldCheckResults::kPartial);
  EXPECT_EQ(
      CheckField({"message_embedded", "message_embedded", "map_embedded2"}),
      FieldCheckResults::kExclude);
}

TEST_F(CloudAuditLogFieldCheckerTest, CyclicPathTestsNonLaefAsMap) {
  ASSERT_OK(AddOrIntersectPaths({"map_embedded_message.message_embedded",
                                 "map_embedded_message.embedded2.embedded3",
                                 "singular_int64"}));

  // Include
  EXPECT_EQ(CheckField({"map_embedded_message", "message_embedded",
                        "message_embedded", "embedded2", "embedded3"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"map_embedded_message", "message_embedded",
                        "message_embedded", "embedded2", "embedded3",
                        "repeated_string"}),
            FieldCheckResults::kInclude);

  // Partial
  EXPECT_EQ(CheckField({"map_embedded_message", "message_embedded",
                        "message_embedded"}),
            FieldCheckResults::kPartial);
  EXPECT_EQ(CheckField({"map_embedded_message", "message_embedded",
                        "message_embedded", "embedded2"}),
            FieldCheckResults::kPartial);

  // Exclude
  EXPECT_EQ(CheckField({"map_embedded_message", "message_embedded",
                        "message_embedded", "singular_int64"}),
            FieldCheckResults::kExclude);
  EXPECT_EQ(CheckField({"map_embedded_message", "message_embedded",
                        "message_embedded", "embedded2", "singular_int64"}),
            FieldCheckResults::kExclude);
}

TEST_F(CloudAuditLogFieldCheckerTest, NoPathsAdded) {
  EXPECT_EQ(CheckField({"singular_double"}), FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded"}), FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"map_embedded_message"}), FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"message_embedded", "map_embedded2"}),
            FieldCheckResults::kInclude);
  EXPECT_EQ(CheckField({"map_embedded_message", "map_embedded2"}),
            FieldCheckResults::kInclude);
}

TEST_F(CloudAuditLogFieldCheckerTest, BrokenFieldChecker) {
  ASSERT_THAT(AddOrIntersectPaths({"map_embedded_unknown.message_embedded"}),
              StatusIs(
                absl::StatusCode::kInvalidArgument
                  ));
}

TEST_F(CloudAuditLogFieldCheckerTest, CheckType) {
  EXPECT_EQ(cloud_audit_log_field_checker()->CheckType(nullptr),
            FieldCheckResults::kInclude);

  EXPECT_EQ(cloud_audit_log_field_checker()->CheckType(root_type()),
            FieldCheckResults::kInclude);
}

}  // namespace testing
}  // namespace proto_processing_lib::proto_scrubber
