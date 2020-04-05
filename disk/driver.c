#include <stdio.h>
#include <stdlib.h>

#define BLOCK_SIZE 512
const int NUM_BLOCKS = 4096;

void readBlock(FILE* disk, short blockNum, void* buffer) {
  fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
  fread(buffer, BLOCK_SIZE, 1, disk);
}

void writeBlock(FILE* disk, short blockNum, void* data) {
  printf("writing to block %d\n", blockNum);
  fseek(disk, blockNum * BLOCK_SIZE, SEEK_SET);
  fwrite(data, BLOCK_SIZE, 1, disk);
}
