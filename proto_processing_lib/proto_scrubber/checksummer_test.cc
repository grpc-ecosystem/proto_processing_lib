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

#include "proto_processing_lib/proto_scrubber/checksummer.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/crc/crc32c.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "proto_processing_lib/proto_scrubber/testdata/checksummer_test.pb.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace proto_processing_lib::proto_scrubber {

namespace testing {

using ::google::protobuf::field_extraction::CordMessageData;
using ::google::protobuf::field_extraction::testing::GetContents;
using ::google::protobuf::field_extraction::testing::GetTextProto;
using ::google::protobuf::field_extraction::testing::TypeHelper;
using ::proto_processing_lib::scrubber::testing::checksummer::
    ChecksummerTestEmbeddedMessage;
using ::proto_processing_lib::scrubber::testing::checksummer::
    ChecksummerTestEnum;
using ::proto_processing_lib::scrubber::testing::checksummer::
    ChecksummerTestMessage;
using ::proto_processing_lib::scrubber::testing::checksummer::MapValue;
using ::proto_processing_lib::scrubber::testing::checksummer::
    ThirdLevelEmbeddedMessage;
using ::ocpdiag::testing::IsOkAndHolds;

class ChecksummerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/"
        "checksummer_test_proto_descriptor.pb");

    absl::StatusOr<std::unique_ptr<TypeHelper>> status;
    ASSERT_OK_AND_ASSIGN(status, TypeHelper::Create(descriptor_path));
    type_helper_ = std::move(status.value());

    root_type_ = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.checksummer."
        "ChecksummerTestMessage");
    EXPECT_NE(root_type_, nullptr) << "Failed to find root type";
    checker_max_depth_ = 10;
    checksummer_ = Checksummer::GetDefault(checker_max_depth_);
  }

  void AssertResultAsExpected(const ChecksummerTestMessage& input_message,
                              uint32_t expected_checksum) const {
    CordMessageData result_data(input_message.SerializeAsCord());
    auto status_or_checksum = checksummer_->CalculateChecksum(
        &result_data, root_type_,
        [this](absl::string_view type) { return FindType(type); });
    EXPECT_THAT(status_or_checksum, IsOkAndHolds(expected_checksum));
  }

  uint32_t CalculateChecksum(const ChecksummerTestMessage& input_message) {
    CordMessageData result_data(input_message.SerializeAsCord());
    auto status_or_checksum = checksummer_->CalculateChecksum(
        &result_data, root_type_,
        [this](absl::string_view type) { return FindType(type); });
    EXPECT_OK(status_or_checksum);
    return status_or_checksum.value();
  }

  uint32_t AddChecksum(uint32_t pre, absl::string_view msg,
                       absl::string_view tags) {
    uint32_t expected_checksum =
        static_cast<uint32_t>(absl::ComputeCrc32c(tags));
    expected_checksum = static_cast<uint32_t>(
        absl::ExtendCrc32c(absl::crc32c_t{expected_checksum}, msg));
    return pre ^ expected_checksum;
  }

  // Tries to find the Type for `type_url`.
  const google::protobuf::Type* FindType(absl::string_view type_url) const {
    absl::StatusOr<const google::protobuf::Type*> type_status_or =
        type_helper_->ResolveTypeUrl(type_url);
    if (type_status_or.ok()) {
      return type_status_or.value();
    }
    return nullptr;
  }

  // Construct a nested test message.
  ChecksummerTestMessage CreateNestedTestMessage(int depth) {
    ChecksummerTestMessage message;
    message.set_message_string("nestedMessage");
    message.add_repeated_uint64(6452);

    auto nested_message = std::make_unique<ChecksummerTestEmbeddedMessage>();
    nested_message->set_message_bytes("nested_message");

    for (int i = 0; i < depth; i++) {
      auto local_nested_message =
          std::make_unique<ChecksummerTestEmbeddedMessage>();
      local_nested_message->set_message_bytes("nested_message");
      local_nested_message->set_message_int32(123);
      local_nested_message->set_message_bool(true);
      *(local_nested_message->add_message_embedded()) = *nested_message;
      nested_message = std::move(local_nested_message);
    }

    *(message.mutable_message_embedded()) = *nested_message;
    return message;
  }

  // Construct a nested test message with Any fields.
  ChecksummerTestMessage CreateNestedAnyTestMessage(int depth) {
    ChecksummerTestMessage message;
    message.set_message_string("nestedAnyMessage");
    message.add_repeated_uint64(6452);

    auto nested_any_message =
        std::make_unique<ChecksummerTestEmbeddedMessage>();
    nested_any_message->set_message_bytes("nested_any_message");

    for (int i = 0; i < depth / 2; i++) {
      auto local_nested_any_message =
          std::make_unique<ChecksummerTestEmbeddedMessage>();
      local_nested_any_message->set_message_bytes("nested_any_message");
      local_nested_any_message->set_message_bool(true);
      local_nested_any_message->mutable_message_any()->PackFrom(
          *nested_any_message);
      nested_any_message = std::move(local_nested_any_message);
    }

    message.mutable_message_any()->PackFrom(*nested_any_message);
    return message;
  }

  int checker_max_depth_;

 private:
  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
  // The root type of the message.
  const google::protobuf::Type* root_type_;

  const Checksummer* checksummer_;
};

