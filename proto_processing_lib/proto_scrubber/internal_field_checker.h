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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_INTERNAL_FIELD_CHECKER_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_INTERNAL_FIELD_CHECKER_H_

#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"

namespace proto_processing_lib::proto_scrubber {

// This field checker is used to filter out google internal fields from the
// message.
class InternalFieldChecker : public FieldCheckerInterface {
 public:
  // This type is neither copyable nor movable.
  InternalFieldChecker(const InternalFieldChecker&) = delete;
  InternalFieldChecker& operator=(const InternalFieldChecker&) = delete;

  ~InternalFieldChecker() override {}

  // Returns the singleton instance.
  static const InternalFieldChecker* GetDefault();

  FieldCheckResults CheckField(
      const std::vector<std::string>& /*path*/,
      const google::protobuf::Field* /*field*/) const override {
    return FieldCheckResults::kInclude;
  }

  bool SupportAny() const override { return true; }

  FieldCheckResults CheckType(
      const google::protobuf::Type* type) const override;

  FieldFilters FilterName() const override {
    return FieldFilters::InternalFieldFilter;
  }

 private:
  InternalFieldChecker();
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_INTERNAL_FIELD_CHECKER_H_
