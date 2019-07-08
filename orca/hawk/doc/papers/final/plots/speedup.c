#include <stdio.h>

#define maxnum 1000

main()
{
  int i;
  double v;
  int base = 0;

  double va[maxnum];

  for (i=0; i<maxnum; i++) va[i]=-1.0;
  while (scanf("%d", &i)>0) {
    scanf("%lf", &(va[i]));
  }
  if (va[0]==-1.0) {
    base = 1;
  }
  if (va[base] == -1.0) {
    fprintf(stderr,"no base response time - cannot compute speed up\n");
    exit(1);
  }
  if (va[base]==0.0) {
    fprintf(stderr,"base response time is 0 - cannot compute speed up\n");
    exit(1);
  }

  for (i=1; i<maxnum; i++) {
    if (va[i]!=-1) printf("%d %f\n", i, va[base]/va[i]);
  }
  exit(0);
}