TEST_F(ChecksummerTest, BasicStringTest) {
  ChecksummerTestMessage input_message;
  std::string msg = "test string";
  std::string tags = "66";
  input_message.set_message_string(msg);
  AssertResultAsExpected(input_message, AddChecksum(0, msg, tags));
}

TEST_F(ChecksummerTest, EmbeddedStringTest) {
  ChecksummerTestMessage input_message;
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string msg = "embedded string";
  message_embedded->set_message_string(msg);
  std::string tags = "154.66";
  AssertResultAsExpected(input_message, AddChecksum(0, msg, tags));
}

TEST_F(ChecksummerTest, SubEmbeddedStringTest) {
  ChecksummerTestMessage input_message;
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string msg = "embedded2";
  std::string tags = "154.258.18";
  auto* embedded2 = message_embedded->mutable_embedded2();
  embedded2->set_message_string(msg);
  AssertResultAsExpected(input_message, AddChecksum(0, msg, tags));
}

TEST_F(ChecksummerTest, BasicInt32Test) {
  ChecksummerTestMessage input_message;
  int32_t msg = 123456;
  std::string tags = "8";
  input_message.set_message_int32(msg);
  AssertResultAsExpected(input_message, AddChecksum(0, "123456", tags));
}

TEST_F(ChecksummerTest, EmbeddedInt32Test) {
  ChecksummerTestMessage input_message;
  auto* message_embedded = input_message.mutable_message_embedded();
  int64_t msg = 123456;
  std::string tags = "154.16";
  message_embedded->set_message_int64(msg);
  AssertResultAsExpected(input_message, AddChecksum(0, "123456", tags));
}

TEST_F(ChecksummerTest, Fixed32Test) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "293";
  input_message.set_message_fixed32(3210);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.269";
  message_embedded->set_message_fixed32(3211);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "3211", tag2);
  expected_checksum = AddChecksum(expected_checksum, "3210", tag1);

  // To make sure that the order of fields doesn't matter.
  uint32_t expected_checksum2 = 0;
  expected_checksum2 = AddChecksum(expected_checksum2, "3210", tag1);
  expected_checksum2 = AddChecksum(expected_checksum2, "3211", tag2);

  AssertResultAsExpected(input_message, expected_checksum);
  AssertResultAsExpected(input_message, expected_checksum2);
}

