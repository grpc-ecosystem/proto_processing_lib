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

#include "proto_processing_lib/proto_scrubber/utility.h"

#include <string>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/api.pb.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace proto_processing_lib::proto_scrubber {
namespace {

TEST(IsWellKnownType, Positive) {
  EXPECT_TRUE(IsWellKnownType("google.protobuf.Timestamp"));
  EXPECT_TRUE(IsWellKnownType("google.protobuf.Int32Value"));
  EXPECT_TRUE(IsWellKnownType("google.protobuf.StringValue"));
}

TEST(IsWellKnownType, Negative) {
  EXPECT_FALSE(IsWellKnownType(""));
  EXPECT_FALSE(IsWellKnownType("some.other.type"));
  EXPECT_FALSE(IsWellKnownType("protobuf.Timestamp"));
  EXPECT_FALSE(IsWellKnownType("GOOGLE.PROTOBUF.TIMESTAMP"));
}

TEST(IsWellKnownTypeOrStructType, Positive) {
  EXPECT_TRUE(IsWellKnownTypeOrStructType("google.protobuf.Timestamp"));
  EXPECT_TRUE(IsWellKnownTypeOrStructType("google.protobuf.Struct"));
  EXPECT_TRUE(IsWellKnownTypeOrStructType("google.protobuf.Value"));
}

TEST(IsWellKnownTypeOrStructType, Negative) {
  EXPECT_FALSE(IsWellKnownTypeOrStructType("some.other.type"));
  EXPECT_FALSE(IsWellKnownTypeOrStructType(""));
}

TEST(UtilityTest, GetBoolFromAny) {
  google::protobuf::Any any;
  google::protobuf::BoolValue bool_value;

  bool_value.set_value(true);
  any.PackFrom(bool_value);

  EXPECT_TRUE(GetBoolFromAny(any));

  bool_value.set_value(false);
  any.PackFrom(bool_value);
  EXPECT_FALSE(GetBoolFromAny(any));

  google::protobuf::StringValue string_value;
  string_value.set_value("Test");
  any.PackFrom(string_value);
  EXPECT_FALSE(GetBoolFromAny(any));
}

TEST(FindOptionOrNull, OptionFound) {
  google::protobuf::RepeatedPtrField<google::protobuf::Option> options;
  google::protobuf::Option* option1 = options.Add();
  option1->set_name("test_option_1");
  EXPECT_EQ(option1, FindOptionOrNull(options, "test_option_1"));
}

TEST(FindOptionOrNull, OptionNotFound) {
  google::protobuf::RepeatedPtrField<google::protobuf::Option> options;
  EXPECT_EQ(nullptr, FindOptionOrNull(options, "nonexistent_option"));
  options.Clear();
  EXPECT_EQ(nullptr, FindOptionOrNull(options, "test_option_1"));
}

TEST(FindOptionOrNull, EmptyOptionName) {
  google::protobuf::RepeatedPtrField<google::protobuf::Option> options;
  EXPECT_EQ(nullptr, FindOptionOrNull(options, ""));
}

TEST(UtilityTest, GetBoolOptionOrDefault) {
  google::protobuf::RepeatedPtrField<google::protobuf::Option> options;
  const std::string option_name = "some_option";
  EXPECT_TRUE(
      GetBoolOptionOrDefault(options, option_name, true /*default_value=*/));
  EXPECT_FALSE(
      GetBoolOptionOrDefault(options, option_name, false /*default_value=*/));

  google::protobuf::Option* option1 = options.Add();
  option1->set_name("test_option_1");
  EXPECT_FALSE(GetBoolOptionOrDefault(options, "test_option_1",
                                      true /*default_value=*/));
}

TEST(IsMapType, Positive) {
  google::protobuf::Type map_type;
  google::protobuf::Option* option = map_type.add_options();
  option->set_name("map_entry");
  google::protobuf::BoolValue bool_value;
  bool_value.set_value(true);
  option->mutable_value()->PackFrom(bool_value);
  EXPECT_TRUE(IsMapType(map_type));
}

TEST(IsMapType, Negative) {
  google::protobuf::Type map_type;
  google::protobuf::Option* option = map_type.add_options();
  option->set_name("some.other.option");
  google::protobuf::BoolValue bool_value;
  bool_value.set_value(true);
  option->mutable_value()->PackFrom(bool_value);
  EXPECT_FALSE(IsMapType(map_type));
}

TEST(IsMap, Positive) {
  google::protobuf::Field repeated_field;
  repeated_field.set_cardinality(google::protobuf::Field::CARDINALITY_REPEATED);

  google::protobuf::Type map_type;
  google::protobuf::Option* option = map_type.add_options();
  option->set_name("map_entry");
  google::protobuf::BoolValue bool_value;
  bool_value.set_value(true);
  option->mutable_value()->PackFrom(bool_value);
  EXPECT_TRUE(IsMap(repeated_field, map_type));
}

TEST(IsMap, Negative) {
  google::protobuf::Field repeated_field;
  // Repeated Field with unknown cardinality.
  repeated_field.set_cardinality(google::protobuf::Field::CARDINALITY_UNKNOWN);

  google::protobuf::Type map_type;
  google::protobuf::Option* option = map_type.add_options();
  option->set_name("proto2.MessageOptions.map_entry");
  google::protobuf::BoolValue bool_value;
  bool_value.set_value(true);
  option->mutable_value()->PackFrom(bool_value);
  EXPECT_FALSE(IsMap(repeated_field, map_type));
}

TEST(UtilityTest, GetTypeWithoutUrl) {
  EXPECT_EQ(GetTypeWithoutUrl("http://example.com/type"), "type");
  EXPECT_EQ(GetTypeWithoutUrl("https://www.test.org/long/path/type"), "type");
  EXPECT_EQ(GetTypeWithoutUrl("shared.services.acme.io/util.Timestamp"),
            "util.Timestamp");
  EXPECT_EQ(GetTypeWithoutUrl(
                "schemas.example.com/project/models/nested.MessageType"),
            "project/models/nested.MessageType");
  EXPECT_EQ(GetTypeWithoutUrl("exampletype"), "exampletype");
  EXPECT_EQ(GetTypeWithoutUrl(""), "");
}

}  // namespace
}  // namespace proto_processing_lib::proto_scrubber
