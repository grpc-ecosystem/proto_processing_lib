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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_FACTORY_HELPER_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_FACTORY_HELPER_H_

#include <functional>
#include <memory>
#include <string>

#include "proto_processing_lib/interface_util.h"
#include "proto_processing_lib/proto_scrubber/field_checker_interface.h"
#include "proto_processing_lib/proto_scrubber/field_mask_tree.h"
#include "proto_processing_lib/proto_scrubber/unknown_field_checker.h"

namespace proto_processing_lib {

// Factory Method to create UnknownFieldChecker.
static std::unique_ptr<proto_processing_lib::FieldMaskTreeInterface>
FactoryCreateFieldMaskTree(
    const google::protobuf::Type* type,
    std::function<const google::protobuf::Type*(const std::string&)>
        type_finder) {
  proto_processing_lib::proto_scrubber::FieldMaskTree* fmt =
      new proto_processing_lib::proto_scrubber::FieldMaskTree(type,
                                                              type_finder);
  return std::unique_ptr<proto_processing_lib::proto_scrubber::FieldMaskTree>(
      fmt);
}

}  // namespace proto_processing_lib

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_FACTORY_HELPER_H_
