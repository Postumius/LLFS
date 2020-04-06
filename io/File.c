#include "../disk/driver.c"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define UNAVAIL 20
#define NUM_INODES 256

#define OUT_OF_INODES -1
#define OUT_OF_BLOCKS -2
#define DIR_FULL -3
#define FILE_TOO_BIG -4
#define PATH_TOO_LONG -5
#define DIR_NOT_FOUND -6
#define WRONG_FILETYPE -7

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

#define DIR 0
#define TXT 1
#define REMOVED_DIR 2
#define REMOVED 3

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

void writeDir(FILE* vdisk, short iNum, directory dir) {
  iNode node;
  readiNode(vdisk, iNum, &node);
  if (node.flags != DIR) {
    fprintf(stderr, "writeDir tried to overwrite a file with a directory\n");
    exit(1);
  }
  byte arr[32*16];
  for(int i=0; i<16; i++) {
    arr[i*32] = dir[i].iNodeNum;
    COPY_ARR(dir[i].filename, arr + i*32+1, 31);
  }
  writeBlock(vdisk, node.directArr[0], arr);
}

short readDir(FILE* vdisk, short iNum, directory dir) {
  iNode node;
  readiNode(vdisk, iNum, &node);
  if (node.flags != DIR) return WRONG_FILETYPE;
  byte arr[32*16];
  readBlock(vdisk, node.directArr[0], arr);
  for(int i=0; i<16; i++) {
    dir[i].iNodeNum = arr[i*32];
    COPY_ARR(arr + i*32+1, dir[i].filename, 31);
  }
  return 0;
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

short newDir(FILE* vdisk, short parentiNum, char* filename) {  
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);
  short iNodeNum = reserveBit(&freeiNodes, NUM_INODES, 1);
  if (iNodeNum == -1) return OUT_OF_INODES;

  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);
  short blockNum = reserveBit(&freeBlocks, NUM_BLOCKS, UNAVAIL);
  if (blockNum == -1) return OUT_OF_BLOCKS;

  directory parent;
  short flag = readDir(vdisk, parentiNum, parent);
  if (flag < 0) return flag;
  short entryNum = reserveEntry(parent, iNodeNum, filename);
  if (entryNum == -1) return DIR_FULL;
  
  iNode node;
  node.size = 512;
  node.flags = 0;
  node.directArr[0] = blockNum;

  directory dir;
  emptyDir(dir);


  writeiNode(vdisk, iNodeNum, node);
  writeDir(vdisk, parentiNum, parent);  
  writeDir(vdisk, iNodeNum, dir);  
  writeBlock(vdisk, 2, freeBlocks.bytes);
  writeBlock(vdisk, 1, freeiNodes.bytes);
  
  return iNodeNum;
}


  
  
  
  





void makeRoot(FILE* vdisk) {
  iNode node;
  node.size = 0;
  node.flags = DIR;
  node.directArr[0] = 3;
  writeiNode(vdisk, 0x00, node);

  directory root;
  emptyDir(root);

  writeDir(vdisk, 0, root);
}

void initLLFS(FILE* vdisk) {                          
  int superblockData[2] = {3, //nblocks
                           0};//ninodes
  writeBlock(vdisk, 0, superblockData);
  writeFreeVec(vdisk, 1, 256, 1);
  writeFreeVec(vdisk, 2, BLOCK_SIZE, UNAVAIL);
  makeRoot(vdisk);
 
  
  /*directory root;
  root[0].iNodeBlockNum = 'a';
  COPY_ARR("john\0", root[0].filename, 4);
  writeDir(vdisk, 3, root);

  directory rootCopy;
  readDir(vdisk, 3, rootCopy);
  printf("%d %s\n", rootCopy[0].iNodeBlockNum,
         rootCopy[0].filename);
  */  
 
}



