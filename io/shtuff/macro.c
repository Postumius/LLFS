#include <stdio.h>

#define COUNT(n) for(int i=0; i<n; i++) printf("%d\n", i);

#define COPY_ARR(_origin,  _n, _dest)\
  for(int _i=0; _i<_n; _i++)\
    *(_dest + _i) = *(_origin + _i);

int main() {
  int arr1[4] = {1,2,3,4};
  int arr2[3];
  COPY_ARR(arr1+1, 3, arr2);
  //for(int i=0; i<3; i++) arr2[0 + i] = arr1[1 + i];
  for(int i=0; i<3; i++)
    printf("%d\n", arr2[i]);
  
  return 0;
}
    
