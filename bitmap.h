#ifndef BITMAP_H
#define BITMAP_H

typedef unsigned char byte;

void
set_bit_high(byte* bitmap, int bitId) {
  int byteId = bitId / 8;
  int bitLoc = bitId % 8;
  bitmap[byteId] = bitmap[byteId] | (1 << bitLoc);
}

void
set_bit_low(byte* bitmap, int bitId) {
  int byteId = bitId / 8;
  int bitLoc = bitId % 8;
  bitmap[byteId] = bitmap[byteId] ^ (1 << bitLoc);
}

int
get_bit_state(byte* bitmap, int bitId) {
  int byteId = bitId / 8;
  int bitLoc = bitId % 8;
  return bitmap[byteId] & (1 << bitLoc);
}

#endif
