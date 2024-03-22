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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_CHECKER_INTERFACE_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_CHECKER_INTERFACE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"

namespace proto_processing_lib::proto_scrubber {

// Result of checking whether a field should be kept after scrubbing.
enum class FieldCheckResults {
  // The whole field should be included in the scrubbing result.
  kInclude = 0,
  // The whole field should be excluded from the scrubbing result.
  kExclude = 1,
  // Part of the field will be included in the scrubbing result. This means
  // the scrubber should look into the field and checks its sub-fields
  // one-by-one.
  kPartial = 2,
};

enum class FieldFilters : int32_t {
  Unspecified = 0,
  // The filter that is used to scrub unknown field.
  UnknownFieldFilter = 1,
  // The filter that is used to scrub fields due to visibility restriction.
  VisibilityFilter = 2,
  // The filter that is used to scrub fields according to field mask.
  FieldMaskFilter = 3,
  // The filter that is used to scrub google internal fields.
  InternalFieldFilter = 4,
  // The filter that is used to scrub fields for Cloud Audit Logging.
  CloudAuditFieldFilter = 5,
  // The filter that is used to scrub fields according to the API version.
  ApiVersionFilter = 6,
};

class FieldCheckerInterface {
 public:
  virtual ~FieldCheckerInterface() {}

  // Returns whether a field should be kept after scrubbing. 'path' is is a list
  // of field names or map key values which represents the path that leads to
  // the field to be checked. 'field' is a pointer to the definition of that
  // field. Caller retains ownership of 'field'. When 'field' is nullptr, it
  // means the field is unknown (not in the Type definition).
  virtual FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const = 0;

  // An overload that also accepts the depth of the field in the message.
  // Can be used for stack overflow protection by each field checker.
  virtual FieldCheckResults CheckField(const std::vector<std::string>& path,
                                       const google::protobuf::Field* field,
                                       const int field_depth) const {
    return CheckField(path, field);
  }

  // An overload that also accepts the parent type.
  // Can be used to detect if parent is a certain type (MapEntry).
  virtual FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field, const int field_depth,
      const google::protobuf::Type* parent_type) const {
    return CheckField(path, field, field_depth);
  }

  // Returns true if this FieldChecker supports checking "Any" fields.
  virtual bool SupportAny() const = 0;

  // Checks whether a type should be kept after scrubbing.
  virtual FieldCheckResults CheckType(
      const google::protobuf::Type* type) const = 0;

  virtual FieldFilters FilterName() const = 0;
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_FIELD_CHECKER_INTERFACE_H_