TEST_F(ChecksummerTest, Fixed64Test) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "337";
  input_message.set_message_fixed64(6410);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.313";
  message_embedded->set_message_fixed64(6411);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "6411", tag2);
  expected_checksum = AddChecksum(expected_checksum, "6410", tag1);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, DoubleTest) {
  ChecksummerTestMessage input_message;
  double msg1 = 1.234L;
  std::string tag1 = "41";
  input_message.set_message_double(msg1);
  auto* message_embedded = input_message.mutable_message_embedded();
  double msg2 = 95.89L;
  std::string tag2 = "154.41";
  message_embedded->set_message_double(msg2);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "1.234", tag1);
  expected_checksum = AddChecksum(expected_checksum, "95.89", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, FloatTest) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "53";
  input_message.set_message_float(32.1f);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.41";
  message_embedded->set_message_double(10.0f);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "32.1", tag1);
  expected_checksum = AddChecksum(expected_checksum, "10", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, BytesTest) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "298";
  input_message.set_message_bytes("some bytes");

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "some bytes", tag1);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, BoolTest) {
  ChecksummerTestMessage input_message;
  // Default value for bool is false, which will be neglected for checksum.
  input_message.set_message_bool(false);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.56";
  message_embedded->set_message_bool(true);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "true", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, EnumTest) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "72";
  input_message.set_message_enum(ChecksummerTestEnum::PENUM3);
  auto* message_embedded = input_message.mutable_message_embedded();
  // PENUM1 is the default value, which is neglected for checksum.
  message_embedded->set_message_enum(ChecksummerTestEnum::PENUM1);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "2", tag1);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, Uint32Test) {
  ChecksummerTestMessage input_message;
  uint32_t msg = 666;
  std::string tag1 = "24";
  input_message.set_message_uint32(msg);
  auto* message_embedded = input_message.mutable_message_embedded();
  uint32_t msg2 = 1234;
  std::string tag2 = "154.24";
  message_embedded->set_message_uint32(msg2);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "666", tag1);
  expected_checksum = AddChecksum(expected_checksum, "1234", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, Uint64Test) {
  ChecksummerTestMessage input_message;
  uint64_t msg = 6464;
  std::string tag1 = "32";
  input_message.set_message_uint64(msg);
  auto* message_embedded = input_message.mutable_message_embedded();
  uint64_t msg2 = 4646;
  std::string tag2 = "154.32";
  message_embedded->set_message_uint64(msg2);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "6464", tag1);
  expected_checksum = AddChecksum(expected_checksum, "4646", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, SInt32Test) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "304";
  input_message.set_message_sint32(3207);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.280";
  message_embedded->set_message_sint32(3208);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "3208", tag2);
  expected_checksum = AddChecksum(expected_checksum, "3207", tag1);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, SInt64Test) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "312";
  input_message.set_message_sint64(6417);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.288";
  message_embedded->set_message_sint64(6418);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "6418", tag2);
  expected_checksum = AddChecksum(expected_checksum, "6417", tag1);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, SFixed32Test) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "325";
  input_message.set_message_sfixed32(3210);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.301";
  message_embedded->set_message_sfixed32(3211);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "3210", tag1);
  expected_checksum = AddChecksum(expected_checksum, "3211", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, SFixed64Test) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "329";
  input_message.set_message_sfixed64(6410);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.305";
  message_embedded->set_message_sfixed64(6411);

  uint32_t expected_checksum = 0;
  // Reversed order.
  expected_checksum = AddChecksum(expected_checksum, "6410", tag1);
  expected_checksum = AddChecksum(expected_checksum, "6411", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, TimestampTest) {
  ChecksummerTestMessage input_message;
  auto* ts = input_message.mutable_timestamp();
  ts->set_seconds(123456);
  ts->set_nanos(0);
  AssertResultAsExpected(input_message, AddChecksum(0, "123456", "162.8"));
}

TEST_F(ChecksummerTest, TimestampNoNanosTest) {
  ChecksummerTestMessage input_message;
  auto* ts = input_message.mutable_timestamp();
  ts->set_seconds(46852);
  AssertResultAsExpected(input_message, AddChecksum(0, "46852", "162.8"));
}

TEST_F(ChecksummerTest, DurationTest) {
  ChecksummerTestMessage input_message;
  auto* dur = input_message.mutable_duration();
  dur->set_seconds(0);
  dur->set_nanos(45678);
  AssertResultAsExpected(input_message, AddChecksum(0, "45678", "170.16"));
}

TEST_F(ChecksummerTest, DurationNoSecondsTest) {
  ChecksummerTestMessage input_message;
  auto* dur = input_message.mutable_duration();
  dur->set_nanos(123455);
  AssertResultAsExpected(input_message, AddChecksum(0, "123455", "170.16"));
}

