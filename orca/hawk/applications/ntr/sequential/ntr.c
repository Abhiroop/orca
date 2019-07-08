#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define iters 1
#define nchan 4
#define nrange 10
#define ndop 512
#define mdop 9
#define ntargets 4
#define k0 0
#define k1 40

typedef enum {
  FALSE,
  TRUE
} boolean;

double inpr[2][ndop][ndop][nchan]; /* Should be [nrange][ndop] but
					 [nrange][nrange] matrix is
					 used so that the corner turn
					 can be performed on the same
					 partitioned object */
int det[ndop][nrange][nchan];
/* Intermediate Data set */
double w[ndop];
/* FFT data structures */
int brt[ndop];
double twiddles[2][ndop/2];
/* target ranges and frequencies */
int range0[ntargets];
int doppler0[ntargets];

/* misc variables */
double sigma0, thresh, const2, const1, pi, a0;
int a=11111111;

static void radar();
static void corner_turn(double inpr[2][ndop][ndop][nchan]);
static void compute(double inpr[2][ndop][ndop][nchan],
		    int det[ndop][nrange][nchan],int brt[ndop],
		    double twiddles[2][ndop/2],double w[ndop], double const2);
static void dgen(double inpr[2][ndop][ndop][nchan], int range0[ntargets], 
		 int doppler0[ntargets], double sigma0);
static double gauss1(double mean, double sigma);
static double random1();
static void gen_bit_reverse_table(int brt[ndop]);
static void gen_w_table(double w[2][ndop/2]);
static void fft(double a[2][ndop][ndop][nchan], int k, int l, int brt[ndop], double w[2][ndop/2]);
static void chkmat(int det[ndop][nrange][nchan]);
void print_inpr(double inpr[2][ndop][ndop][nchan]);
void print_mag(double mag[ndop][nrange][nchan]);

int main() {
  int k;
  /* Initialize constants. */
  pi = 3.141592653589793;
  sigma0=1.13;
  thresh=2.421;
  const2=thresh/(nrange*(k1-k0)-1+thresh);
  const1=2*pi/ndop;

  /* Target frequencies. */
  doppler0[0] = 6;
  doppler0[1] = 16;
  doppler0[2] = 26;
  doppler0[3] = 36;

  /* Target ranges. */
  range0[0] = 2;
  range0[1] = 4;
  range0[2] = 6;
  range0[3] = 8;
  
  gen_bit_reverse_table(brt);
  gen_w_table(twiddles);
  /* Hamming window */
  a0 = 0.5*(ndop-1);
  for (k=0; k<ndop; k++) w[k] = 0.54 + 0.46*cos(const1*(k-a0));
  radar();
  return 0;
}

void
radar() {
  int iter;

  for (iter=0; iter<iters; iter++) {
    dgen(inpr, range0, doppler0, sigma0);
    corner_turn(inpr);
    compute(inpr,det,brt,twiddles,w,const2);
    /* Check results */
    chkmat(det);
  }
}

void
corner_turn(double inpr[2][ndop][ndop][nchan]) {
  int i, j, k;
  double temp;

  for (k=0; k<nchan; k++) 
    for (j=0; j<nrange; j++)
      for (i=0; i< ndop; i++) {
	temp = inpr[0][i][j][k];
	inpr[0][i][j][k] = inpr[0][j][i][k];
	inpr[0][j][i][k] = temp;
	temp = inpr[1][i][j][k];
	inpr[1][i][j][k] = inpr[1][j][i][k];
	inpr[1][j][i][k] = temp;
      }
}

