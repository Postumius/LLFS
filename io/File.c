#include "../disk/driver.c"
#include <stdbool.h>
#include <stdlib.h>

#define UNAVAIL 20
#define NUM_INODES 256

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

#define COPY_ARR(_origin, _dest, _n)\
  for(int _i=0; _i<_n; _i++)\
    *(_dest + _i) = *(_origin + _i);

typedef struct iNode {
  unsigned size;
  unsigned flags;
  short directArr[10];
  short singleInd;
  short doubleInd;
} iNode;

typedef union splitInt {
  int combine;
  short split[2];
} spitInt;

void writeiNode(FILE* vdisk, byte iNodeNum, iNode node) {
  short block[256];
  readBlock(vdisk, iNodeNum/0x10+UNAVAIL-16, block);  
  byte start = (iNodeNum % 0x10) * 0x10;
  
  union splitInt n;
  
  n.combine = node.size;
  block[start] = n.split[1];
  block[start+1] = n.split[0];

  n.combine = node.flags;
  block[start+2] = n.split[1];
  block[start+3] = n.split[0];

  COPY_ARR(node.directArr,  block+start+4, 10);

  block[start+14] = node.singleInd;
  block[start+15] = node.doubleInd;

  writeBlock(vdisk, iNodeNum/0x10+UNAVAIL-16, block);
}

void readiNode(FILE* vdisk, short iNodeNum, iNode* node) {
  short block[256];
  readBlock(vdisk, iNodeNum/0x10+UNAVAIL-16, block);
  byte start = (iNodeNum % 0x10) * 0x10;
  
  union splitInt n;
  
  n.split[1] = block[start];
  n.split[0] = block[start+1];
  node->size = n.combine;

  n.split[1] = block[start+2];
  n.split[0] = block[start+3];
  node->flags = n.combine;

  COPY_ARR(block+start+4, node->directArr, 10);
 
  node->singleInd = block[start+14];
  node->doubleInd = block[start+15];
}

typedef struct entry {
  byte iNodeNum;
  char filename[31];
} entry;

typedef entry directory[16];

void writeDir(FILE* vdisk, short blockNum, directory dir) {
  byte arr[32*16];
  for(int i=0; i<16; i++) {
    arr[i*32] = dir[i].iNodeNum;
    COPY_ARR(dir[i].filename, arr + i*32+1, 31);
  }
  writeBlock(vdisk, blockNum, arr);
}

void readDir(FILE* vdisk, short blockNum, directory dir) {
  byte arr[32*16];
  readBlock(vdisk, blockNum, arr);
  for(int i=0; i<16; i++) {
    dir[i].iNodeNum = arr[i*32];
    COPY_ARR(arr + i*32+1, dir[i].filename, 31);
  }
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

short reserveEntry(directory parent, short iNodeNum, char* filename) {
  for(int i=0; i<16; i++) {
    if (parent[i].iNodeNum == 0x00) {
      parent[i].iNodeNum = iNodeNum;
      COPY_ARR(filename, parent[i].filename, 31);
      return i;      
    }
  }
  return -1;
}

void emptyDir(directory dir) {
  for(short i=0; i<16; i++) {
    dir[i].iNodeNum = 0x00;
  }
}

char newDir(FILE* vdisk, iNode parentiNode, char* filename) {  
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);
  short iNodeNum = reserveBit(&freeiNodes, NUM_INODES, 1);
  if (iNodeNum == -1) return 'i';

  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);
  short blockNum = reserveBit(&freeBlocks, NUM_BLOCKS, UNAVAIL);
  if (blockNum == -1) return 'b';

  directory parent;
  readDir(vdisk, parentiNode.directArr[0], parent);
  short entryNum = reserveEntry(parent, iNodeNum, filename);
  if (entryNum == -1) return 'e';
  
  iNode node;
  node.size = 0;
  node.flags = 0;
  node.directArr[0] = blockNum;

  directory dir;
  emptyDir(dir);

  writeBlock(vdisk, 1, freeiNodes.bytes);
  writeBlock(vdisk, 2, freeBlocks.bytes);
  writeDir(vdisk, parentiNode.directArr[0], parent);
  writeiNode(vdisk, iNodeNum, node);
  writeDir(vdisk, blockNum, dir);
  
  return '0';
}

char newFile(FILE* vdisk, iNode parentiNode, char* filename) {  
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);
  short iNodeNum = reserveBit(&freeiNodes, NUM_INODES, 1);
  if (iNodeNum == -1) return 'i';

  directory parent;
  readDir(vdisk, parentiNode.directArr[0], parent);
  short entryNum = reserveEntry(parent, iNodeNum, filename);
  if (entryNum == -1) return 'e';
  
  iNode node;
  node.size = 0;
  node.flags = 1;

  writeBlock(vdisk, 1, freeiNodes.bytes);
  writeBlock(vdisk, 2, freeBlocks.bytes);
  writeDir(vdisk, parentiNode.directArr[0], parent);
  writeiNode(vdisk, iNodeNum, node);
  
  return '0';
}




