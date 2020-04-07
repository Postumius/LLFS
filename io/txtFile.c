 short newFile(FILE* vdisk, short parentiNum, char* filename) {  
  bitArray freeiNodes;
  readBlock(vdisk, 1, freeiNodes.bytes);
  bitArray freeBlocks;
  readBlock(vdisk, 2, freeBlocks.bytes);
  
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
  
  
  writeDir(vdisk, parentiNum, parent);
  writeiNode(vdisk, iNodeNum, node);
  writeBlock(vdisk, 2, freeBlocks.bytes);
  writeBlock(vdisk, 1, freeiNodes.bytes);
  
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