TEST_F(ChecksummerTest, MixedTest) {
  ChecksummerTestMessage input_message;
  std::string msg1 = "test string";
  std::string tag1 = "66";
  input_message.set_message_string(msg1);
  auto* message_embedded = input_message.mutable_message_embedded();
  int64_t msg2 = 1234;
  std::string tag2 = "154.16";
  message_embedded->set_message_int64(msg2);

  int64_t msg3 = 9876;
  std::string tag3 = "8";
  input_message.set_message_int32(msg3);

  auto* embedded2 = message_embedded->mutable_embedded2();
  std::string msg4 = "embedded2";
  std::string tag4 = "154.258.18";
  embedded2->set_message_string(msg4);

  uint32_t expected_1 = 0;
  expected_1 = AddChecksum(expected_1, msg1, tag1);
  expected_1 = AddChecksum(expected_1, "1234", tag2);
  expected_1 = AddChecksum(expected_1, "9876", tag3);
  expected_1 = AddChecksum(expected_1, msg4, tag4);
  AssertResultAsExpected(input_message, expected_1);
  // Another random order, to make sure the order of fields doesn't matter.
  uint32_t expected_2 = 0;
  expected_2 = AddChecksum(expected_2, msg4, tag4);
  expected_2 = AddChecksum(expected_2, msg1, tag1);
  expected_2 = AddChecksum(expected_2, "9876", tag3);
  expected_2 = AddChecksum(expected_2, "1234", tag2);
  AssertResultAsExpected(input_message, expected_2);
}

TEST_F(ChecksummerTest, RepeatedFieldTest) {
  ChecksummerTestMessage input_message;
  // Unpacked.
  std::string tag1 = "138";
  input_message.add_repeated_string("repeated1");
  input_message.add_repeated_string("repeated2");

  // Packed.
  std::string tag2 = "128";
  input_message.add_repeated_bool(true);
  input_message.add_repeated_bool(false);

  // Packed.
  std::string tag3 = "144";
  input_message.add_repeated_enum((ChecksummerTestEnum::PENUM3));
  input_message.add_repeated_enum((ChecksummerTestEnum::PENUM2));
  input_message.add_repeated_enum((ChecksummerTestEnum::PENUM1));

  auto* message_embedded = input_message.mutable_message_embedded();
  // Packed.
  std::string tag4 = "154.80";
  message_embedded->add_repeated_int32(3221);
  message_embedded->add_repeated_int32(3222);

  auto* embedded2 = message_embedded->mutable_embedded2();
  std::string tag5 = "154.258.26.18";
  auto* embedded3 = embedded2->add_embedded3();
  embedded3->add_repeated_string("subembeded");
  embedded3->add_repeated_string("subembeded2");

  std::string tag6 = "154.154.144";
  ChecksummerTestEmbeddedMessage* repeated1 =
      message_embedded->add_message_embedded();
  repeated1->add_repeated_enum((ChecksummerTestEnum::PENUM3));

  // Packed.
  std::string tag7 = "154.154.104";
  ChecksummerTestEmbeddedMessage* repeated2 =
      message_embedded->add_message_embedded();
  repeated2->add_repeated_uint64(6412);

  std::string tag8 = "154.154.154.138";
  ChecksummerTestEmbeddedMessage* repeated3 = repeated1->add_message_embedded();
  repeated3->add_repeated_string("embedded repeated");

  // Packed float.
  std::string tag9 = "154.125";
  message_embedded->add_repeated_float(3.4567);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "repeated1", tag1);
  expected_checksum = AddChecksum(expected_checksum, "repeated2", tag1);
  expected_checksum = AddChecksum(expected_checksum, "true", tag2);
  expected_checksum = AddChecksum(expected_checksum, "false", tag2);
  expected_checksum = AddChecksum(expected_checksum, "0", tag3);
  expected_checksum = AddChecksum(expected_checksum, "1", tag3);
  expected_checksum = AddChecksum(expected_checksum, "2", tag3);
  expected_checksum = AddChecksum(expected_checksum, "3221", tag4);
  expected_checksum = AddChecksum(expected_checksum, "3222", tag4);
  expected_checksum = AddChecksum(expected_checksum, "subembeded", tag5);
  expected_checksum = AddChecksum(expected_checksum, "subembeded2", tag5);
  expected_checksum = AddChecksum(expected_checksum, "2", tag6);
  expected_checksum = AddChecksum(expected_checksum, "6412", tag7);
  expected_checksum = AddChecksum(expected_checksum, "embedded repeated", tag8);
  expected_checksum = AddChecksum(expected_checksum, "3.4567", tag9);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, SeparatedRepeatedFieldsTest) {
  ChecksummerTestMessage input_message_1;
  std::string tag1 = "138";
  input_message_1.add_repeated_string("repeated1");
  input_message_1.add_repeated_string("repeated2");
  std::string tag2 = "66";
  input_message_1.set_message_string("repeated string 1");
  std::string serialized_checksum_1 = input_message_1.SerializeAsString();

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "repeated1", tag1);
  expected_checksum = AddChecksum(expected_checksum, "repeated2", tag1);
  expected_checksum = AddChecksum(expected_checksum, "repeated string 1", tag2);
  AssertResultAsExpected(input_message_1, expected_checksum);

  ChecksummerTestMessage input_message_2;
  input_message_2.add_repeated_string("repeated3");
  input_message_2.add_repeated_string("repeated4");
  input_message_2.MergeFrom(input_message_1);

  expected_checksum = AddChecksum(expected_checksum, "repeated3", tag1);
  expected_checksum = AddChecksum(expected_checksum, "repeated4", tag1);
  AssertResultAsExpected(input_message_2, expected_checksum);
}

