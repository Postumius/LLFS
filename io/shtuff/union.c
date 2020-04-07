#include <stdio.h>
#include <stdlib.h>

typedef union splitShort {
  short combined;
  unsigned char split[2];
} splitShort;

int main() {
  FILE* fl = fopen("test", "w");
  splitShort n;
  n.combined = 0xFFFF;
  fwrite(n.split, 2, 1, fl);
  printf("%d %d\n", n.split[0], n.split[1]);
  fclose(fl);
}
