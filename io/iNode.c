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
