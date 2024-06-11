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

#include "proto_processing_lib/proto_scrubber/proto_scrubber.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "ocpdiag/core/testing/parse_text_proto.h"
#include "third_party/proto_converter/src/field_mask_utility.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/crc/crc32c.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/cord_message_data.h"
#include "proto_field_extraction/test_utils/utils.h"
#include "proto_processing_lib/proto_scrubber/checksummer.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/field_mask_tree.h"
#include "proto_processing_lib/proto_scrubber/proto_scrubber_enums.h"
#include "proto_processing_lib/proto_scrubber/proto_scrubber_test_lib.h"
#include "proto_processing_lib/proto_scrubber/testdata/field_mask_test.pb.h"
#include "proto_processing_lib/proto_scrubber/utility.h"
#include "google/protobuf/text_format.h"
#include "ocpdiag/core/testing/proto_matchers.h"
#include "ocpdiag/core/testing/status_matchers.h"

namespace proto_processing_lib::proto_scrubber {
namespace testing {
namespace {

using ::google::protobuf::field_extraction::CordMessageData;
using ::google::protobuf::field_extraction::testing::GetTextProto;
using ::google::protobuf::field_extraction::testing::TypeHelper;
using ::google::protobuf::contrib::parse_proto::ParseTextProtoOrDie;
using ::google::protobuf::util::converter::DecodeCompactFieldMaskPaths;
using scrubber::testing::ScrubberTestMessage;
using ::testing::_;
using ::testing::Bool;
using ::ocpdiag::testing::EqualsProto;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrictMock;
using ::ocpdiag::testing::ParseTextProtoOrDie;

class FieldMaskScrubberTest : public ::testing::Test,
                              public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    const std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/"
        "field_mask_test_proto_descriptor.pb");
    absl::StatusOr<std::unique_ptr<TypeHelper>> status =
        TypeHelper::Create(descriptor_path);
    type_helper_ = std::move(status.value());

    const google::protobuf::Type* type = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
    EXPECT_NE(type, nullptr) << "Failed to find root type";

    tree_ = std::make_unique<FieldMaskTree>(
        type, [this](const std::string& type) { return FindType(type); });
    EXPECT_NE(tree_, nullptr) << "Failed to create tree";

    std::string input_file = GetTestDataFilePath(
        "proto_scrubber/testdata/field_mask_test_input.proto.txt");

    ASSERT_OK(GetTextProto(input_file, &input_message_))
        << "Failed to read input file";

    cord_message_data_ = CordMessageData(input_message_.SerializeAsCord());

    if (GetParam()) {
      auto status_or_checksum = Checksummer::GetDefault()->CalculateChecksum(
          &cord_message_data_, type,
          [this](const std::string& type) { return FindType(type); });
      ASSERT_OK(status_or_checksum);
      original_checksum_ = status_or_checksum.value();
    }
  }

  // Adds a FieldMask path to builder_.
  void AddPath(absl::string_view path) {
    ASSERT_OK(DecodeCompactFieldMaskPaths(path, [this](absl::string_view path) {
      paths_.push_back((std::string(path)));
      return ::absl::OkStatus();
    }));
  }

  // Tests if the result of scrubbing is expected.
  void AssertResultAsExpected(absl::string_view expected,
                              uint32_t remained_checksum) {
    const google::protobuf::Type* type = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");

    ASSERT_OK(tree_->AddOrIntersectFieldPaths(paths_));
    auto scrubber = std::make_unique<ProtoScrubber>(
        type, [this](const std::string& type) { return FindType(type); },
        std::vector<const FieldCheckerInterface*>{tree_.get()},
        ScrubberContext::kTestScrubbing);
    if (GetParam()) {
      auto scrub_result = scrubber->ScrubWithChecksum(&cord_message_data_);
      ASSERT_OK(scrub_result);
      // original_checksum_ is the checksum including all original fields.
      // remained_checksum is the checksum for remained fields, when XOR with
      // the scrubbed fields checksum, it should equal the
      // original_checksum_.
      EXPECT_EQ(remained_checksum ^ scrub_result.value(), original_checksum_);
    } else {
      auto scrub_result = scrubber->Scrub(&cord_message_data_);
      ASSERT_OK(scrub_result);
    }

    google::protobuf::TextFormat::Parser parser;
    ScrubberTestMessage expected_message;

    if (expected == "<ALL>") {
      expected_message = input_message_;
    } else {
      ASSERT_TRUE(parser.ParseFromString(expected, &expected_message));
    }
    ScrubberTestMessage result_message;
    result_message.ParseFromCord(cord_message_data_.Cord());

    EXPECT_THAT(result_message, EqualsProto(expected_message));
  }

  uint32_t CalChecksum(uint32_t pre, absl::string_view msg,
                       absl::string_view tags) {
    uint32_t expected_checksum =
        static_cast<uint32_t>(absl::ComputeCrc32c(tags));
    expected_checksum = static_cast<uint32_t>(
        absl::ExtendCrc32c(absl::crc32c_t{expected_checksum}, msg));
    return pre ^ expected_checksum;
  }

  const google::protobuf::Type* FindType(const std::string& type_url) {
    absl::StatusOr<const google::protobuf::Type*> result =
        type_helper_->ResolveTypeUrl(type_url);
    if (!result.ok()) {
      return nullptr;
    }
    return result.value();
  }

  FieldMaskTree* tree() { return tree_.get(); }

  CordMessageData& cord_message_data() { return cord_message_data_; }

  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;

  // Original Checksum for the whole message.
  uint32_t original_checksum_{};

 private:
  // A scrubber builder.
  std::unique_ptr<FieldMaskTree> tree_;
  // The result of scrubbing.
  CordMessageData cord_message_data_;
  // List of FieldMask paths that has been added.
  std::vector<std::string> paths_;
  // Original input message.
  ScrubberTestMessage input_message_;
};

INSTANTIATE_TEST_SUITE_P(EnableChecksum, FieldMaskScrubberTest, (Bool()));

