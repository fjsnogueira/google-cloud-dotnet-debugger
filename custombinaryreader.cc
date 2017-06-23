// Copyright 2017 Google Inc. All Rights Reserved.
// Licensed under the Apache License Version 2.0.

#include "custombinaryreader.h"

#include <iostream>
#include <iterator>
#include <vector>

#include "metadataheaders.h"

const int one = 1;
#define big_endian() ( (*(char*)&one) == 0 )

namespace google_cloud_debugger_portable_pdb {

  using std::ifstream;
  using std::ios;
  using std::vector;

  bool CustomBinaryStream::ConsumeFile(std::string file) {
    ifstream file_stream(file, ios::in | ios::binary | ios::ate);
    if (file_stream.is_open()) {
      std::streampos file_size;

      file_stream.unsetf(std::ios::skipws);

      file_size = file_stream.tellg();
      file_content_.reserve(file_size);
      file_stream.seekg(0, file_stream.beg);

      file_content_.insert(file_content_.begin(),
        std::istream_iterator<uint8_t>(file_stream),
        std::istream_iterator<uint8_t>());

      iterator_ = file_content_.begin();
      begin_ = file_content_.begin();
      end_ = file_content_.end();
      return true;
    }

    return false;
  }

  bool CustomBinaryStream::ConsumeVector(std::vector<uint8_t> *byte_vector)
  {
    if (!byte_vector) {
      return false;
    }

    iterator_ = byte_vector->begin();
    begin_ = byte_vector->begin();
    end_ = byte_vector->end();
    return true;
  }

  bool CustomBinaryStream::ReadBytes(uint8_t *result, uint32_t bytes_to_read)
  {
    while (bytes_to_read != 0) {
      if (iterator_ == end_) {
        return false;
      }

      *(result++) = *(iterator_++);
      --bytes_to_read;
    }
    return true;
  }

  bool CustomBinaryStream::HasNext()
  {
    return iterator_ < end_;
  }

  bool CustomBinaryStream::Peak(uint8_t * result)
  {
    if (iterator_ == end_) {
      return false;
    }

    *result = *iterator_;

    return true;
  }

  bool CustomBinaryStream::SeekFromCurrent(uint32_t index)
  {
    if (iterator_ + index > end_) {
      return false;
    }

    iterator_ += index;
    return true;
  }

  bool CustomBinaryStream::SeekFromOrigin(uint32_t position)
  {
    std::vector<uint8_t>::iterator begin = file_content_.begin();
    if (begin + position > end_) {
      return false;
    }

    iterator_ = begin + position;
    return true;
  }

  bool CustomBinaryStream::SetStreamLength(uint32_t length)
  {
    if (iterator_ + length > end_) {
      return false;
    }

    end_ = iterator_ + length;
    return true;
  }

  bool CustomBinaryStream::ReadByte(uint8_t *result)
  {
    return ReadBytes(result, 1);
  }

  bool CustomBinaryStream::ReadUInt16(uint16_t *result)
  {
    vector<uint8_t> buffer(2, 0);
    if (!ReadBytes(buffer.data(), 2)) {
      return false;
    }

    if (big_endian()) {
      *result = ((buffer[0] << 8) & 0xFF00) |
        buffer[1];
    }
    else {
      *result = ((buffer[1] << 8) & 0xFF00) |
        buffer[0];
    }

    return true;
  }

