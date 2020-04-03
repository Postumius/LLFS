#include <stdio.h>
#include <string.h>

int main() {
  char* path[4];
  char s[4*31] = "one/two/three/four/five";
  path[0] = strtok(s, "/");

  for(int i=1; i<4; i++) {
    path[i] = strtok(NULL, "/");
  }
  if (strtok(NULL, "/")) {
    printf("path too long\n");
    return 0;
  }

  for(int i=0; path[i] && i<4; i++) {
    printf("%s\n", path[i]);
  }
}
  