void
compute(double inpr[2][ndop][ndop][nchan],
	int det[ndop][nrange][nchan],int brt[ndop],
	double twiddles[2][ndop/2],double w[ndop], double const2) {
  
  double localthresh[nchan];
  double mag[ndop][nrange][nchan];
  double magsum;
  int i,j,k;

  print_inpr(inpr);
  for (i=0; i<nchan; i++) {

    /* Window and doppler processing */
    for (j=0; j<nrange; j++)
      for (k=0; k<ndop; k++) {
	inpr[0][k][j][i] = inpr[0][k][j][i]*w[k];
	inpr[1][k][j][i] = inpr[1][k][j][i]*w[k];
      }
  }
  print_inpr(inpr);
  for (i=0; i<nchan; i++)
    for (j=0; j<nrange; j++) 
      fft(inpr,j, i, brt,twiddles);
  print_inpr(inpr);

  for (i=0; i<nchan; i++)
    for (j=0; j<nrange; j++) 
      for (k=k0; k<k1; k++) {
	inpr[0][k][j][i] = inpr[0][k][j][i]/ndop;
	inpr[1][k][j][i] = inpr[1][k][j][i]/ndop;
      }
  print_inpr(inpr);
  
  for (i=0; i<nchan; i++) {
    for (j=0; j<nrange; j++)
      for (k=k0; k<k1; k++)
	mag[k][j][i] = sqrt(inpr[0][k][j][i]*inpr[0][k][j][i] + inpr[1][k][j][i]*inpr[1][k][j][i]);
  }
  
  print_mag(mag);
  for (i=0; i<nchan; i++) {
    /* Doopler subselection and reduction */
    magsum =0.0;
    for (j=0; j<nrange; j++)
      for (k=k0; k<k1; k++) magsum += mag[k][j][i];
  }

  for (i=0; i<nchan; i++) {
    /* CFAR detection */
    localthresh[i] = const2 * magsum;
    for (j=0; j<nrange; j++) 
      for (k=k0; k<k1; k++) {
	if (mag[k][j][i] > localthresh[i])
	  det[k][j][i] = 1;
	else
	  det[k][j][i] = 0;
      }
  }
}

/* dgen - generate the input data set. */

void
dgen(double inpr[2][ndop][ndop][nchan], int range0[ntargets], 
     int doppler0[ntargets], double sigma0) {
  int i, j, k, m, range, doppler;
  double arg1, arg2;

  for (i=0; i<nchan; i++)
    for (k=0; k<ndop; k++)
      for (j=0; j<nrange; j++) {
	inpr[0][j][k][i] = gauss1(0.0,sigma0);
	inpr[1][j][k][i] = gauss1(0.0,sigma0);
      }
  
  pi=3.141592653589793;
  arg1 = 2.0*pi/ndop;
  for (m=0; m<ntargets; m++) {
    range = range0[m];
    doppler = doppler0[m];
    for (i=0; i<nchan; i++) 
      for (k=0; k<ndop; k++) {
	arg2 = arg1*(k)*doppler;
	inpr[0][range][k][i] = inpr[0][range][k][i] + cos(arg2);
	inpr[1][range][k][i] = inpr[1][range][k][i] + sin(arg2);
      }
  }
}

/* Gauss1 - crude approximation of normal distribution formed
   from sum of 12 uniforms. */
double 
gauss1(double mean, double sigma) {
  int i;
  double x;
  
  x = 0.0;
  for (i=0; i<12; i++) x += random1();
  return mean + sigma*(x - 6.0);
}

/* random1 - uniformly distributed [0,1) real. */
double
random1() {
  int m, m1, b, a0, a1, b0, b1;

  m = 100000000;
  m1 = 10000;
  b = 31415821;  
  a1 = a/m1;
  a0 = a % m1;
  b1 = b/m1;
  b0 = b % m1;
  a0 = ((a0*b1 + a1*b0) % m1)*m1 + a0*b0;
  a = (a0 + 1) % m;
  return (double )(a/m);
}

/* gen_bit_reverse_table - initialize bit reverse table. */
void 
gen_bit_reverse_table(int brt[ndop]) {
  int i, j, k, nv2;
  int tbrt[ndop+1];

  nv2 = ndop/2;
  j = 1;
  tbrt[1] = j;
  for (i=2; i<=ndop; i++) {
    k = nv2;
    while (k < j) {
      j = j - k;
      k = k/2;
    }
    j = j + k;
    tbrt[i] = j;
  }
  for (i=0; i<ndop; i++) brt[i] = tbrt[i+1]-1;
}

