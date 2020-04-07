#include "../io/File.c"

int main() {
  FILE* vdisk = fopen("../disk/vdisk", "w+");  
  initLLFS(vdisk);

  iNode rootNode;
  readiNode(vdisk, 0x00, &rootNode);

  printf("create file in root directory\n");
  log l1 = file_open(vdisk, "/file1");
  file_show(vdisk);  
  printf("\nwrite to file\n");
  file_writeStr(vdisk, &l1, "I am file 1\0", 12);
  file_close(vdisk, &l1);
  file_show(vdisk);  
  
  printf("\nread file\n");  
  l1 = file_open(vdisk,"/file1");
  char str1[13];  
  file_readStr(vdisk, &l1, 12, str1);
  printf("'%s'\n", str1);

  printf("\ncreate subdirectories\n");
  file_mkdir(vdisk, "/dir1");
  file_mkdir(vdisk, "/dir1/dir2");
  file_mkdir(vdisk, "/dir1/dir3");
  file_mkdir(vdisk, "/dir1/dir3/dir4");
  file_show(vdisk);

  printf("\ncreate file in arbitrary directory\n");
  log l2 = file_open(vdisk, "/dir1/dir3/file2");
  printf("write to file at position 10 000\n");
  file_seek(vdisk, &l2, 10000);
  file_writeStr(vdisk, &l2, "I am file 2\0", 12);
  file_close(vdisk, &l2);
  file_show(vdisk);

  printf("\nread file at position 10 000\n");
  l2 = file_open(vdisk, "/dir1/dir3/file2");
  file_seek(vdisk, &l2, 10000);
  char str2[13];  
  file_readStr(vdisk, &l2, 12, str2);
  printf("'%s'\n", str2);

  printf("\nremoving files and some directories\n");
  file_close(vdisk, &l1);
  file_rm(vdisk, "/file1");
  file_show(vdisk);
  file_rmDir(vdisk, "/dir1/dir3/dir4");
  file_show(vdisk);
  file_close(vdisk, &l2);
  file_rm(vdisk, "/dir1/dir3/file2");
  file_show(vdisk);
  file_rmDir(vdisk, "/dir1/dir3/");
  file_show(vdisk);
  
  fclose(vdisk);
  return 0;
}

 
