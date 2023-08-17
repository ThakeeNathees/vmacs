// Copyright (c) 2023 Thakee Nathees

#pragma once

#include <stdint.h>


class UTF8 {

public:

  int EncodeBytesCount(int value) {
    if (value <= 0x7f) return 1;
    if (value <= 0x7ff) return 2;
    if (value <= 0xffff) return 3;
    if (value <= 0x10ffff) return 4;

    // if we're here means it's an invalid leading byte
    return 0;
  }


  int DecodeBytesCount(uint8_t byte) {

    if ((byte >> 7) == 0b0) return 1;
    if ((byte >> 6) == 0b10) return 1; //< continuation byte
    if ((byte >> 5) == 0b110) return 2;
    if ((byte >> 4) == 0b1110) return 3;
    if ((byte >> 3) == 0b11110) return 4;

    // if we're here means it's an invalid utf8 byte
    return 1;
  }


  int EncodeValue(int value, uint8_t* bytes) {

    if (value <= 0x7f) {
      *bytes = value & 0x7f;
      return 1;
    }

    // 2 byte character 110xxxxx 10xxxxxx -> last 6 bits write to 2nd byte and
    // first 5 bit write to first byte
    if (value <= 0x7ff) {
      *(bytes++) = (uint8_t)(0b11000000 | ((value & 0b11111000000) >> 6));
      *(bytes) = (uint8_t)(0b10000000 | ((value & 111111)));
      return 2;
    }

    // 3 byte character 1110xxxx 10xxxxxx 10xxxxxx -> from last, 6 bits write
    // to  3rd byte, next 6 bits write to 2nd byte, and 4 bits to first byte.
    if (value <= 0xffff) {
      *(bytes++) = (uint8_t)(0b11100000 | ((value & 0b1111000000000000) >> 12));
      *(bytes++) = (uint8_t)(0b10000000 | ((value & 0b111111000000) >> 6));
      *(bytes) = (uint8_t)(0b10000000 | ((value & 0b111111)));
      return 3;
    }

    // 4 byte character 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx -> last 6 bits to
    // to 4th byte, next 6 bits to 3rd byte, next 6 bits to 2nd byte, 3 bits
    // first byte.
    if (value <= 0x10ffff) {
      *(bytes++) = (uint8_t)(0b11110000 | ((value & (0b111 << 18)) >> 18));
      *(bytes++) = (uint8_t)(0b10000000 | ((value & (0b111111 << 12)) >> 12));
      *(bytes++) = (uint8_t)(0b10000000 | ((value & (0b111111 << 6)) >> 6));
      *(bytes) = (uint8_t)(0b10000000 | ((value & 0b111111)));
      return 4;
    }

    return 0;
  }


  int DecodeBytes(uint8_t* bytes, int* value) {

    int continue_bytes = 0;
    int byte_count = 1;
    int _value = 0;

    if ((*bytes & 0b11000000) == 0b10000000) {
      *value = *bytes;
      return byte_count;
    }

    else if ((*bytes & 0b11100000) == 0b11000000) {
      continue_bytes = 1;
      _value = (*bytes & 0b11111);
    }

    else if ((*bytes & 0b11110000) == 0b11100000) {
      continue_bytes = 2;
      _value = (*bytes & 0b1111);
    }

    else if ((*bytes & 0b11111000) == 0b11110000) {
      continue_bytes = 3;
      _value = (*bytes & 0b111);
    }

    else {
      // Invalid leading byte
      return -1;
    }

    // now add the continuation bytes to the _value
    while (continue_bytes--) {
      bytes++, byte_count++;

      if ((*bytes & 0b11000000) != 0b10000000) return -1;

      _value = (_value << 6) | (*bytes & 0b00111111);
    }

    *value = _value;
    return byte_count;
  }

};