/* gen_w_table: generate powers of w. */
void
gen_w_table(double w[2][ndop/2]) {
  int i;
  double wr, wi, ptr, pti;

  wr = cos(pi/(ndop/2));
  wi = -sin(pi/(ndop/2));
  w[0][0] = 1.0;
  w[1][0] = 0.0;
  ptr = 1.0;
  pti = 0.0;
  for (i=1; i<ndop/2; i++) {
    w[0][i] = ptr*wr - pti*wi;
    w[1][i] = ptr*wi + pti*wr;
    ptr = w[0][i];
    pti = w[1][i];
  }
}

/* Fast Fourier Transform 1D in-place complex-complex
   decimation-in-time Cooley-Tukey. */
void
fft(double a[2][ndop][ndop][nchan], int k, int l, int brt[ndop], double w[2][ndop/2]) {
  int i,j, powerOfW, sPowerOfW, ijDiff, stage, stride, first;
  double pwr,pwi,tr,ti;
  double ir, ii, jr, ji;

  /* Bit reverse step. */
  for (i=0; i<ndop; i++) {
    j=brt[i];
    if (i<=j) {
      tr = a[0][j][k][l];
      ti = a[1][j][k][l];
      a[0][j][k][l] = a[0][i][k][l];
      a[1][j][k][l] = a[1][i][k][l];
      a[0][i][k][l] = tr;
      a[1][i][k][l] = ti;
    }
  }

  /* Butterfly computations. */
  ijDiff = 1;
  stride = 2;
  sPowerOfW = (ndop/2);
  for (stage=0; stage<mdop; stage++) {
    first = 0;
    for (powerOfW=0; powerOfW<(ndop/2); powerOfW += sPowerOfW) {
      pwr = w[0][powerOfW];
      pwi = w[1][powerOfW];
      for (i = first; i<ndop; i += stride) {
	j = i + ijDiff;
	jr = a[0][j][k][l];
	ji = a[1][j][k][l];
	ir = a[0][i][k][l];
	ii = a[1][i][k][l];
	tr = jr*pwr - ji*pwi;
	ti = jr*pwi + ji*pwr;
	a[0][j][k][l] = ir - tr;
	a[1][j][k][l] = ii - ti;
	a[0][i][k][l] = ir + tr;
	a[1][i][k][l] = ii + ti;
      }
      first = first + 1;
    }
    ijDiff = stride;
    stride = stride * 2;
    sPowerOfW = sPowerOfW / 2;
  }
}

/* chkmat - check the results Since the results of the radar program
   are probabalistic, it is hard to write a data set independent
   routine to check the results.  However, it is easy to eyeball the
   results for channel 1 and check that they are correct. What you
   want to see in the det array is all zeros except near the
   doppler[k]'th element of the range0[k]'th row of array det,
   k=0,..,ntargets, where you want to see a small number (3 or 4) of
   contiguous 1's.  */

void
chkmat(int det[ndop][nrange][nchan]) {
  int j,k;

  printf("***************************\n");
  printf("Narrowband tracking radar\n");
  printf("%3d channels %3d range cells\n", nchan,nrange);
  printf("%4d doppler cells %2d targets\n",ndop, ntargets);
  printf("***************************\n");
  printf("Results for channel 1:\n");
  for (j=0; j<nrange; j++)
    for (k=k0; k<k1; k++) {
      printf("%d", det[k][j][0]);
      if (k==(k1-1)) printf("\n");
    }
  printf("\n");
}

void 
print_inpr(double inpr[2][ndop][ndop][nchan]) {
  int i,j;
  printf("inpr:\n");
  for (i=0; i<8; i++)
    for (j=0; j<nrange; j++)
      printf("%10.3f%c", inpr[0][i][j][0], (j==(nrange-1) ? '\n' : ' '));
}

void 
print_mag(double mag[ndop][nrange][nchan]) {
  int i,j;
  printf("mag:\n");

  for (i=0; i<8; i++)
    for (j=0; j<nrange; j++)
      printf("%10.3f%c", mag[i][j][0], (j==(nrange-1) ? '\n' : ' '));
}

