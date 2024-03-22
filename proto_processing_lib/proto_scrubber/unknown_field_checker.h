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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_UNKNOWN_FIELD_CHECKER_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_UNKNOWN_FIELD_CHECKER_H_

#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/flags/declare.h"
#include "absl/strings/string_view.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"

namespace proto_processing_lib::proto_scrubber {

// Forward declare test peer.
namespace testing {
class UnknownFieldCheckerTestPeer;
}

// This field checker is used to filter out unknown fields from the message.
// If we can't find a field in the message type then it would be a nullptr.
// We use that to return kExclude for this field to filter it out from the
// message.
class UnknownFieldChecker : public FieldCheckerInterface {
 public:
  // This type is neither copyable nor movable.
  UnknownFieldChecker(const UnknownFieldChecker&) = delete;
  UnknownFieldChecker& operator=(const UnknownFieldChecker&) = delete;

  ~UnknownFieldChecker() override {}

  // Returns the singleton instance.
  static const UnknownFieldChecker* GetDefault();

  FieldCheckResults CheckField(
      const std::vector<std::string>& path,
      const google::protobuf::Field* field) const override;

  FieldCheckResults CheckField(const std::vector<std::string>& path,
                               const google::protobuf::Field* field,
                               const int field_depth) const override;

  bool SupportAny() const override { return false; }

  FieldCheckResults CheckType(
      const google::protobuf::Type* /*type*/) const override {
    return FieldCheckResults::kInclude;
  }

  FieldFilters FilterName() const override {
    return FieldFilters::UnknownFieldFilter;
  }

 private:
  friend class testing::UnknownFieldCheckerTestPeer;

  UnknownFieldChecker();

  // Cache the flag values for improved performance, especially because these
  // values will be read for each field in a single proto message.
  const bool check_embedded_fields_;
  const int max_depth_;
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_UNKNOWN_FIELD_CHECKER_H_