  bool CustomBinaryStream::ReadUInt32(uint32_t *result)
  {
    vector<uint8_t> buffer(4, 0);
    if (!ReadBytes(buffer.data(), 4)) {
      return false;
    }

    if (big_endian()) {
      *result = ((buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
    }
    else {
      *result = ((buffer[3] << 24) | (buffer[2] << 16) | (buffer[1] << 8) | buffer[0]);
    }

    return true;
  }

  bool CustomBinaryStream::ReadCompressedUInt32(uint32_t *uncompress_int)
  {
    uint8_t first_byte;
    if (!ReadByte(&first_byte)) {
      return false;
    }

    // If the first bit is a 0, return the value. Range 0 - 0x7F.
    if ((first_byte & 0x80) == 0) {
      *uncompress_int = first_byte;
      return true;
    }

    uint8_t second_byte;
    if (!ReadByte(&second_byte)) {
      return false;
    }

    // If the first two bits are "10", return the first two bytes.
    // Mask it with 0b11000000 (0xC0) and confirm the result is 0b10000000 (0x80).
    // Result should be in the range 0x80 - 0x3FFF.
    if ((first_byte & 0xC0) == 0x80) {
      *uncompress_int = ((first_byte << 8) | second_byte) & 0x3FFF;
      return true;
    }
    
    uint8_t third_byte;
    uint8_t fourth_byte;
    if (!ReadByte(&third_byte) || !ReadByte(&fourth_byte)) {
      return false;
    }

    // If the first three bits are "110", return the first four bytes.
    // Mask it with 0b11100000 (0xE0) and confirm the result is 0b11000000 (0xC0).
    // Result should be in the range 0x4000 - 0x1FFFFFFF.
    if ((first_byte & 0xE0) == 0xC0) {
      *uncompress_int = ((first_byte << 24) | (second_byte << 16) | (third_byte << 8) | fourth_byte) & 0x1FFFFFFF;
      return true;
    }

    return false;
  }

  bool CustomBinaryStream::ReadCompressSignedInt32(int32_t *uncompressed_int)
  {
    // A simpler explanation is in "Expert .NET 2.0 IL Assembler". To encode a signed
    // integer value:
    // 1. Take the absolute value of the integer and shift it left by 1 bit.
    // 2. Set the least significant bit equal to the sign (MSB) of the original value.
    // 3. Apply the regular ComporessedInt method.
    // Reversing is straight forward.
    uint8_t first_byte;
    if (!Peak(&first_byte)) {
      return false;
    }

    uint32_t raw_bytes;
    if (!ReadCompressedUInt32(&raw_bytes)) {
      return false;
    }

    int32_t result = (int32_t)raw_bytes;
    // Bits are rotated by 1 so 2 complement bit is at the end.
    bool negative = ((result & 0x1) != 0);
    result >>= 1;

    if (negative) {
      // To apply two's complement we merge the bits in based on the width.
      // 1 byte uses 6 bits, 2 byte values use 14 bits, and 4 byte values use 28 bits.
      // Get the width by checking the first byte again.
      if ((first_byte & 0x80) == 0) {
        result |= 0xFFFFFFC0;
      }
      else if ((first_byte & 0xC0) == 0x80) {
        result |= 0xFFFFE000;
      }
      else if ((first_byte & 0xE0) == 0xC0) {
        result |= 0xF0000000;
      }
      else {
        return false;
      }
    }

    *uncompressed_int = result;
    return true;
  }

  bool CustomBinaryStream::ReadTableIndex(Heap heap, uint8_t heap_size, uint32_t *table_index)
  {
    // The Heap enum also encodes the bit mask into the heapSize value.
    if ((heap & heap_size) != 0x0) {
      return ReadUInt32(table_index);
    }

    uint16_t temp;
    if (!ReadUInt16(&temp)) {
      return false;
    }

    *table_index = temp;
    return true;
  }

  bool CustomBinaryStream::ReadTableIndex(MetadataTable table, const CompressedMetadataTableHeader &metadata_header, uint32_t *table_index)
  {
    if (!metadata_header.valid_mask_[static_cast<int>(table)])
    {
      // WARNING: If you are reading a table index into a metadata table that isn't
      // present, something is wrong.
      //
      // In practice, this happens when you only load the PDB metadata tables and not the
      // primary assembly's too. Since the PDB doesn't contain the rest of the metadata.
      // If the table happens to contain more than 2^16 entries, we will read the wrong
      // number of bytes and TERRIBLE THINGS will happen since all future reads will be
      // corrupt.
      //
      // BUG: Read assembly metadata (headers at least) in addition to PDB metadata.
      // For now we assume everything is less than 2^16.
      uint16_t temp;
      if (!ReadUInt16(&temp)) {
        return false;
      }

      *table_index = temp;
      return true;
    }

    uint32_t present_table_index = 0;
    for (size_t index = 0; index < static_cast<int>(table); ++index) {
      if (metadata_header.valid_mask_[index]) {
        ++present_table_index;
      }
    }

    uint32_t rows_present = metadata_header.num_rows_[present_table_index];
    // If the table has less than 2^16 rows then it is stored using 2 bytes.
    // Otherwise, 4 bytes.
    if (rows_present < 0x10000) { // 2^16
      uint16_t temp;
      if (!ReadUInt16(&temp)) {
        return false;
      }

      *table_index = temp;
      return true;
    }

    return ReadUInt32(table_index);
  }
}