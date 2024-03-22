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

#ifndef THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_CHECKSUMMER_H_
#define THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_CHECKSUMMER_H_

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "proto_field_extraction/message_data/message_data.h"
#include "google/protobuf/io/coded_stream.h"

namespace proto_processing_lib::proto_scrubber {

using ::google::protobuf::io::CodedInputStream;

class Checksummer {
 public:
  ~Checksummer() {}

  // Returns the singleton instance.
  static const Checksummer* GetDefault();

  // Returns the singleton instance with specific max depth limit.
  static const Checksummer* GetDefault(const int max_depth);

  Checksummer(const Checksummer&) = delete;
  Checksummer& operator=(const Checksummer&) = delete;

  // Calculates and returns the checksum of the message.
  absl::StatusOr<uint32_t> CalculateChecksum(
      google::protobuf::field_extraction::MessageData* data_buffer,
      const google::protobuf::Type* type,
      std::function<const google::protobuf::Type*(const std::string&)>
          type_finder) const;
  absl::StatusOr<uint32_t> CalculateChecksum(
      absl::Cord* data, const google::protobuf::Type* type,
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder) const;
  absl::StatusOr<uint32_t> CalculateChecksum(
      google::protobuf::field_extraction::MessageData* data_buffer,
      const google::protobuf::Type* type,
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      std::vector<std::string>* parent_tags) const;
  absl::StatusOr<uint32_t> CalculateChecksum(
      absl::Cord* data, const google::protobuf::Type* type,
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      std::vector<std::string>* parent_tags) const;

  // CalculateSingleFieldChecksum returns the checksum of
  // the current field only, which is identified by field and tag. It also
  // updates the position of the input_stream after reading this field.
  absl::StatusOr<uint32_t> CalculateSingleFieldChecksum(
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field* field,
      std::vector<std::string>* parent_tags, uint32_t* tag,
      int32_t* position) const;

  // Adds the new checksum for field(tags : value) to the
  // total checksum.
  void AddToChecksum(absl::string_view tags, absl::string_view value,
                     uint32_t* checksum) const;

  using TypeRenderer = void (*)(
      const Checksummer*,
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream*, const google::protobuf::Field&,
      std::vector<std::string>*, uint32_t*);

 private:
  Checksummer();

  explicit Checksummer(const int max_depth);

  absl::Status ScanMessage(
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Type& type, std::vector<std::string>* parent_tags,
      uint32_t* checksum) const;

  absl::Status ScanNonRepeatedField(
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field& field,
      std::vector<std::string>* parent_tags, uint32_t* tag,
      uint32_t* checksum) const;

  absl::StatusOr<uint32_t> ScanRepeatedField(
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field& field,
      std::vector<std::string>* parent_tags, uint32_t* tag, int32_t* position,
      uint32_t* checksum) const;

  absl::Status ScanPacked(
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field& field,
      std::vector<std::string>* parent_tags, uint32_t* tag,
      uint32_t* checksum) const;

  absl::Status ScanAnyMessage(
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Type& type, std::vector<std::string>* parent_tags,
      uint32_t* checksum) const;

  // When ignore_zero_value is true, ESF checksums excludes the well known field
  // if the field value is the default value added by ESF when this field
  // is not populated in the original message.
  void ScanPrimitiveField(google::protobuf::io::CodedInputStream* input_stream,
                          const google::protobuf::Field& field,
                          std::vector<std::string>* parent_tags, uint32_t* tag,
                          uint32_t* checksum, bool ignore_zero_value) const;

  static void InitRendererMap();
  static TypeRenderer* FindTypeRenderer(absl::string_view type_url);

  const int max_depth_;

  // Renders and calculates checksum for google.protobuf.Timestamp and
  // google.protobuf.Duration.
  static void RenderTimestampAndDuration(
      const Checksummer*,
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field& field,
      std::vector<std::string>* parent_tags, uint32_t* checksum);

  // Renders and calculates checksum for fields defined by
  // google/protobuf/wrappers.proto
  static void RenderWrapperValue(
      const Checksummer*,
      const std::function<const google::protobuf::Type*(const std::string&)>&
          type_finder,
      google::protobuf::io::CodedInputStream* input_stream,
      const google::protobuf::Field& field,
      std::vector<std::string>* parent_tags, uint32_t* checksum);
};

}  // namespace proto_processing_lib::proto_scrubber

#endif  // THIRD_PARTY_PROTO_PROCESSING_LIB_SRC_PROTO_SCRUBBER_CHECKSUMMER_H_