short newFile(FILE* vdisk, short parentiNum, char* filename) {  
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);
  short iNodeNum = reserveBit(&freeiNodes, NUM_INODES, 1);
  if (iNodeNum == -1) return OUT_OF_INODES;

  directory parent;
  short flag = readDir(vdisk, parentiNum, parent);
  if (flag < 0) return flag;
  short entryNum = reserveEntry(parent, iNodeNum, filename);
  if (entryNum == -1) return DIR_FULL;

  iNode node;
  node.size = 0;
  node.flags = TXT;

  writeBlock(vdisk, 1, freeiNodes.bytes);
  writeDir(vdisk, parentiNum, parent);
  writeiNode(vdisk, iNodeNum, node);
  
  return iNodeNum;
}

short sizeToBlocks(unsigned size) {
  return (size%512 == 0)? size/512: size/512+1;
}

short newIndBlock(FILE* vdisk, bitArray* freeBlocks, short blockNum) {
  short indBlockNum = reserveBit(freeBlocks, NUM_BLOCKS, UNAVAIL);
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

short addBlock(FILE* vdisk, bitArray* freeBlocks, iNode* node) {
 
  short blockNum = reserveBit(freeBlocks, NUM_BLOCKS, UNAVAIL);
  if (blockNum == -1) return OUT_OF_BLOCKS;
  
  unsigned oldNumBlocks = sizeToBlocks(node->size);
  if (oldNumBlocks == 10 + 256 + 256*256)
    return FILE_TOO_BIG;
  
  if (oldNumBlocks < 10) {
    node->directArr[oldNumBlocks] = blockNum;
  } else if (oldNumBlocks == 10) {
    short indBlockNum = newIndBlock(vdisk, freeBlocks, blockNum);
    if (indBlockNum == -1) {
      return OUT_OF_BLOCKS;
    } else {
      node->singleInd = indBlockNum;
    }
  } else if (oldNumBlocks < 10 + 256) {
    updateIndBlock(vdisk, node->singleInd,
                   oldNumBlocks-10, blockNum);      
  } else if ((oldNumBlocks-10-256) % 256 == 0) {
    short indBlockNum = newIndBlock(vdisk, freeBlocks, blockNum);
    if (indBlockNum == -1) {
      return OUT_OF_BLOCKS;
    } else if (oldNumBlocks == 10+256) {
      short doubleIndNum = newIndBlock(vdisk, freeBlocks, indBlockNum);
      if (doubleIndNum == -1) {
        return OUT_OF_BLOCKS;
      } else {
        node->doubleInd = doubleIndNum;
      }
    } else {
      updateIndBlock(vdisk, node->doubleInd,
                     (oldNumBlocks-10-256)/256, indBlockNum);
    }
  } else {
    short doubleInd[256];
    readBlock(vdisk, node->doubleInd, doubleInd);
    updateIndBlock(vdisk, doubleInd[(oldNumBlocks-10-256)/256],
                   (oldNumBlocks-10-256)%256, blockNum);
  }
  printf("adding block %d\n", blockNum);
  return blockNum;  
}
 
short growFileTo(FILE* vdisk, byte iNum, unsigned newSize) {
  iNode node;
  readiNode(vdisk, iNum, &node);  
  if(newSize <= node.size) return 0;
  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);
  
  short oldNumBlocks = sizeToBlocks(node.size);
  short newNumBlocks = sizeToBlocks(newSize);
  for(short i=oldNumBlocks; i<newNumBlocks; i++) {
    node.size = i*512;
    short blockNum = addBlock(vdisk, &freeBlocks, &node);
    if (blockNum < 0) return blockNum;
  }
  node.size = newSize;
  writeiNode(vdisk, iNum, node);
  writeBlock(vdisk, 2, freeBlocks.bytes);
  return 0;
}

short getBlockNum(FILE* vdisk, iNode node, short blockPos) {
  if (blockPos < 10) {
    return node.directArr[blockPos];
  } else if (blockPos < 10 + 256) {
    short singleInd[256];
    readBlock(vdisk, node.singleInd, singleInd);
    return singleInd[blockPos-10];
  } else if (blockPos < 10+256+256*256) {
    short doubleInd[256];
    readBlock(vdisk, node.doubleInd, doubleInd);
    short singleInd[256];
    readBlock(vdisk, singleInd[(blockPos-10-256)/256], singleInd);
    return singleInd[(blockPos-10-256)%256];
  } else {
    return FILE_TOO_BIG;
  }
}        

