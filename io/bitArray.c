typedef unsigned char byte;

typedef struct bitArray {
  byte bytes[BLOCK_SIZE];
} bitArray;

void flipAt(bitArray* arr, size_t i) {
  byte mask = 0b10000000 >> (i%8);
  arr->bytes[i/8] ^= mask;
}

bool isSetAt(bitArray arr, short i) {
  byte mask = 0b10000000 >> (i%8);
  return arr.bytes[i/8] & mask;
}

void writeFreeVec(FILE* vdisk, short blockNum, short size, short reserved) {
  bitArray freeVec;
  for(int i=0; i<size; i++) {
    freeVec.bytes[i] = 0xFF;
  }
  for(int i=0; i<reserved; i++) {
    flipAt(&freeVec, i);
  }
  writeBlock(vdisk, blockNum, freeVec.bytes);
}

short reserveBit(bitArray* bits, short numBits, short unavail) {
  for(short i=unavail; i<numBits; i++) {
    if (isSetAt(*bits, i)) {
        flipAt(bits, i);
        return i;
    }
  }
  return -1;
}

void freeBit(bitArray* bits, short bitNum) {
  if(!isSetAt(*bits, bitNum))
    flipAt(bits, bitNum);
}     