TEST_P(FieldMaskScrubberTest, Int32Field) {
  AddPath("message_int32");
  AssertResultAsExpected("message_int32: -32", CalChecksum(0, "-32", "8"));
}

TEST_P(FieldMaskScrubberTest, Int64Field) {
  AddPath("message_int64");
  AssertResultAsExpected("message_int64: -64", CalChecksum(0, "-64", "16"));
}

TEST_P(FieldMaskScrubberTest, BothInt32AndInt64) {
  AddPath("message_int32");
  AddPath("message_int64");
  uint32_t checksum = CalChecksum(0, "-32", "8");
  checksum = CalChecksum(checksum, "-64", "16");
  AssertResultAsExpected("message_int32: -32 \n message_int64: -64", checksum);
}

TEST_P(FieldMaskScrubberTest, Uint32Field) {
  AddPath("message_uint32");
  AssertResultAsExpected("message_uint32: 32", CalChecksum(0, "32", "24"));
}

TEST_P(FieldMaskScrubberTest, Uint64Field) {
  AddPath("message_uint64");
  AssertResultAsExpected("message_uint64: 64", CalChecksum(0, "64", "32"));
}

TEST_P(FieldMaskScrubberTest, DoubleField) {
  AddPath("message_double");
  AssertResultAsExpected("message_double: 1.234",
                         CalChecksum(0, "1.234", "41"));
}

TEST_P(FieldMaskScrubberTest, FloatField) {
  AddPath("message_float");
  AssertResultAsExpected("message_float: 0.567", CalChecksum(0, "0.567", "53"));
}

TEST_P(FieldMaskScrubberTest, BoolField) {
  AddPath("message_bool");
  AssertResultAsExpected("message_bool: true", CalChecksum(0, "true", "56"));
}

TEST_P(FieldMaskScrubberTest, StringField) {
  AddPath("message_string");
  AssertResultAsExpected("message_string: \"abc\"",
                         CalChecksum(0, "abc", "66"));
}

TEST_P(FieldMaskScrubberTest, EnumField) {
  AddPath("message_enum");
  AssertResultAsExpected("message_enum: PENUM2", CalChecksum(0, "1", "72"));
}