typedef struct log {
  short iNum;
  //iNode node;
  char* block;
  unsigned pos;
} log;

short file_seek(FILE* vdisk, log* l, unsigned pos) {
  printf("writing to file %d\n", l->iNum);
  iNode node;
  readiNode(vdisk, l->iNum, &node);
  
  short flag = growFileTo(vdisk, l->iNum, pos+1);
  if (flag < 0) {
    fprintf(stderr, "file_seek ran out of space: %d\n", flag);
    return flag;
  }
  if (pos/512 != l->pos/512) {
    short oldBlockNum = getBlockNum(vdisk, node, l->pos/512);
    short newBlockNum = getBlockNum(vdisk, node, pos/512);
    printf("switching from block %d to %d\n", oldBlockNum, newBlockNum);

    writeBlock(vdisk, oldBlockNum, l->block);
    readBlock(vdisk, newBlockNum, l->block);
  }
  l->pos = pos;
  return 0;
}

void file_writeChar(log* l, char c) {
  l->block[l->pos % 512] = c;
}

char file_readChar(log l) {
  return l.block[l.pos % 512];
}

void file_writeStr(FILE* vdisk, log* l, char* str, unsigned length) {
  unsigned startPos = l->pos;
  short flag = file_seek(vdisk, l, startPos + length-1);
  if (flag < 0) {
    fprintf(stderr, "file_writeString couldn't seek\n");
    return;
  }
  for(unsigned i=0; i<length; i++) {
    file_seek(vdisk, l, startPos + i);   
    file_writeChar(l, str[i]);
  }
}

void file_readStr(FILE* vdisk, log* l, unsigned length, char* str) {
  for(unsigned i=0; i<length; i++) {
    short flag = file_seek(vdisk, l, l->pos + i);
    if (flag < 0) {
      fprintf(stderr, "file_readStr couldn't seek\n");
      return;
    }
    str[i] = file_readChar(*l);
  }
}

byte searchDir(directory here, char* filename) {
  for(int i=0; i<16; i++) {
    if(!strncmp(here[i].filename, filename, 31) &&
       here[i].iNodeNum != 0) {
      return here[i].iNodeNum;
    }
  }
  return 0;
}

typedef struct filePath {
  char str[4*31];
  char* tokens[5];
  short length;
} filePath;  
void tokenizePath(char* string, filePath* path) {
  COPY_ARR(string, path->str, 4*31);
  if (!(path->tokens[0] = strtok(path->str, "/"))) return;
  for(path->length = 1;
      (path->tokens[path->length] = strtok(NULL, "/"))
      && path->length<4+1;        
      path->length++);                                                     
}

short walkPath(FILE* vdisk, byte iNum, filePath path) {
  if (path.length == 0) {
    return iNum;
  } else if (path.length == 4+1) {
    return PATH_TOO_LONG;
  }  
  directory here;
  short flag = readDir(vdisk, iNum, here);
  if(flag < 0) return flag;

  for(int i=0; i<path.length; i++) {
    iNum = searchDir(here, path.tokens[i]);
    if (!iNum) {
      return DIR_NOT_FOUND;
    } else {
      readDir(vdisk, iNum, here);
      flag = readDir(vdisk, iNum, here);
      if(flag < 0) return flag;
    }
  }
  return iNum;
}

short getParent(FILE* vdisk, char pathString[4*31],
                char dirName[31], directory parent) {
  filePath path;
  tokenizePath(pathString, &path);
  if (path.length <= 0) return DIR_NOT_FOUND;
  
  path.length--;
  COPY_ARR(path.tokens[path.length], dirName, 31);
  short parentiNum = walkPath(vdisk, 0, path);
  if (parentiNum < 0) return parentiNum;  
  
  iNode parentNode;
  readiNode(vdisk, parentiNum, &parentNode);
  if (parentNode.flags != DIR) return DIR_NOT_FOUND;

  readDir(vdisk, parentiNum, parent);
  return parentiNum;
}

