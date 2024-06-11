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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_UTILITY_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_UTILITY_H_

#include <string>

#include "google/protobuf/type.pb.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace proto_processing_lib::proto_scrubber {
// Returns true if type_name represents a well-known type.
// Only the fields that are allowed to be in query params should be in this
// list i.e., no google.protobuf.Any or google.protobuf.Struct or any types
// that aren't allowed to be bound as a query param as specified in
// j/c/g/api/tools/framework/model/TypeRef.java&rcl=91333167&l=36.
bool IsWellKnownType(absl::string_view type_name);

// Whether the field with the given type name is a well-known type or any of
// struct types i.e., google.protobuf.{Struct,Value,ListValue}.
bool IsWellKnownTypeOrStructType(absl::string_view type_name);

// Returns the option with the given name or nullptr if not found.
const google::protobuf::Option* FindOptionOrNull(
    const google::protobuf::RepeatedPtrField<google::protobuf::Option>& options,
    const absl::string_view& option_name);

// Extracts a boolean value from a protobuf Any message.
bool GetBoolFromAny(const google::protobuf::Any& any);

// Retrieves a boolean option value from the provided options, or returns the
// default if the option is not found.
bool GetBoolOptionOrDefault(
    const google::protobuf::RepeatedPtrField<google::protobuf::Option>& options,
    const std::string& option_name, bool default_value);

// Checks if the given protobuf Type represents a map type by examining the
// 'map_entry' option.
bool IsMapType(const google::protobuf::Type& type);

// Determines if the given field represents a protobuf map within the provided
// type.
bool IsMap(const google::protobuf::Field& field,
           const google::protobuf::Type& type);

// Extracts the type name portion from a type URL.
absl::string_view GetTypeWithoutUrl(absl::string_view type_url);

// Returns the relative path to the test data file.
std::string GetTestDataFilePath(absl::string_view path);

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_UTILITY_H_
