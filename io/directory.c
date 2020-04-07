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