TEST_F(ChecksummerTest, MapTest) {
  ChecksummerTestMessage input_message;
  auto* map_message = input_message.mutable_message_map();
  std::string tag1 = "258.10";
  map_message->set_map_id("map");

  std::string key_tag1 = "258.26.8";
  std::string value_tag1 = "258.26.16";
  (*map_message->mutable_primitive_map())[21] = 2121;
  (*map_message->mutable_primitive_map())[22] = 2222;

  MapValue map_value;
  std::string key_tag2 = "258.18.10";
  std::string value_tag2 = "258.18.18.10";
  map_value.add_value("map_value1");
  map_value.add_value("map_value2");
  (*map_message->mutable_properties())["property_key1"] = map_value;
  (*map_message->mutable_properties())["property_key2"] = map_value;

  ChecksummerTestEmbeddedMessage embedded1;
  std::string key_tag3 = "258.34.10";
  std::string value_tag3 = "258.34.18.269";
  embedded1.set_message_fixed32(3232);
  (*map_message->mutable_message_map())["message_map1"] = embedded1;

  ChecksummerTestEmbeddedMessage embedded2;
  std::string value_tag4 = "258.34.18.280";
  embedded2.set_message_sint32(3233);
  (*map_message->mutable_message_map())["message_map2"] = embedded2;

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "map", tag1);
  expected_checksum = AddChecksum(expected_checksum, "21", key_tag1);
  expected_checksum = AddChecksum(expected_checksum, "22", key_tag1);
  expected_checksum = AddChecksum(expected_checksum, "2121", value_tag1);
  expected_checksum = AddChecksum(expected_checksum, "2222", value_tag1);
  expected_checksum =
      AddChecksum(expected_checksum, "property_key1", value_tag1);
  expected_checksum =
      AddChecksum(expected_checksum, "property_key2", value_tag1);
  expected_checksum = AddChecksum(expected_checksum, "message_map1", key_tag3);
  expected_checksum = AddChecksum(expected_checksum, "3232", value_tag3);
  expected_checksum = AddChecksum(expected_checksum, "message_map2", key_tag3);
  expected_checksum = AddChecksum(expected_checksum, "3233", value_tag4);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, OneofTest) {
  ChecksummerTestMessage input_message;
  std::string tag1 = "352";
  input_message.set_int_data(1234);
  auto* message_embedded = input_message.mutable_message_embedded();
  std::string tag2 = "154.330";
  message_embedded->set_str_data("oneofString");

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "1234", tag1);
  expected_checksum = AddChecksum(expected_checksum, "oneofString", tag2);
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, AnyTest) {
  ChecksummerTestEmbeddedMessage any_message;
  const std::string tag1 = "154.346.18.330";
  any_message.set_str_data("oneofString");
  const std::string tag2 = "154.346.18.113";
  any_message.add_repeated_double(12.343);

  ChecksummerTestMessage input_message;
  input_message.mutable_message_embedded()->mutable_message_any()->PackFrom(
      any_message);
  const std::string tag3 = "154.346.10";
  const std::string value3 =
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.checksummer."
      "ChecksummerTestEmbeddedMessage";
  input_message.mutable_message_embedded()->set_extra_string("extra");
  const std::string tag4 = "154.354";
  input_message.mutable_message_embedded()->set_message_string("hello");
  const std::string tag5 = "154.66";

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "oneofString", tag1);
  expected_checksum = AddChecksum(expected_checksum, "12.343", tag2);
  expected_checksum = AddChecksum(expected_checksum, value3, tag3);
  expected_checksum = AddChecksum(expected_checksum, "extra", tag4);
  expected_checksum = AddChecksum(expected_checksum, "hello", tag5);
  AssertResultAsExpected(input_message, expected_checksum);

  ChecksummerTestEmbeddedMessage empty_any_message;
  ChecksummerTestMessage empty_any_input_message;
  empty_any_input_message.mutable_message_embedded()
      ->mutable_message_any()
      ->PackFrom(empty_any_message);
  expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, value3, tag3);
  AssertResultAsExpected(empty_any_input_message, expected_checksum);
}