TEST_P(FieldMaskScrubberTest, RepeatedInt32Field) {
  AddPath("repeated_int32");
  uint32_t checksum = CalChecksum(0, "-1", "80");
  checksum = CalChecksum(checksum, "-2", "80");
  checksum = CalChecksum(checksum, "-33", "80");
  AssertResultAsExpected("repeated_int32: [-1, -2, -33]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedInt64Field) {
  AddPath("repeated_int64");
  uint32_t checksum = CalChecksum(0, "-11", "88");
  checksum = CalChecksum(checksum, "-12", "88");
  checksum = CalChecksum(checksum, "-133", "88");
  AssertResultAsExpected("repeated_int64: [-11, -12, -133]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedUint32Field) {
  AddPath("repeated_uint32");
  uint32_t checksum = CalChecksum(0, "1", "96");
  checksum = CalChecksum(checksum, "2", "96");
  checksum = CalChecksum(checksum, "33", "96");
  AssertResultAsExpected("repeated_uint32: [1, 2, 33]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedUint64Field) {
  AddPath("repeated_uint64");
  uint32_t checksum = CalChecksum(0, "11", "104");
  checksum = CalChecksum(checksum, "12", "104");
  checksum = CalChecksum(checksum, "133", "104");
  AssertResultAsExpected("repeated_uint64: [11, 12, 133]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedDoubleField) {
  AddPath("repeated_double");
  uint32_t checksum = CalChecksum(0, "1.234", "113");
  checksum = CalChecksum(checksum, "2.123", "113");
  checksum = CalChecksum(checksum, "1346", "113");
  AssertResultAsExpected("repeated_double: [1.234, 2.123, 1346]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedFloatField) {
  AddPath("repeated_float");
  uint32_t checksum = CalChecksum(0, "0.1234", "125");
  checksum = CalChecksum(checksum, "902.679", "125");
  // Note: GetTextProto limits the accuracy.
  AssertResultAsExpected("repeated_float: [0.1234, 902.67898]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedBoolField) {
  AddPath("repeated_bool");
  uint32_t checksum = CalChecksum(0, "true", "128");
  checksum = CalChecksum(checksum, "false", "128");
  AssertResultAsExpected("repeated_bool: [true, false]", checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedStringField) {
  AddPath("repeated_string");
  uint32_t checksum = CalChecksum(0, "dog", "138");
  checksum = CalChecksum(checksum, "eat", "138");
  checksum = CalChecksum(checksum, "bones", "138");
  AssertResultAsExpected("repeated_string: [\"dog\", \"eat\", \"bones\"]",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, RepeatedEnumField) {
  AddPath("repeated_enum");
  uint32_t checksum = CalChecksum(0, "1", "144");
  checksum = CalChecksum(checksum, "2", "144");
  checksum = CalChecksum(checksum, "0", "144");
  AssertResultAsExpected("repeated_enum: [PENUM2, PENUM3, PENUM1]", checksum);
}

TEST_P(FieldMaskScrubberTest, TimeStampField) {
  AddPath("timestamp");
  uint32_t checksum = CalChecksum(0, "1", "162.8");
  checksum = CalChecksum(checksum, "123", "162.16");
  AssertResultAsExpected("timestamp: {seconds: 1\n nanos: 123}", checksum);
}

TEST_P(FieldMaskScrubberTest, DurationField) {
  AddPath("duration");
  uint32_t checksum = CalChecksum(0, "2", "170.8");
  checksum = CalChecksum(checksum, "987", "170.16");
  AssertResultAsExpected("duration: {seconds: 2\n nanos: 987}", checksum);
}

TEST_P(FieldMaskScrubberTest, DoubleWrapperField) {
  AddPath("wkt_double");
  uint32_t checksum = CalChecksum(0, "12.345", "178.9");
  AssertResultAsExpected("wkt_double: { value: 12.345 }", checksum);
}

TEST_P(FieldMaskScrubberTest, FloatWrapperField) {
  AddPath("wkt_float");
  uint32_t checksum = CalChecksum(0, "10.145", "186.13");
  AssertResultAsExpected("wkt_float: { value: 10.145 }", checksum);
}

TEST_P(FieldMaskScrubberTest, Int64WrapperField) {
  AddPath("wkt_int64");
  uint32_t checksum = CalChecksum(0, "-2", "194.8");
  AssertResultAsExpected("wkt_int64: { value: -2 }", checksum);
}

TEST_P(FieldMaskScrubberTest, Uint64WrapperField) {
  AddPath("wkt_uint64");
  uint32_t checksum = CalChecksum(0, "3", "202.8");
  AssertResultAsExpected("wkt_uint64: { value: 3 }", checksum);
}

TEST_P(FieldMaskScrubberTest, Int32WrapperField) {
  AddPath("wkt_int32");
  uint32_t checksum = CalChecksum(0, "-4", "210.8");
  AssertResultAsExpected("wkt_int32: { value: -4 }", checksum);
}

TEST_P(FieldMaskScrubberTest, Uint32WrapperField) {
  AddPath("wkt_uint32");
  uint32_t checksum = CalChecksum(0, "5", "218.8");
  AssertResultAsExpected("wkt_uint32: { value: 5 }", checksum);
}

TEST_P(FieldMaskScrubberTest, BoolWrapperField) {
  AddPath("wkt_bool");
  uint32_t checksum = CalChecksum(0, "true", "226.8");
  AssertResultAsExpected("wkt_bool: { value: true }", checksum);
}

TEST_P(FieldMaskScrubberTest, StringWrapperField) {
  AddPath("wkt_string");
  uint32_t checksum = CalChecksum(0, "def", "234.10");
  AssertResultAsExpected("wkt_string: { value: \"def\" }", checksum);
}

TEST_P(FieldMaskScrubberTest, BytesWrapperField) {
  AddPath("wkt_bytes");
  uint32_t checksum = CalChecksum(0, "ddd", "242.10");
  AssertResultAsExpected("wkt_bytes: { value: \"ddd\" }", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedInt32Field) {
  AddPath("message_embedded.message_int32");
  uint32_t checksum = CalChecksum(0, "-32", "154.8");
  AssertResultAsExpected("message_embedded: {message_int32: -32}", checksum);
}

TEST_P(FieldMaskScrubberTest, BothEmbeddedAndNonEmbeddedInt32Fields) {
  AddPath("message_embedded.message_int32");
  AddPath("message_int32");
  uint32_t checksum = CalChecksum(0, "-32", "8");
  checksum = CalChecksum(checksum, "-32", "154.8");
  AssertResultAsExpected(
      "message_int32: -32 \n message_embedded: {message_int32: -32}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedInt64Field) {
  AddPath("message_embedded.message_int64");
  uint32_t checksum = CalChecksum(0, "-64", "154.16");
  AssertResultAsExpected("message_embedded: {message_int64: -64}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedBothInt32AndInt64) {
  AddPath("message_embedded.message_int32");
  AddPath("message_embedded.message_int64");
  uint32_t checksum = CalChecksum(0, "-64", "154.16");
  checksum = CalChecksum(checksum, "-32", "154.8");
  AssertResultAsExpected(
      "message_embedded: {message_int32: -32 \n message_int64: -64}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedUint32Field) {
  AddPath("message_embedded.message_uint32");
  uint32_t checksum = CalChecksum(0, "32", "154.24");
  AssertResultAsExpected("message_embedded: {message_uint32: 32}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedUint64Field) {
  AddPath("message_embedded.message_uint64");
  uint32_t checksum = CalChecksum(0, "64", "154.32");
  AssertResultAsExpected("message_embedded: {message_uint64: 64}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedDoubleField) {
  AddPath("message_embedded.message_double");
  uint32_t checksum = CalChecksum(0, "1.234", "154.41");
  AssertResultAsExpected("message_embedded: {message_double: 1.234}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedFloatField) {
  AddPath("message_embedded.message_float");
  uint32_t checksum = CalChecksum(0, "0.567", "154.53");
  AssertResultAsExpected("message_embedded: {message_float: 0.567}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedBoolField) {
  AddPath("message_embedded.message_bool");
  uint32_t checksum = CalChecksum(0, "true", "154.56");
  AssertResultAsExpected("message_embedded: {message_bool: true}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedStringField) {
  AddPath("message_embedded.message_string");
  uint32_t checksum = CalChecksum(0, "level 1", "154.66");
  AssertResultAsExpected("message_embedded: {message_string: \"level 1\"}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedEnumField) {
  AddPath("message_embedded.message_enum");
  uint32_t checksum = CalChecksum(0, "1", "154.72");
  AssertResultAsExpected("message_embedded: {message_enum: PENUM2}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedInt32Field) {
  AddPath("message_embedded.repeated_int32");
  uint32_t checksum = CalChecksum(0, "-1", "154.80");
  checksum = CalChecksum(checksum, "-2", "154.80");
  checksum = CalChecksum(checksum, "-33", "154.80");
  AssertResultAsExpected("message_embedded: {repeated_int32: [-1, -2, -33]}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedInt64Field) {
  AddPath("message_embedded.repeated_int64");
  uint32_t checksum = CalChecksum(0, "-11", "154.88");
  checksum = CalChecksum(checksum, "-12", "154.88");
  checksum = CalChecksum(checksum, "-133", "154.88");
  AssertResultAsExpected("message_embedded: {repeated_int64: [-11, -12, -133]}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedUint32Field) {
  AddPath("message_embedded.repeated_uint32");
  uint32_t checksum = CalChecksum(0, "1", "154.96");
  checksum = CalChecksum(checksum, "2", "154.96");
  checksum = CalChecksum(checksum, "33", "154.96");
  AssertResultAsExpected("message_embedded: {repeated_uint32: [1, 2, 33]}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedUint64Field) {
  AddPath("message_embedded.repeated_uint64");
  uint32_t checksum = CalChecksum(0, "11", "154.104");
  checksum = CalChecksum(checksum, "12", "154.104");
  checksum = CalChecksum(checksum, "133", "154.104");
  AssertResultAsExpected("message_embedded: {repeated_uint64: [11, 12, 133]}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedDoubleField) {
  AddPath("message_embedded.repeated_double");
  uint32_t checksum = CalChecksum(0, "1.234", "154.113");
  checksum = CalChecksum(checksum, "2.123", "154.113");
  checksum = CalChecksum(checksum, "1346", "154.113");
  AssertResultAsExpected(
      "message_embedded: {repeated_double: [1.234, 2.123, 1346]}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedFloatField) {
  AddPath("message_embedded.repeated_float");
  uint32_t checksum = CalChecksum(0, "0.1234", "154.125");
  checksum = CalChecksum(checksum, "902.679", "154.125");
  // Note: GetTextProto limits the accuracy.
  AssertResultAsExpected(
      "message_embedded: {repeated_float: [0.1234, 902.67898]}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedBoolField) {
  AddPath("message_embedded.repeated_bool");
  uint32_t checksum = CalChecksum(0, "true", "154.128");
  checksum = CalChecksum(checksum, "false", "154.128");
  AssertResultAsExpected("message_embedded: {repeated_bool: [true, false]}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedStringField) {
  AddPath("message_embedded.repeated_string");
  uint32_t checksum = CalChecksum(0, "dog", "154.138");
  checksum = CalChecksum(checksum, "eat", "154.138");
  checksum = CalChecksum(checksum, "bones", "154.138");
  AssertResultAsExpected(
      "message_embedded: {repeated_string: [\"dog\", \"eat\", \"bones\"]}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedRepeatedEnumField) {
  AddPath("message_embedded.repeated_enum");
  uint32_t checksum = CalChecksum(0, "1", "154.144");
  checksum = CalChecksum(checksum, "2", "154.144");
  checksum = CalChecksum(checksum, "0", "154.144");
  AssertResultAsExpected(
      "message_embedded: {repeated_enum: [PENUM2, PENUM3, PENUM1]}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedTimeStampField) {
  AddPath("message_embedded.timestamp");
  uint32_t checksum = CalChecksum(0, "1", "154.162.8");
  checksum = CalChecksum(checksum, "123", "154.162.16");
  AssertResultAsExpected(
      "message_embedded: {timestamp: {seconds: 1\n nanos: 123}}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedDurationField) {
  AddPath("message_embedded.duration");
  uint32_t checksum = CalChecksum(0, "2", "154.170.8");
  checksum = CalChecksum(checksum, "987", "154.170.16");
  AssertResultAsExpected(
      "message_embedded: {duration: {seconds: 2\n nanos: 987}}", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedDoubleWrapperField) {
  AddPath("message_embedded.wkt_double");
  uint32_t checksum = CalChecksum(0, "12.345", "154.178.9");
  AssertResultAsExpected("message_embedded: {wkt_double: { value: 12.345 }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedFloatWrapperField) {
  AddPath("message_embedded.wkt_float");
  uint32_t checksum = CalChecksum(0, "10.145", "154.186.13");
  AssertResultAsExpected("message_embedded: {wkt_float: { value: 10.145 }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedInt64WrapperField) {
  AddPath("message_embedded.wkt_int64");
  uint32_t checksum = CalChecksum(0, "-2", "154.194.8");
  AssertResultAsExpected("message_embedded: {wkt_int64: { value: -2 }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedUint64WrapperField) {
  AddPath("message_embedded.wkt_uint64");
  uint32_t checksum = CalChecksum(0, "3", "154.202.8");
  AssertResultAsExpected("message_embedded: {wkt_uint64: { value: 3 }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedInt32WrapperField) {
  AddPath("message_embedded.wkt_int32");
  uint32_t checksum = CalChecksum(0, "-4", "154.210.8");
  AssertResultAsExpected("message_embedded: {wkt_int32: { value: -4 }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedUint32WrapperField) {
  AddPath("message_embedded.wkt_uint32");
  uint32_t checksum = CalChecksum(0, "5", "154.218.8");
  AssertResultAsExpected("message_embedded: {wkt_uint32: { value: 5 }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedBoolWrapperField) {
  AddPath("message_embedded.wkt_bool");
  uint32_t checksum = CalChecksum(0, "true", "154.226.8");
  AssertResultAsExpected("message_embedded: {wkt_bool: { value: true }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedStringWrapperField) {
  AddPath("message_embedded.wkt_string");
  uint32_t checksum = CalChecksum(0, "def", "154.234.10");
  AssertResultAsExpected("message_embedded: {wkt_string: { value: \"def\" }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedBytesWrapperField) {
  AddPath("message_embedded.wkt_bytes");
  uint32_t checksum = CalChecksum(0, "ddd", "154.242.10");
  AssertResultAsExpected("message_embedded: {wkt_bytes: { value: \"ddd\" }}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, MultipleLevelEmbeddedField) {
  AddPath("message_string");
  AddPath("message_embedded.message_string");
  AddPath("message_embedded.embedded2.message_string");
  AddPath("message_embedded.embedded2.embedded3.repeated_string");

  uint32_t checksum = CalChecksum(0, "abc", "66");
  checksum = CalChecksum(checksum, "level 1", "154.66");
  checksum = CalChecksum(checksum, "level 2", "154.258.18");
  checksum = CalChecksum(checksum, "level 3a", "154.258.26.18");
  checksum = CalChecksum(checksum, "level 3b", "154.258.26.18");

  AssertResultAsExpected(
      "message_string: \"abc\"\n"
      "message_embedded: {\n"
      "  message_string: \"level 1\""
      "  embedded2: {\n"
      "    message_string: \"level 2\"\n"
      "    embedded3: {\n"
      "      repeated_string: [\"level 3a\"]\n"
      "    },\n"
      "    embedded3: {\n"
      "      repeated_string: [\"level 3b\"]\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, MultipleLevelRepeatedEmbeddedField) {
  AddPath("message_string");
  AddPath("message_embedded.message_string");
  AddPath("message_embedded.embedded2.message_string");
  AddPath("message_embedded.embedded2.embedded3.message_enum");

  uint32_t checksum = CalChecksum(0, "abc", "66");
  checksum = CalChecksum(checksum, "level 1", "154.66");
  checksum = CalChecksum(checksum, "level 2", "154.258.18");
  checksum = CalChecksum(checksum, "1", "154.258.26.24");

  AssertResultAsExpected(
      "message_string: \"abc\"\n message_embedded: {message_string: \"level "
      "1\"\n embedded2: {message_string: \"level 2\"\n "
      "embedded3: {}\n embedded3: {message_enum: PENUM2}}}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, UnmatchedEmbeddedMessageShouldBeFilteredOut) {
  AddPath("message_embedded.message_embedded.message_uint32");
  AddPath("message_int32");
  uint32_t checksum = CalChecksum(0, "-32", "8");
  AssertResultAsExpected("message_int32: -32\n message_embedded {}", checksum);
}

TEST_P(FieldMaskScrubberTest, NoFieldsMatchYieldEmptyMessage) {
  AddPath("another_int32");
  AssertResultAsExpected("", 0);
}

TEST_P(FieldMaskScrubberTest, NoMaskMeansNoFiltering) {
  AssertResultAsExpected("<ALL>", original_checksum_);
}

TEST_P(FieldMaskScrubberTest, AddingNonExistPathYieldError) {
  EXPECT_FALSE(
      tree()->AddOrIntersectFieldPaths({"message_embedded.nonexist"}).ok());
}

TEST_P(FieldMaskScrubberTest, CompactFormShouldWork) {
  AddPath(
      "message_embedded(message_int32,message_int64,repeated_string),wkt_int32,"
      "message_int32,duration,timestamp");

  uint32_t checksum = CalChecksum(0, "-32", "154.8");
  checksum = CalChecksum(checksum, "-64", "154.16");
  checksum = CalChecksum(checksum, "dog", "154.138");
  checksum = CalChecksum(checksum, "eat", "154.138");
  checksum = CalChecksum(checksum, "bones", "154.138");
  checksum = CalChecksum(checksum, "-4", "210.8");
  checksum = CalChecksum(checksum, "-32", "8");
  checksum = CalChecksum(checksum, "1", "162.8");
  checksum = CalChecksum(checksum, "123", "162.16");
  checksum = CalChecksum(checksum, "2", "170.8");
  checksum = CalChecksum(checksum, "987", "170.16");

  AssertResultAsExpected(
      "message_int32: -32\n"
      "message_embedded: {\n"
      "  message_int32: -32\n "
      "  message_int64: -64\n"
      "  repeated_string: [\"dog\", \"eat\", \"bones\"]\n"
      "}\n"
      "wkt_int32: { value: -4 }\n"
      "timestamp: {\n"
      "  seconds: 1\n"
      "  nanos: 123\n"
      "}\n"
      "duration: {\n"
      "  seconds: 2\n"
      "  nanos: 987\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, ParentShouldOverrideChildren) {
  // A path to a child field should have no effect if its parent is also added.
  AddPath("message_embedded.embedded2.embedded3.message_int32");
  AddPath("message_embedded.embedded2");

  uint32_t checksum = CalChecksum(0, "2", "154.258.8");
  checksum = CalChecksum(checksum, "level 2", "154.258.18");
  checksum = CalChecksum(checksum, "3", "154.258.26.8");
  checksum = CalChecksum(checksum, "level 3a", "154.258.26.18");
  checksum = CalChecksum(checksum, "1", "154.258.26.24");
  checksum = CalChecksum(checksum, "level 3b", "154.258.26.18");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  embedded2: {\n"
      "    message_int32: 2\n"
      "    message_string: \"level 2\"\n"
      "    embedded3: {\n"
      "      message_int32: 3\n"
      "      repeated_string: [\"level 3a\"]\n"
      "    },\n"
      "    embedded3: {\n"
      "      message_enum: PENUM2\n"
      "      repeated_string: [\"level 3b\"]\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, ResultCanBeScrubbedAgain) {
  AddPath(
      "message_embedded(message_int32,message_int64,repeated_string),wkt_int32,"
      "message_int32,duration,timestamp");

  uint32_t checksum = CalChecksum(0, "-32", "8");
  checksum = CalChecksum(checksum, "-32", "154.8");
  checksum = CalChecksum(checksum, "-64", "154.16");
  checksum = CalChecksum(checksum, "dog", "154.138");
  checksum = CalChecksum(checksum, "eat", "154.138");
  checksum = CalChecksum(checksum, "bones", "154.138");
  checksum = CalChecksum(checksum, "-4", "210.8");
  checksum = CalChecksum(checksum, "1", "162.8");
  checksum = CalChecksum(checksum, "123", "162.16");
  checksum = CalChecksum(checksum, "2", "170.8");
  checksum = CalChecksum(checksum, "987", "170.16");

  AssertResultAsExpected(
      "message_int32: -32\n"
      "message_embedded: {\n"
      "  message_int32: -32\n "
      "  message_int64: -64\n"
      "  repeated_string: [\"dog\", \"eat\", \"bones\"]\n"
      "}\n"
      "wkt_int32: { value: -4 }\n"
      "timestamp: {\n"
      "  seconds: 1\n"
      "  nanos: 123\n"
      "}\n"
      "duration: {\n"
      "  seconds: 2\n"
      "  nanos: 987\n"
      "}",
      checksum);

  const google::protobuf::Type* root_type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  std::unique_ptr<FieldMaskTree> tree(new FieldMaskTree(
      root_type, [this](const std::string& type) { return FindType(type); }));

  ASSERT_OK(tree->AddOrIntersectFieldPaths({"wkt_int32"}));
  auto scrubber = std::make_unique<ProtoScrubber>(
      root_type, [this](const std::string& type) { return FindType(type); },
      std::vector<const FieldCheckerInterface*>{tree.get()},
      ScrubberContext::kTestScrubbing);
  uint32_t dropped_checksum;
  if (GetParam()) {
    auto scrub_result = scrubber->ScrubWithChecksum(&cord_message_data());
    ASSERT_OK(scrub_result);
    dropped_checksum = scrub_result.value();
  } else {
    auto scrub_result = scrubber->Scrub(&cord_message_data());
    ASSERT_OK(scrub_result);
  }

  google::protobuf::TextFormat::Parser parser;
  ScrubberTestMessage expected_message;
  QCHECK(parser.ParseFromString("wkt_int32: { value: -4 }", &expected_message));
  ScrubberTestMessage result_message;
  result_message.ParseFromCord(cord_message_data().Cord());

  EXPECT_THAT(result_message, EqualsProto(expected_message));

  if (GetParam()) {
    uint32_t checksum_2 = CalChecksum(0, "-4", "210.8");
    EXPECT_EQ(checksum, checksum_2 ^ dropped_checksum);
  }
}

TEST_P(FieldMaskScrubberTest, UnknownFieldsShouldBeIncluded) {
  // Unknown fields are not included in checksum.
  if (GetParam()) {
    return;
  }
  AddPath("wkt_int32,");
  AssertResultAsExpected("wkt_int32: { value: -4 }\n", 0);

  const google::protobuf::Type* root_type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  std::unique_ptr<FieldMaskTree> tree(new FieldMaskTree(
      root_type, [this](const std::string& type) { return FindType(type); }));

  ASSERT_OK(tree->AddOrIntersectFieldPaths({"wkt_int32"}));
  google::protobuf::Type new_root_type = *root_type;
  new_root_type.clear_fields();
  auto scrubber = std::make_unique<ProtoScrubber>(
      &new_root_type,
      [this](const std::string& type) { return FindType(type); },
      std::vector<const FieldCheckerInterface*>{tree.get()},
      ScrubberContext::kTestScrubbing);
  if (GetParam()) {
    auto scrub_result = scrubber->ScrubWithChecksum(&cord_message_data());
    ASSERT_OK(scrub_result);
  } else {
    auto scrub_result = scrubber->Scrub(&cord_message_data());
    ASSERT_OK(scrub_result);
  }

  google::protobuf::TextFormat::Parser parser;
  ScrubberTestMessage expected_message;
  QCHECK(parser.ParseFromString("wkt_int32: { value: -4 }", &expected_message));
  ScrubberTestMessage result_message;
  result_message.ParseFromCord(cord_message_data().Cord());

  EXPECT_THAT(result_message, EqualsProto(expected_message));
}

TEST_P(FieldMaskScrubberTest, MapShouldBeIncludedCorrectly) {
  AddPath("message_embedded.maps.properties");
  uint32_t checksum = CalChecksum(0, "key_1", "154.250.18.10");
  checksum = CalChecksum(checksum, "New", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "England", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "Patriots", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "key_2", "154.250.18.10");
  checksum = CalChecksum(checksum, "Tom", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "Brady", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "Super", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "Bowl", "154.250.18.18.10");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    properties: {\n"
      "      key: \"key_1\"\n"
      "      value: {\n"
      "        value: [\"New\", \"England\", \"Patriots\"]\n"
      "      }\n"
      "    }\n"
      "    properties: {\n"
      "      key: \"key_2\"\n"
      "      value: {\n"
      "        value: [\"Tom\", \"Brady\"]\n"
      "      }\n"
      "    }\n"
      "    properties: {\n"
      "      key: \"\"\n"
      "      value: {\n"
      "        value: [\"Super\", \"Bowl\"]\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, MapShouldBeSkippedCorrectly) {
  AddPath("message_embedded.maps.map_id");
  uint32_t checksum = CalChecksum(0, "map test", "154.250.10");
  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    map_id: \"map test\"\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, MapKeyValueShouldBeHandledCorrectly) {
  AddPath("message_embedded.maps.properties[\"key_1\"]");
  uint32_t checksum = CalChecksum(0, "key_1", "154.250.18.10");
  checksum = CalChecksum(checksum, "New", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "England", "154.250.18.18.10");
  checksum = CalChecksum(checksum, "Patriots", "154.250.18.18.10");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    properties: {\n"
      "      key: \"key_1\"\n"
      "      value: {\n"
      "        value: [\"New\", \"England\", \"Patriots\"]\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, PrimitiveMapValueWorks) {
  AddPath(
      "message_embedded.maps.primitive_map[\"123\"],message_embedded.maps."
      "primitive_map[\"0\"],message_embedded.maps.primitive_map[\"12\"]");
  uint32_t checksum = CalChecksum(0, "123", "154.250.26.8");
  checksum = CalChecksum(checksum, "456", "154.250.26.16");
  checksum = CalChecksum(checksum, "12", "154.250.26.8");
  checksum = CalChecksum(checksum, "789", "154.250.26.16");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    primitive_map: {\n"
      "      key: 123"
      "      value: 456\n"
      "    }\n"
      "    primitive_map: {\n"
      "      key: 12"
      "      value: 789\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, NestedMapWorks) {
  AddPath(
      "message_embedded.maps.message_map[\"msg_key1\"],message_embedded.maps."
      "message_map[\"msg_key2\"].maps.properties[\"][{},!@#$%^&*()\\\"',<>?/"
      "\\\\\"]");
  uint32_t checksum = CalChecksum(0, "msg_key1", "154.250.34.10");
  checksum = CalChecksum(checksum, "-32", "154.250.34.18.8");
  checksum = CalChecksum(checksum, "-64", "154.250.34.18.16");
  checksum = CalChecksum(checksum, "nested map", "154.250.34.18.250.10");
  checksum = CalChecksum(checksum, "nested_key1", "154.250.34.18.250.18.10");
  checksum = CalChecksum(checksum, "Intercepted", "154.250.34.18.250.18.18.10");
  checksum = CalChecksum(checksum, "ball", "154.250.34.18.250.18.18.10");
  checksum = CalChecksum(checksum, "nested_key11", "154.250.34.18.250.18.10");
  checksum = CalChecksum(checksum, "Deflated", "154.250.34.18.250.18.18.10");
  checksum = CalChecksum(checksum, "ball", "154.250.34.18.250.18.18.10");

  checksum = CalChecksum(checksum, "msg_key2", "154.250.34.10");
  checksum = CalChecksum(checksum, "][{},!@#$%^&*()\"',<>?/\\",
                         "154.250.34.18.250.18.10");
  checksum = CalChecksum(checksum, "running", "154.250.34.18.250.18.18.10");
  checksum = CalChecksum(checksum, "back", "154.250.34.18.250.18.18.10");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    message_map: {\n"
      "      key: \"msg_key1\"\n"
      "      value: {\n"
      "        message_int32: -32\n"
      "        message_int64: -64\n"
      "        maps: {\n"
      "          map_id: \"nested map\"\n"
      "          properties: {\n"
      "            key: \"nested_key1\"\n"
      "            value: {\n"
      "              value: [\"Intercepted\", \"ball\"]\n"
      "            }\n"
      "          }\n"
      "          properties: {\n"
      "            key: \"nested_key11\"\n"
      "            value: {\n"
      "              value: [\"Deflated\", \"ball\"]\n"
      "            }\n"
      "          }\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "    message_map {\n"
      "      key: \"msg_key2\"\n"
      "      value {\n"
      "        maps {\n"
      "          properties {\n"
      "            key: \"][{},!@#$%^&*()\\\"\',<>?/\\\\\"\n"
      "            value {\n"
      "              value: \"running\"\n"
      "              value: \"back\"\n"
      "            }\n"
      "          }\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, MapFieldWithoutKeysWorks) {
  AddPath("message_embedded.maps.message_map.message_int32");
  uint32_t checksum = CalChecksum(0, "msg_key1", "154.250.34.10");
  checksum = CalChecksum(checksum, "-32", "154.250.34.18.8");
  checksum = CalChecksum(checksum, "msg_key2", "154.250.34.10");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    message_map: {\n"
      "      key: \"msg_key1\"\n"
      "      value: {\n"
      "        message_int32: -32\n"
      "      }\n"
      "    }\n"
      "    message_map {\n"
      "      key: \"msg_key2\"\n"
      "      value {\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, NestedMapWithoutKeysWorks) {
  AddPath(
      "message_embedded.maps.message_map.maps."
      "properties[\"nested_key1\"]");
  uint32_t checksum = CalChecksum(0, "msg_key1", "154.250.34.10");
  checksum = CalChecksum(checksum, "nested_key1", "154.250.34.18.250.18.10");
  checksum = CalChecksum(checksum, "Intercepted", "154.250.34.18.250.18.18.10");
  checksum = CalChecksum(checksum, "ball", "154.250.34.18.250.18.18.10");
  checksum = CalChecksum(checksum, "msg_key2", "154.250.34.10");

  AssertResultAsExpected(
      "message_embedded: {\n"
      "  maps: {\n"
      "    message_map: {\n"
      "      key: \"msg_key1\"\n"
      "      value: {\n"
      "        maps: {\n"
      "          properties: {\n"
      "            key: \"nested_key1\"\n"
      "            value: {\n"
      "              value: [\"Intercepted\", \"ball\"]\n"
      "            }\n"
      "          }\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "    message_map {\n"
      "      key: \"msg_key2\"\n"
      "      value {\n"
      "        maps {\n"
      "        }\n"
      "      }\n"
      "    }\n"
      "  }\n"
      "}",
      checksum);
}

TEST_P(FieldMaskScrubberTest, EnumFieldValue) {
  AddPath("message_enum[\"1\"]");
  AssertResultAsExpected("message_enum: PENUM2", CalChecksum(0, "1", "72"));
}

TEST_P(FieldMaskScrubberTest, MismatchEnumFieldValue) {
  AddPath("message_enum[\"0\"]");
  AssertResultAsExpected("", 0);
}

TEST_P(FieldMaskScrubberTest, EnumFieldValue2) {
  AddPath("message_enum[\"1\"]");
  AddPath("message_enum[\"0\"]");
  AssertResultAsExpected("message_enum: PENUM2", CalChecksum(0, "1", "72"));
}

TEST_P(FieldMaskScrubberTest, PackedRepeatedEnumFieldValue) {
  AddPath("repeated_enum[\"1\"],repeated_enum[\"2\"]");
  uint32_t checksum = CalChecksum(0, "1", "144");
  checksum = CalChecksum(checksum, "2", "144");
  AssertResultAsExpected("repeated_enum: [PENUM2, PENUM3]", checksum);
}

TEST_P(FieldMaskScrubberTest, EmbeddedNonPackedRepeatedEnumFieldValue) {
  AddPath("message_embedded.repeated_enum[\"0\"]");
  uint32_t checksum = CalChecksum(0, "0", "154.144");
  AssertResultAsExpected("message_embedded: {repeated_enum: [PENUM1]}",
                         checksum);
}

TEST_P(FieldMaskScrubberTest, EmptyListValueExclude) {
  AddPath("struct_value.string_value");
  uint32_t checksum = CalChecksum(0, "", "");
  AssertResultAsExpected("", checksum);
}

TEST_P(FieldMaskScrubberTest, EmptyListValueExcludeNested) {
  AddPath("message_struct.fields[\"string_field\"]");
  uint32_t checksum = CalChecksum(0, "", "");
  AssertResultAsExpected("", checksum);
}

TEST_P(FieldMaskScrubberTest, AddFieldFilterWithUnknownFieldsShouldBeIncluded) {
  // Unknown fields are not included in checksum.
  if (GetParam()) {
    return;
  }
  AddPath("wkt_int32,");
  AssertResultAsExpected("wkt_int32: { value: -4 }\n", 0);

  const google::protobuf::Type* root_type = FindType(
      "type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
  EXPECT_NE(root_type, nullptr) << "Failed to find root type";
  std::unique_ptr<FieldMaskTree> tree(new FieldMaskTree(
      root_type, [this](const std::string& type) { return FindType(type); }));

  ASSERT_OK(tree->AddOrIntersectFieldPaths({"wkt_int32"}));
  google::protobuf::Type new_root_type = *root_type;
  new_root_type.clear_fields();
  auto scrubber = std::make_unique<ProtoScrubber>(
      &new_root_type,
      [this](const std::string& type) { return FindType(type); },
      std::vector<const FieldCheckerInterface*>{},
      ScrubberContext::kTestScrubbing);
  scrubber->AddFieldFilter(tree.get());
  if (GetParam()) {
    auto scrub_result = scrubber->ScrubWithChecksum(&cord_message_data());
    ASSERT_OK(scrub_result);
  } else {
    auto scrub_result = scrubber->Scrub(&cord_message_data());
    ASSERT_OK(scrub_result);
  }

  google::protobuf::TextFormat::Parser parser;
  ScrubberTestMessage expected_message;
  QCHECK(parser.ParseFromString("wkt_int32: { value: -4 }", &expected_message));

  ScrubberTestMessage result_message;
  result_message.ParseFromCord(cord_message_data().Cord());

  EXPECT_THAT(result_message, EqualsProto(expected_message));
}

// Tests the interaction of the proto scrubber with a mock field checker.
//
// While it's recommended not to test interactions, this is needed to ensure
// changes made to the proto scrubber do not break third party field checkers.
//
// Additionally, this is used to make stronger guarantees on stack overflow
// protection. For example, we can test each recursive method is being counted
// for the message depth.

class FieldCheckerInteractionTest : public ::testing::Test,
                                    public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    const std::string descriptor_path = GetTestDataFilePath(
        "proto_scrubber/"
        "field_mask_test_proto_descriptor.pb");
    absl::StatusOr<std::unique_ptr<TypeHelper>> status =
        TypeHelper::Create(descriptor_path);
    type_helper_ = std::move(status.value());
  }

  const google::protobuf::Type* FindType(const std::string& type_url) {
    absl::StatusOr<const google::protobuf::Type*> result =
        type_helper_->ResolveTypeUrl(type_url);
    if (!result.ok()) {
      return nullptr;
    }
    return result.value();
  }

  void ScrubTestMessage(absl::string_view input_proto_text,
                        const MockFieldChecker* mock_field_checker) {
    ScrubberTestMessage input = ParseTextProtoOrDie(input_proto_text);

    const google::protobuf::Type* root_type = FindType(
        "type.googleapis.com/"
        "proto_processing_lib.scrubber.testing.ScrubberTestMessage");
    ProtoScrubber scrubber(
        root_type,
        [this](const std::string& type) {
          return FindType(type);
        },
        {mock_field_checker}, ScrubberContext::kTestScrubbing, false);

    CordMessageData cord_message_data(input.SerializeAsCord());
    // Run function under test.
    scrubber.ScrubWithChecksum(&cord_message_data).IgnoreError();
  }

 private:
  // A TypeHelper for testing.
  std::unique_ptr<TypeHelper> type_helper_ = nullptr;
};

TEST_F(FieldCheckerInteractionTest, DepthForSingleNestedMessage) {
  StrictMock<MockFieldChecker> mock_field_checker;

  Sequence s;
  EXPECT_CALL(mock_field_checker, SupportAny)
      .InSequence(s)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 0))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 1))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kInclude));

  ScrubTestMessage(
      "message_embedded {\n"
      "  message_bool: true\n"
      "}",
      &mock_field_checker);
}

TEST_F(FieldCheckerInteractionTest, DepthForMultipleNestedPaths) {
  StrictMock<MockFieldChecker> mock_field_checker;

  Sequence s;
  EXPECT_CALL(mock_field_checker, SupportAny)
      .InSequence(s)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 0))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 1))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 2))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kInclude));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 1))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 2))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kInclude));

  ScrubTestMessage(
      "message_embedded {\n"
      "  message_embedded {\n"
      "    message_double: 17.5\n"
      "  }\n"
      "  embedded2 {\n"
      "    message_int32: 32\n"
      "  }\n"
      "}",
      &mock_field_checker);
}

TEST_F(FieldCheckerInteractionTest, DepthPreservedForScrubbingAny) {
  StrictMock<MockFieldChecker> mock_field_checker;

  Sequence s;
  EXPECT_CALL(mock_field_checker, SupportAny)
      .InSequence(s)
      .WillOnce(Return(true));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 0))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 1))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, CheckType)
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kPartial));
  EXPECT_CALL(mock_field_checker, SupportAny)
      .InSequence(s)
      .WillOnce(Return(false));
  EXPECT_CALL(mock_field_checker, CheckField(_, _, 2))
      .InSequence(s)
      .WillOnce(Return(FieldCheckResults::kInclude));

  ScrubTestMessage(
      "message_embedded {\n"
      "  any {\n"
      "[type.googleapis.com/"
      "proto_processing_lib.scrubber.testing.ScrubberTestMessage] {\n"
      "      message_int32: 32\n"
      "    }\n"
      "  }"
      "}",
      &mock_field_checker);
}

}  // namespace
}  // namespace testing
}  // namespace proto_processing_lib::proto_scrubber
