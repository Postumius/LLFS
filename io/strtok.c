#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define COPY_ARR(_origin, _dest, _n)\
  for(int _i=0; _i<_n; _i++)\
    *(_dest + _i) = *(_origin + _i);

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

int main() {
  filePath path;
  char s[4*31] = "one/two/three/four/five";
  tokenizePath(s, &path);

  printf("%s\n", s);
  
  for(int i=0; path.tokens[i] && i<4; i++) {
    printf("%s\n", path.tokens[i]);
  }
}
  