TEST_F(ChecksummerTest, MapInsideAnyTest) {
  ChecksummerTestMessage input_message;
  input_message.set_message_sint32(3240);
  input_message.set_message_string("anyMessage");
  ThirdLevelEmbeddedMessage any_message;
  ChecksummerTestEmbeddedMessage* embedded = any_message.add_message_embedded();
  auto* map_message = embedded->mutable_maps();
  MapValue map_value;
  map_value.add_value("map_value1");
  (*map_message->mutable_properties())["property_key"] = map_value;

  ChecksummerTestEmbeddedMessage embedded1;
  std::string key_tag3 = "258.34.10";
  std::string value_tag3 = "258.34.18.269";
  embedded1.set_message_enum(ChecksummerTestEnum::PENUM2);

  (*map_message->mutable_message_map())["message_map1"] = embedded1;
  ChecksummerTestEmbeddedMessage embedded2;
  embedded2.set_message_sint32(3250);
  (*map_message->mutable_message_map())["message_map2"] = embedded2;
  input_message.mutable_message_any()->PackFrom(any_message);

  uint32_t expected_checksum = 0;
  expected_checksum =
      AddChecksum(expected_checksum, "map_value1", "362.18.34.250.18.18.10");
  expected_checksum =
      AddChecksum(expected_checksum, "message_map1", "362.18.34.250.34.10");
  expected_checksum =
      AddChecksum(expected_checksum, "3250", "362.18.34.250.34.18.280");
  expected_checksum =
      AddChecksum(expected_checksum, "message_map2", "362.18.34.250.34.10");
  expected_checksum =
      AddChecksum(expected_checksum, "1", "362.18.34.250.34.18.72");
  expected_checksum =
      AddChecksum(expected_checksum, "property_key", "362.18.34.250.18.10");
  expected_checksum = AddChecksum(expected_checksum, "anyMessage", "66");
  expected_checksum = AddChecksum(expected_checksum, "3240", "304");
  expected_checksum = AddChecksum(expected_checksum,
                                  "type.googleapis.com/"
                                  "proto_processing_lib.scrubber.testing."
                                  "checksummer.ThirdLevelEmbeddedMessage",
                                  "362.10");
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, NestedAnyTest) {
  ChecksummerTestMessage input_message;
  input_message.set_message_string("nestedAnyMessage");
  ChecksummerTestEmbeddedMessage any_message;
  any_message.add_repeated_uint64(6452);
  ChecksummerTestEmbeddedMessage nested_any_message;
  nested_any_message.set_message_bytes("nested_any_message");
  any_message.mutable_message_any()->PackFrom(nested_any_message);
  input_message.mutable_message_any()->PackFrom(any_message);

  uint32_t expected_checksum = 0;
  expected_checksum = AddChecksum(expected_checksum, "nestedAnyMessage", "66");
  expected_checksum = AddChecksum(expected_checksum, "6452", "362.18.104");
  expected_checksum =
      AddChecksum(expected_checksum, "nested_any_message", "362.18.346.18.274");
  expected_checksum = AddChecksum(expected_checksum,
                                  "type.googleapis.com/"
                                  "proto_processing_lib.scrubber.testing."
                                  "checksummer.ChecksummerTestEmbeddedMessage",
                                  "362.10");
  expected_checksum = AddChecksum(expected_checksum,
                                  "type.googleapis.com/"
                                  "proto_processing_lib.scrubber.testing."
                                  "checksummer.ChecksummerTestEmbeddedMessage",
                                  "362.18.346.10");
  AssertResultAsExpected(input_message, expected_checksum);
}