void file_mkdir(FILE* vdisk, char pathString[4*31]) {
  char dirName[31];
  directory parent;
  short parentiNum = getParent(vdisk, pathString, dirName, parent);
  if (parentiNum < 0)
    fprintf(stderr, "file_mkdir: couldn't follow path\n");
  
  if (searchDir(parent, dirName)) {
    fprintf(stderr, "file_mkdir: directory %s already exists\n", pathString);
  } else {
    short diriNum = newDir(vdisk, parentiNum, dirName);
    if (diriNum < 0) 
      fprintf(stderr, "file_mkdir failed to create directory: %d\n", diriNum);
  }
}

void rmEntry(directory here, byte iNum) {
  for(int i=0; i<16; i++) {
    if(here[i].iNodeNum == iNum) 
      here[i].iNodeNum = 0;
  }
}

void rmDir(FILE* vdisk, byte parentiNum, byte iNum){
  iNode node;
  readiNode(vdisk, iNum, &node);

  directory dir;
  readDir(vdisk, iNum, dir);

  directory parent;
  readDir(vdisk, parentiNum, parent);
  rmEntry(parent, iNum);

  node.flags = REMOVED_DIR;
  rmEntry(parent, iNum);
  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);  
  freeBit(&freeBlocks, node.directArr[0]);  
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);
  freeBit(&freeiNodes, iNum);

  writeiNode(vdisk, iNum, node);
  writeBlock(vdisk, 2, freeBlocks.bytes);
  writeBlock(vdisk, 1, freeiNodes.bytes);
  writeDir(vdisk, parentiNum, parent);
}

void file_rmDir(FILE* vdisk, char pathString[4*31]) {
  char dirName[31];
  directory parent;
  short parentiNum = getParent(vdisk, pathString, dirName, parent);
  if (parentiNum < 0) {
    fprintf(stderr, "file_rmdir: couldn't follow path\n");
    return;
  }
  byte iNum = searchDir(parent, dirName);
  if (!iNum) {
    fprintf(stderr, "file_rmdir: couldn't follow path\n");
    return;
  }
  
  iNode node;
  readiNode(vdisk, iNum, &node);
  if (node.flags != DIR) {
    fprintf(stderr, "file_rmdir: %s is not a directory\n", dirName);
    return;
  }
  
  directory dir;
  readDir(vdisk, iNum, dir);
  for(int i=0; i<16; i++) {
    if(dir[i].iNodeNum != 0) {
      fprintf(stderr, "file_rmdir: directory isn't empty\n");
      return;
    }
  }  
  rmDir(vdisk, parentiNum, iNum);
}

bool openArr[256];

log file_open(FILE* vdisk, char pathString[4*31]) {
  log l;
  l.block = malloc(sizeof(char)*512);
  
  char filename[31];
  directory parent;
  short parentiNum = getParent(vdisk, pathString, filename, parent);  
  
  short iNum = searchDir(parent, filename);
  if (!iNum) {      
    iNum = newFile(vdisk, parentiNum, filename);
    if (iNum < 0) {
      fprintf(stderr, "file_open failed to create file: %d\n", iNum);
      return l;     
    }
  } else {
    if (openArr[iNum]) {
      fprintf(stderr, "file_open couldn't open %s because it's already open\n", filename);
      return l;
    }       
  }
  openArr[iNum] = true;
  l.iNum = iNum;
  iNode node;
  readiNode(vdisk, iNum, &node);
  readBlock(vdisk, node.directArr[0], &l.block);
  l.pos = 0;
  file_seek(vdisk, &l, 0);
  return l;
}

