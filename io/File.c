#include "../disk/driver.c"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define UNAVAIL 20
#define NUM_INODES 256

//polymorphic copy
#define COPY_ARR(_origin, _dest, _n)\
  for(int _i=0; _i<_n; _i++)\
    *(_dest + _i) = *(_origin + _i);



//error codes, these get used a lot
#define OUT_OF_INODES -1
#define OUT_OF_BLOCKS -2
#define DIR_FULL -3
#define FILE_TOO_BIG -4
#define PATH_TOO_LONG -5
#define DIR_NOT_FOUND -6
#define WRONG_FILETYPE -7

//filetype flags for iNodes
#define DIR 0
#define TXT 1
#define REMOVED_DIR 2
#define REMOVED 3

#include "bitArray.c"
#include "iNode.c"
#include "directory.c"
#include "txtFile.c"
#include "File.h"

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

}


short file_seek(FILE* vdisk, log* l, unsigned pos) {
  if (sizeToBlocks(pos+1) > sizeToBlocks(l->size)) {
    short flag = growFileTo(vdisk, l->iNum, pos+1);
    if (flag < 0) {
      fprintf(stderr, "file_seek ran out of space: %d\n", flag);
      return flag;
    }
  }
  if(pos >= l->size) {
    l->size = pos+1;
  }
  if (pos/512 != l->pos/512) {
    iNode node;
    readiNode(vdisk, l->iNum, &node);
    short oldBlockNum = getBlockNum(vdisk, node, l->pos/512);
    short newBlockNum = getBlockNum(vdisk, node, pos/512);
    
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
  unsigned startPos = l->pos;
  for(unsigned i=0; i<length; i++) {
    short flag = file_seek(vdisk, l, startPos + i);
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
    l.size = 0;
    if (iNum < 0) {
      fprintf(stderr, "file_open failed to create file: %d\n", iNum);
      return l;     
    } 
  } else {
    iNode node;
    readiNode(vdisk, iNum, &node);
    readBlock(vdisk, node.directArr[0], l.block);
    l.size = node.size;
    if (openArr[iNum]) {
      fprintf(stderr, "file_open couldn't open %s because it's already open\n", filename);
      return l;
    }       
  }
  openArr[iNum] = true;
  l.iNum = iNum;
  l.pos = 0;  
  return l;
}

void file_close(FILE* vdisk, log* l) {
  iNode node;
  readiNode(vdisk, l->iNum, &node);
  node.size = l->size;
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

void indent(int numIndents) {
  printf("\n");
  for(int i=0; i<numIndents; i++) {
    printf("  ");
  }
}

void showRec(FILE* vdisk, int depth, byte iNum) {
  iNode node;
  readiNode(vdisk, iNum, &node);
  if(node.flags == DIR) {
    directory dir;
    readDir(vdisk, iNum, dir);
    for(int i=0; i<16; i++) {
      if(dir[i].iNodeNum != 0) {
        indent(depth+1);
        printf("%s", dir[i].filename);
        showRec(vdisk, depth+1, dir[i].iNodeNum);
      }      
    }
  } else if(node.flags == TXT) {
    printf(" %dB", node.size);
  }
}    

void file_show(FILE* vdisk) {
  printf("root");
  showRec(vdisk, 0, 0);
  printf("\n");
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