TEST_F(ChecksummerTest, DeeplyNestedMessageTest) {
  auto within_limit_message = CreateNestedTestMessage(checker_max_depth_ - 1);
  auto on_limit_message = CreateNestedTestMessage(checker_max_depth_);
  auto over_limit_message = CreateNestedTestMessage(checker_max_depth_ + 1);

  auto within_result = CalculateChecksum(within_limit_message);
  auto on_result = CalculateChecksum(on_limit_message);
  auto over_result = CalculateChecksum(over_limit_message);

  // The checksum results should be same for on/over nested limit depth message,
  // because the exceeding part will be skipped.
  EXPECT_EQ(on_result, over_result);
  // The checksum results should be different for within/over case.
  EXPECT_NE(within_result, over_result);
}

TEST_F(ChecksummerTest, DeeplyNestedAnyMessageTest) {
  // For the Any message, the actual data will be added within an Any
  // field, which means the depths will be double.
  auto within_limit_message =
      CreateNestedAnyTestMessage(checker_max_depth_ - 1);
  auto on_limit_message = CreateNestedAnyTestMessage(checker_max_depth_);
  auto over_limit_message = CreateNestedAnyTestMessage(checker_max_depth_ + 1);

  auto within_result = CalculateChecksum(within_limit_message);
  auto on_result = CalculateChecksum(on_limit_message);
  auto over_result = CalculateChecksum(over_limit_message);

  // The checksum results should be same for on/over nested limit depth message,
  // because the exceeding part will be skipped.
  EXPECT_EQ(on_result, over_result);
  // The checksum results should be different for within/over case.
  EXPECT_NE(within_result, over_result);
}

TEST_F(ChecksummerTest, ComprehensiveTest) {
  std::string input_file = GetTestDataFilePath(
      "proto_scrubber/testdata/checksummer_test_input.proto.txt");
  ChecksummerTestMessage input_message;
  ASSERT_OK(GetTextProto(input_file, &input_message));
  AssertResultAsExpected(input_message, 2217847532);
}

// The ChecksummerTestMessage used here is set by:
//   ChecksummerTestMessage r;
//   r.set_message_string("hello");
//   r.set_unknown_string("unknown_message");
//   auto* message_embedded = r.mutable_message_embedded();
//   message_embedded->set_unknown_int32(3212);
TEST_F(ChecksummerTest, UnknownFieldsTest) {
  std::string content;
  std::string filepath = GetTestDataFilePath(
      "proto_scrubber/testdata/checksum_message_with_unknown.txt");
  absl::Status result = GetContents(filepath, &content);
  CHECK_OK(result);

  ChecksummerTestMessage input_message;
  input_message.ParseFromString(content);
  std::string msg = "hello";
  std::string tag = "66";
  AssertResultAsExpected(input_message, AddChecksum(0, msg, tag));
}

}  // namespace testing
}  // namespace proto_processing_lib::proto_scrubber
