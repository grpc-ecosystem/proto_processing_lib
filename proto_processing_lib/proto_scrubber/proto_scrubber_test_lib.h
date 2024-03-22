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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_TEST_LIB_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_TEST_LIB_H_

#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "gmock/gmock.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"

namespace proto_processing_lib::proto_scrubber {

// Max depth for proto messages allowed by the DepthLimitedChecker.
inline constexpr int kCheckerMaxDepth = 7;

// A checker that forces the scrubber to go through all fields in a message.
class FullScanChecker : public FieldCheckerInterface {
 public:
  FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const override {
    // These field types have special partial handling logic.
    if (field != nullptr &&
        (field->kind() == google::protobuf::Field::TYPE_MESSAGE ||
         field->kind() == google::protobuf::Field::TYPE_GROUP ||
         field->kind() == google::protobuf::Field::TYPE_ENUM)) {
      return FieldCheckResults::kPartial;
    }
    return FieldCheckResults::kInclude;
  }
  bool SupportAny() const override { return true; }
  FieldCheckResults CheckType(
      const google::protobuf::Type* type) const override {
    return FieldCheckResults::kPartial;
  }

  FieldFilters FilterName() const override {
    return FieldFilters::UnknownFieldFilter;
  }
};

// A checker that acts as a FullScanChecker until a preset depth is reached.
// It then excludes all fields past that depth, limiting the depth of the output
// message.
class DepthLimitedChecker : public FullScanChecker {
 public:
  FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const override {
    if (path.size() <= kCheckerMaxDepth) {
      return FullScanChecker::CheckField(path, field);
    }
    return FieldCheckResults::kExclude;
  }
};

// A checker that acts as a FullScanChecker until a preset depth is reached.
// It then includes all fields past that depth. This preserves the entire
// message without recursing through deeply nested fields.
class DepthExtendedChecker : public FullScanChecker {
 public:
  FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const override {
    if (path.size() <= kCheckerMaxDepth) {
      return FullScanChecker::CheckField(path, field);
    }
    return FieldCheckResults::kInclude;
  }
};

class MockFieldChecker : public FieldCheckerInterface {
 public:
  MOCK_METHOD(FieldCheckResults, CheckField,
              (const std::vector<std::string>& path,
               const google::protobuf::Field* field),
              (const, override));
  MOCK_METHOD(FieldCheckResults, CheckField,
              (const std::vector<std::string>& path,
               const google::protobuf::Field* field, const int field_depth),
              (const, override));
  MOCK_METHOD(bool, SupportAny, (), (const, override));
  MOCK_METHOD(FieldCheckResults, CheckType,
              (const google::protobuf::Type* type), (const, override));
  MOCK_METHOD(FieldFilters, FilterName, (), (const, override));
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_TEST_LIB_H_