void file_close(FILE* vdisk, log* l) {
  iNode node;
  readiNode(vdisk, l->iNum, &node);
  
  writeiNode(vdisk, l->iNum, node);
  writeBlock(vdisk, getBlockNum(vdisk, node, l->pos/512), l->block);
  openArr[l->iNum] = false;
  free(l->block);
}

void rm(FILE* vdisk, byte parentiNum, byte iNum) {
  iNode node;
  readiNode(vdisk, iNum, &node);
  node.flags = REMOVED;

  directory parent;
  readDir(vdisk, parentiNum, parent);
  rmEntry(parent, iNum);
  
  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);
  for(int i=0; i<sizeToBlocks(node.size); i++) {
    freeBit(&freeBlocks, getBlockNum(vdisk, node, i));
  }
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);  
  freeBit(&freeiNodes, iNum);
  
  writeiNode(vdisk, iNum, node);
  writeBlock(vdisk, 2, freeBlocks.bytes);
  writeBlock(vdisk, 1, freeiNodes.bytes);
  writeDir(vdisk, parentiNum, parent);

}

void file_rm(FILE* vdisk, char pathString[4*31]) {
  char filename[31];
  directory parent;
  short parentiNum = getParent(vdisk, pathString, filename, parent);
  if (parentiNum < 0) {
    fprintf(stderr, "file_rm: couldn't follow path\n");
    return;
  }
  byte iNum = searchDir(parent, filename);
  if (!iNum) {
    fprintf(stderr, "file_rm: couldn't follow path\n");
    return;
  } else if (openArr[iNum]) {
    fprintf(stderr, "file_rm: can't remove %s; it's currently open\n", filename);
    return;
  }
  iNode node;
  readiNode(vdisk, iNum, &node);
  if (node.flags != TXT) {
    fprintf(stderr, "file_rm: %s is not a text file\n", filename);
    return;
  }

  rm(vdisk, parentiNum, iNum);  
}


void checkRec(FILE* vdisk, bitArray freeiNodes, bitArray freeBlocks,
               short iNum) {
  iNode node;
  readiNode(vdisk, iNum, &node);
  directory dir;
  readDir(vdisk, iNum, dir);
  for(int i=0; i<16; i++) {
    byte childNum = dir[i].iNodeNum;
    if (childNum > 0) {
      if(isSetAt(freeiNodes, childNum)) {
        rmEntry(dir, childNum);
        return;
      } else {
        iNode childNode;
        readiNode(vdisk, childNum, &childNode);
        if (node.flags == REMOVED_DIR) {    
          rmDir(vdisk, iNum, childNum);
          return;
        } else if (node.flags == REMOVED){
          rm(vdisk, iNum, childNum);
          return;
        } else if (node.flags == DIR) {
          if(isSetAt(freeBlocks, node.directArr[0])) {
            directory childDir;
            emptyDir(childDir);
            writeDir(vdisk, childNum, childDir);
            flipAt(&freeBlocks, node.directArr[0]);
            return;
          } else {
            checkRec(vdisk, freeiNodes, freeBlocks, childNum);
          }
        } else {
          for(int i=0; i<sizeToBlocks(node.size); i++) {
            short blockNum = getBlockNum(vdisk, node, i);
              if(isSetAt(freeBlocks, blockNum)) {
                flipAt(&freeiNodes, blockNum);
              }
          }
          return;
        }
      }
    }
  }
}

int main() {
  FILE* vdisk = fopen("../disk/vdisk", "w+");  
  initLLFS(vdisk);

  iNode rootNode;
  readiNode(vdisk, 0x00, &rootNode);
  
  char dirPath[4*31] = "/john";
  file_mkdir(vdisk, dirPath);
  
  char filePath[4*31] = "/john/frum";
  log l = file_open(vdisk, filePath);
  
  char bean[16] = "real human bean\0";
  file_writeStr(vdisk, &l, bean, 16);
  file_close(vdisk, &l);

  file_rm(vdisk, filePath);

  file_rmDir(vdisk, dirPath);
  
  fclose(vdisk);
  return 0;
}

