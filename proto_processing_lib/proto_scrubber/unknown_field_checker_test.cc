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

#include "proto_processing_lib/proto_scrubber/unknown_field_checker.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace proto_processing_lib::proto_scrubber::testing {

// Test peer that allows constructing the unknown field checker without using
// the statically initialized default object.
class UnknownFieldCheckerTestPeer {
 public:
  UnknownFieldCheckerTestPeer() {}

  const UnknownFieldChecker checker_;
};

namespace {

using ::google::protobuf::field_extraction::testing::TypeHelper;

class UnknownFieldCheckerTest : public ::testing::Test {
 protected:
  UnknownFieldCheckerTest()
  {}

  // Constructor and `SetUp` are separated on purpose for lazy initialization.
  void SetUp() override {
    std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/field_mask_test_proto_descriptor.pb");

    auto status = TypeHelper::Create(descriptor_path);
    ASSERT_OK(status);
    type_helper_ = std::move(status.value());

    peer_ = std::make_unique<UnknownFieldCheckerTestPeer>();
    unknown_field_checker_ = &(peer_->checker_);

    checker_max_depth_ = 100;
  }

  // A helper function to find a pointer to the type of a type url.
  const google::protobuf::Type* FindType(absl::string_view type_url) {
    absl::StatusOr<const google::protobuf::Type*> result =
        type_helper_->ResolveTypeUrl(type_url);
    EXPECT_OK(result.status());
    return result.value();
  }

  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;

  // Need to use a test peer that is constructed when the test runs to prevent
  // premature caching of flag values.
  std::unique_ptr<UnknownFieldCheckerTestPeer> peer_;
  const UnknownFieldChecker* unknown_field_checker_;

  // Used to test recursion limit.
  int checker_max_depth_;
};

TEST_F(UnknownFieldCheckerTest, NonNullFieldReturnsIncluded) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field field = type->fields(0);
  ASSERT_GT(type->fields_size(), 0);
  EXPECT_EQ(unknown_field_checker_->CheckField({field.name()}, &field),
            FieldCheckResults::kInclude);
}

TEST_F(UnknownFieldCheckerTest, MessageFieldReturnsInclude) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field field = type->fields(0);  // message_embedded.
  EXPECT_EQ(unknown_field_checker_->CheckField({field.name()}, &field),
            FieldCheckResults::kInclude);
}

TEST_F(UnknownFieldCheckerTest, NullFieldReturnsExcluded) {
  EXPECT_EQ(unknown_field_checker_->CheckField({"message_int64"}, nullptr),
            FieldCheckResults::kExclude);

  EXPECT_EQ(unknown_field_checker_->CheckField({"message_int64"}, nullptr,
                                               checker_max_depth_),
            FieldCheckResults::kExclude);
}

TEST_F(UnknownFieldCheckerTest, FieldPathDoesNotMatterInResults) {
  EXPECT_EQ(unknown_field_checker_->CheckField({""}, nullptr),
            FieldCheckResults::kExclude);
}

TEST_F(UnknownFieldCheckerTest, MessageFieldReturnsPartial) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field field = type->fields(18);  // message_embedded.
  EXPECT_EQ(unknown_field_checker_->CheckField({field.name()}, &field),
            FieldCheckResults::kPartial);
}

TEST_F(UnknownFieldCheckerTest, StructReturnsInclude) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field struct_field = type->fields(32);  // message_struct.
  EXPECT_EQ(
      unknown_field_checker_->CheckField({struct_field.name()}, &struct_field),
      FieldCheckResults::kInclude);

  google::protobuf::Field value_field = type->fields(33);  // struct_value.
  EXPECT_EQ(
      unknown_field_checker_->CheckField({value_field.name()}, &value_field),
      FieldCheckResults::kInclude);

  google::protobuf::Field list_field = type->fields(34);  // struct_list_value
  EXPECT_EQ(
      unknown_field_checker_->CheckField({list_field.name()}, &list_field),
      FieldCheckResults::kInclude);
}

TEST_F(UnknownFieldCheckerTest, WellKnownTypeReturnsInclude) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field timestamp_field = type->fields(19);  // timestamp.
  EXPECT_EQ(unknown_field_checker_->CheckField({timestamp_field.name()},
                                               &timestamp_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field duration_field = type->fields(20);  // duration.
  EXPECT_EQ(unknown_field_checker_->CheckField({duration_field.name()},
                                               &duration_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field double_value_field = type->fields(21);  // DoubleValue
  EXPECT_EQ(unknown_field_checker_->CheckField({double_value_field.name()},
                                               &double_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field float_value_field = type->fields(22);  // FloatValue
  EXPECT_EQ(unknown_field_checker_->CheckField({float_value_field.name()},
                                               &float_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field int64_value_field = type->fields(23);  // Int64Value
  EXPECT_EQ(unknown_field_checker_->CheckField({int64_value_field.name()},
                                               &int64_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field uint64_value_field = type->fields(24);  // UInt64Value
  EXPECT_EQ(unknown_field_checker_->CheckField({uint64_value_field.name()},
                                               &uint64_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field int32_value_field = type->fields(25);  // Int32Value
  EXPECT_EQ(unknown_field_checker_->CheckField({int32_value_field.name()},
                                               &int32_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field uint32_value_field = type->fields(26);  // UInt32Value
  EXPECT_EQ(unknown_field_checker_->CheckField({uint32_value_field.name()},
                                               &uint32_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field bool_value_field = type->fields(27);  // BoolValue
  EXPECT_EQ(unknown_field_checker_->CheckField({bool_value_field.name()},
                                               &bool_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field string_value_field = type->fields(28);  // StringValue
  EXPECT_EQ(unknown_field_checker_->CheckField({string_value_field.name()},
                                               &string_value_field),
            FieldCheckResults::kInclude);

  google::protobuf::Field bytes_value_field = type->fields(29);  // BytesValue
  EXPECT_EQ(unknown_field_checker_->CheckField({bytes_value_field.name()},
                                               &bytes_value_field),
            FieldCheckResults::kInclude);
}

TEST_F(UnknownFieldCheckerTest, StackOverflowProtection) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field field = type->fields(18);  // message_embedded.
  std::vector<std::string> path;

  // Under recursion limit.
  for (int i = 0; i < checker_max_depth_ - 1; ++i) {
    path.push_back(field.name());
  }
  EXPECT_EQ(unknown_field_checker_->CheckField(path, &field, path.size()),
            FieldCheckResults::kPartial);

  // Over recursion limit.
  path.push_back(field.name());
  EXPECT_EQ(unknown_field_checker_->CheckField(path, &field, path.size()),
            FieldCheckResults::kInclude);
}

TEST_F(UnknownFieldCheckerTest, PathDoesNotMatterForStackOverflowProtection) {
  const google::protobuf::Type* type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  google::protobuf::Field field = type->fields(18);  // message_embedded.

  // Under recursion limit.
  EXPECT_EQ(
      unknown_field_checker_->CheckField({""}, &field, checker_max_depth_ - 1),
      FieldCheckResults::kPartial);

  // Over recursion limit.
  EXPECT_EQ(
      unknown_field_checker_->CheckField({""}, &field, checker_max_depth_),
      FieldCheckResults::kInclude);
}

}  // namespace
}  // namespace proto_processing_lib::proto_scrubber::testing
