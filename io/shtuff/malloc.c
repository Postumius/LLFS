#include <stdio.h>
#include <stdlib.h>

int main() {
  char* s = malloc(sizeof(char)*10);
  s[0] = 'a';
  printf("%s\n", s);
}
