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

#include "proto_processing_lib/proto_scrubber/internal_field_checker.h"

#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"
#include "proto_processing_lib/proto_scrubber/constants.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"

namespace proto_processing_lib::proto_scrubber {

namespace {

// Set storing type_urls of Google Internal message, which should be scrubbed.
const absl::flat_hash_set<std::string>& GoogleInternalFieldsTypeUrl() {
  static absl::flat_hash_set<std::string>* google_internal_fields_type_url =
      []() {
        return new absl::flat_hash_set<std::string>({kDebugInfoTypeName});
      }();
  return *google_internal_fields_type_url;
}

// Gets whether the given type_url matches the type_url of any
// Google Public message.
bool IsGoogleInternalField(absl::string_view type_url) {
  return GoogleInternalFieldsTypeUrl().contains(type_url);
}
}  // namespace

InternalFieldChecker::InternalFieldChecker() {}

FieldCheckResults InternalFieldChecker::CheckType(
    const google::protobuf::Type* type) const {
  if (type && IsGoogleInternalField(type->name())) {
    return FieldCheckResults::kExclude;
  }
  return FieldCheckResults::kInclude;
}

// static.
const InternalFieldChecker* InternalFieldChecker::GetDefault() {
  static const InternalFieldChecker* default_internal_field_checker =
      new InternalFieldChecker();
  return default_internal_field_checker;
}

}  // namespace proto_processing_lib::proto_scrubber
