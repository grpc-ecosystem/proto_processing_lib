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

#include <cstddef>
#include <string>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/api.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "proto_processing_lib/proto_scrubber/constants.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace proto_processing_lib::proto_scrubber {

bool IsWellKnownType(absl::string_view type_name) {
  static const auto* const types = new absl::flat_hash_set<absl::string_view>(
      {"google.protobuf.Timestamp", "google.protobuf.Duration",
       "google.protobuf.DoubleValue", "google.protobuf.FloatValue",
       "google.protobuf.Int64Value", "google.protobuf.UInt64Value",
       "google.protobuf.Int32Value", "google.protobuf.UInt32Value",
       "google.protobuf.BoolValue", "google.protobuf.StringValue",
       "google.protobuf.BytesValue", "google.protobuf.FieldMask"});
  return types->contains(type_name);
}

bool IsWellKnownTypeOrStructType(absl::string_view name) {
  return IsWellKnownType(name) || name == kStructType ||
         name == kStructValueType || name == kStructListValueType;
}

bool GetBoolFromAny(const google::protobuf::Any& any) {
  google::protobuf::BoolValue b;
  b.ParseFromString(any.value());
  return b.value();
}

const google::protobuf::Option* FindOptionOrNull(
    const google::protobuf::RepeatedPtrField<google::protobuf::Option>& options,
    const absl::string_view& option_name) {
  for (auto& opt : options) {
    if (opt.name() == option_name) {
      return &opt;
    }
  }
  return nullptr;
}

bool GetBoolOptionOrDefault(
    const google::protobuf::RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, bool default_value) {
  const google::protobuf::Option* opt = FindOptionOrNull(options, option_name);
  if (opt == nullptr) {
    return default_value;
  }
  return GetBoolFromAny(opt->value());
}

bool IsMapType(const google::protobuf::Type& type) {
  return GetBoolOptionOrDefault(type.options(), "map_entry", false)
      ;
}

bool IsMap(const google::protobuf::Field& field,
           const google::protobuf::Type& type) {
  return (field.cardinality() ==
              google::protobuf::Field::CARDINALITY_REPEATED &&
          IsMapType(type));
}

absl::string_view GetTypeWithoutUrl(absl::string_view type_url) {
  if (type_url.size() > kTypeUrlSize && type_url[kTypeUrlSize] == '/') {
    return absl::ClippedSubstr(type_url, kTypeUrlSize + 1);
  } else {
    size_t idx = type_url.rfind('/');
    return absl::ClippedSubstr(type_url, idx + 1);
  }
}

std::string GetTestDataFilePath(absl::string_view path) {
  return absl::StrCat("proto_processing_lib/", std::string(path));
}

}  // namespace proto_processing_lib::proto_scrubber
