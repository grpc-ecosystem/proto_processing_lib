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

#include "proto_processing_lib/proto_scrubber/unknown_field_checker.h"

#include <string>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/string_view.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/utility.h"

namespace proto_processing_lib::proto_scrubber {

UnknownFieldChecker::UnknownFieldChecker()
    : FieldCheckerInterface(),
      check_embedded_fields_(
true),
      max_depth_(
100)
{}

FieldCheckResults UnknownFieldChecker::CheckField(
    const std::vector<std::string>& path,
    const google::protobuf::Field* field) const {
  return CheckField(path, field, /*field_depth=*/0);
}

FieldCheckResults UnknownFieldChecker::CheckField(
    const std::vector<std::string>& /*path*/,
    const google::protobuf::Field* field, const int field_depth) const {
  if (field == nullptr) {
    return FieldCheckResults::kExclude;
  }

  if (IsWellKnownTypeOrStructType(GetTypeWithoutUrl(field->type_url()))) {
    return FieldCheckResults::kInclude;
  }

  // If this is a message, the message may contain fields that are unknown.
  if (check_embedded_fields_ &&
      field->kind() == google::protobuf::Field::TYPE_MESSAGE) {
    if (field_depth < max_depth_) {
      return FieldCheckResults::kPartial;
    }
  }
  return FieldCheckResults::kInclude;
}

// static.
const UnknownFieldChecker* UnknownFieldChecker::GetDefault() {
  static const UnknownFieldChecker* default_unknown_field_checker =
      new UnknownFieldChecker();
  return default_unknown_field_checker;
}

}  // namespace proto_processing_lib::proto_scrubber