void makeRoot(FILE* vdisk) {
  iNode node;
  node.size = 0;
  node.flags = 0;
  node.directArr[0] = 3;
  writeiNode(vdisk, 0x00, node);

  directory root;
  emptyDir(root);

  writeDir(vdisk, 3, root);
}

void initLLFS() {
  FILE* vdisk = fopen("../disk/vdisk", "w+");  
                          
  int superblockData[2] = {3, //nblocks
                           0};//ninodes
  writeBlock(vdisk, 0, superblockData);


  writeFreeVec(vdisk, 1, 256, 1);
  writeFreeVec(vdisk, 2, BLOCK_SIZE, UNAVAIL);

  makeRoot(vdisk);
  iNode rootNode;
  readiNode(vdisk, 0x00, &rootNode);
  
  char flag = newDir(vdisk, rootNode, "John\0");
  printf("%c\n", flag);
  
  /*directory root;
  root[0].iNodeBlockNum = 'a';
  COPY_ARR("john\0", root[0].filename, 4);
  writeDir(vdisk, 3, root);

  directory rootCopy;
  readDir(vdisk, 3, rootCopy);
  printf("%d %s\n", rootCopy[0].iNodeBlockNum,
         rootCopy[0].filename);
  */  
  fclose(vdisk);    
}



short sizeToBlocks(unsigned size) {
  return (size%512 == 0)? size/512: size/512+1;
}

short newIndBlock(FILE* vdisk, bitArray* freeBlocks, short blockNum) {
  short indBlockNum = reserveBit(&freeblocks, NUM_BLOCKS, UNAVAIL);
  if (indBlockNum == -1) return -1;  
  
  short indBlock[256];
  indBlock[0] = blockNum;
  writeBlock(vdisk, indBlockNum, indBlock);
  return indBlockNum;
}

void updateIndBlock(FILE* vdisk, short indBlockNum,
                     short pos, short newBlockNum) {
  short indBlock[256];
  readBlock(vdisk, indBlockNum, indBlock);
  indBlock[pos] = newBlockNum;
  writeBlock(vdisk, indBlockNum, indBlock);
}

char addBlock(FILE* vdisk, iNode node) {
  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);
  short blockNum = reserveBit(&freeBlocks, NUM_BLOCKS, UNAVAIL);
  if (blockNum == -1) return 'b';
  
  oldNumBlocks = sizeToBlocks(node.size);
  if (oldNumBlocks == 10 + 256 + 256*256)
    return 'f';
  
  if (oldNumBlocks < 10) {
    node.directArr[oldNumBlocks] = blockNum;
  } else if (oldNumBlocks == 10) {
    short indBlockNum = newIndBlock(vdisk, &freeBlocks, blockNum);
    if (indBlockNum == -1) {
      return 'i';
    } else {
      node.singleInd = indBlockNum;
    }
  } else if (oldNumBlocks < 10 + 256) {
    updateIndBlock(vdisk, node.singleInd,
                   oldNumBlocks-10, blockNum);      
  } else if ((oldNumBlocks-10-256) % 256 == 0) {
    short indBlockNum = newIndBlock(vdisk, &freeBlocks, blockNum);
    if (indBlockNum == -1) {
      return 'i';
    } else if (oldNumBlocks == 10+256) {
      short doubleIndNum = newIndBlock(&freeblocks, indBlockNum);
      if (doubleIndNum == -1) {
        return 'd';
      } else {
        node.doubleInd = doubleIndNum;
      }
    } else {
      updateIndBlock(vdisk, node.doubleInd,
                     (oldNumBlocks-10-256)/256, indBlockNum);
    }
  } else {
    short doubleInd[256];
    readBlock(vdisk, node.doubleInd, doubleInd);
    updateIndBlock(vdisk, doubleInd[(oldNumBlocks-10-256)/256],
                   (oldNumBlocks-10-256)%256, blockNum);
  }
  writeBlock(vdisk, 2, freeBlocks.bytes);
  return '0';
}
    
char growFile(FILE* vdisk, iNode node, unsigned newSize) {
  oldNumBlocks = sizeToBlocks(node.size);
  newNumBlocks = sizeToBlocks(newSize);

typedef struct log {
  iNode node;
  short whichBlock;
  char block[512];
  unsigned ptr;
} log;

void seek(FILE* vdisk, log l, short pos) {
  if (pos > l.node.size) {
    
  short whichBlock = pos / 512;

int main() {
  initLLFS();
  return 0;
}

