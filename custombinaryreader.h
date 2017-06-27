// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef CUSTOM_BINARY_READER_H_
#define CUSTOM_BINARY_READER_H_

#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "metadatatables.h"

typedef std::vector<uint8_t>::iterator binary_stream_iter;

namespace google_cloud_debugger_portable_pdb {
struct CompressedMetadataTableHeader;

enum Heap : std::uint8_t {
  StringsHeap = 0x01,
  GuidsHeap = 0x02,
  BlobsHeap = 0x04
};

// Class that consumes a file or a uint8_t vector and produces a
// binary stream. This stream is used to read byte, integers,
// compressed integers and table index.
class CustomBinaryStream {
 public:
  // Consumes a file and exposes the file content as a binary stream.
  bool ConsumeFile(const std::string &file);

  // Consumes a uint8_t vector and exposes it as a binary stream.
  // The caller must ensure that the vector remains valid while
  // using this stream.
  bool ConsumeVector(std::vector<uint8_t> *byte_vector);

  // Returns true if there is a next byte in the stream.
  bool HasNext() const;

  // Returns the next byte without advancing the stream.
  bool Peek(std::uint8_t *result) const;

  // Sets the stream position to position from the current position.
  bool SeekFromCurrent(std::uint64_t position);

  // Sets the stream position to position from the original position.
  bool SeekFromOrigin(std::uint64_t position);

  // Sets where the stream will end. This should be less than the current end_.
  bool SetStreamLength(std::uint64_t length);

  // Returns the number of bytes remaining in the stream.
  size_t GetRemainingStreamLength() { return std::distance(iterator_, end_); }

  // Reads the next byte in the stream. Returns false if the byte
  // cannot be read.
  bool ReadByte(std::uint8_t *result);

  // Tries to read bytes_to_read in the stream and stores result in result.
  // Returns false if the number of bytes read are less than that.
  bool ReadBytes(std::uint8_t *result, std::uint32_t bytes_to_read,
                 std::uint32_t *bytes_read);

  // Reads the next UInt16 from the stream. Returns false if the UInt16
  // cannot be read.
  bool ReadUInt16(std::uint16_t *result);

  // Reads the next UInt32 from the stream. Returns false if the UInt32
  // cannot be read.
  bool ReadUInt32(std::uint32_t *result);

  // Reads an unsigned integer using the encoding mechanism described in the
  // ECMA spec. See II.23.2 "Blobs and signatures". Returns false if the
  // Compressed UInt32 cannot be read.
  bool ReadCompressedUInt32(std::uint32_t *result);

  // Reads a signed int using the encoding mechanism described in the ECMA spec.
  // See II.23.2 "Blobs and signatures".
  //
  //
  // Why the long SignedInt name, rather than "CompressedUInt" and
  // "CompressedInt"? Because 90% of all reads from compressed values are for
  // UInts and it is too easy to confuse the two. Only call this method if you
  // are positive you are reading a signed value.
  bool ReadCompressSignedInt32(std::int32_t *result);

  // Reads a heap table index according to II.24.2.6 "#~ stream" under schema.
  // Returns false if it cannot be read.
  bool ReadTableIndex(Heap heap, std::uint8_t heap_size,
                      std::uint32_t *table_index);

  // Reads the size of a metadata table index according to II.24.2.6 "#~ stream"
  // under schema. Returns false if it cannot be read.
  bool ReadTableIndex(MetadataTable table,
                      const CompressedMetadataTableHeader &metadata_header,
                      std::uint32_t *table_index);

  // Returns the current stream position.
  binary_stream_iter Current() { return iterator_; }

  // Returns the end position of the stream.
  binary_stream_iter End() { return end_; }

  // Returns the beginning position of the stream.
  binary_stream_iter Begin() { return begin_; }

 private:
  // The binary content of the file (if this stream s from a file).
  std::vector<std::uint8_t> file_content_;

  // The current stream position.
  binary_stream_iter iterator_;

  // The end position of the stream.
  binary_stream_iter end_;

  // The start position of the stream.
  binary_stream_iter begin_;
};

}  // namespace google_cloud_debugger_portable_pdb

#endif
