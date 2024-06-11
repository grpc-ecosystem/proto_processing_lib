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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_ENUMS_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_ENUMS_H_

#include <cstdint>

namespace proto_processing_lib::proto_scrubber {

// Indicates what type of proto is being scrubbed. This provides a consistent
// context for logging purposes.
enum class ScrubberContext : int32_t {
  kTestScrubbing,
  kRequestScrubbing,
  kResponseScrubbing
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_PROTO_SCRUBBER_ENUMS_H_
