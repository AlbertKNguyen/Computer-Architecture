#include <x86intrin.h>
#include <immintrin.h>
#include <string.h>
#include <stdio.h>
#define UNROLL (4)
#define BLOCKSIZE 32
#define min(a,b) (a < b ? a:b)

// Best Performing: SSE, Prefetch, Unrolling

/// SSE Instructions (had to add -mavx flag to makefile CFLAGS)
void dgemm(int m, int n, float *A, float *C) {
    __m256 c;
    int i;
    for (i = 0; i < m - 7; i += 8) {
        for (int k = 0; k < n; k++) {
            for (int j = 0; j < m; j++) {
                // c = C[i+j*m] (could further optimize by moving this loop under k loop then swapping j and k loop)
                c = _mm256_loadu_ps(C + i + j * m);
                // c = c + A[i+k*m] * A[j+k*m];
                c = _mm256_add_ps(c, _mm256_mul_ps(_mm256_loadu_ps(A + i + k * m),  _mm256_broadcast_ss(A + j + k * m)));
                // C[i+j*m] = c;
                _mm256_storeu_ps(C + i + j * m, c);
            }
        }
    }
    for (; i < m; i++) {
        for (int k = 0; k < n; k++) {
            for (int j = 0; j < m; j++) {
                C[i+j*m] += A[i+k*m] * A[j+k*m];
            }
        }
    }
    
}

/// Prefetch
/*
void dgemm(int m, int n, float *A, float *C) {
    float B[m];
    for (int i = 0; i < m; i++) {
        for (int k = 0; k < n; k++) {
            // Prefetch A[k*m]
            memcpy(B, &A[k*m], m * sizeof(float));
            for (int j = 0; j < m; j++) {
                C[i+j*m] += B[i] * B[j];
                printf("%f\n", C[i+j*m]);
            }
        }
    }
}

// Alternate prefetch attempt (does not work)
void dgemm(int m, int n, float *A, float *C) {
    for (int i = 0; i < m; i++) {
        for (int k = 0; k < n; k++) {
            __builtin_prefetch(&A[i+k*m]);
            for (int j = 0; j < m; j++) {
                C[i+j*m] += A[i+k*m] * A[j+k*m];
            }
        }
    }
}
*/

/// Unrolling (3 Iterations)
/*
// Unroll j 3 times
void dgemm(int m, int n, float *A, float *C) {
    int j;
    for (int i = 0; i < m; i++) {
        for (int k = 0; k < n; k++) {
            for (j = 0; j < m - 3; j += 4) {
                int index = i+k*m;
                C[i+j*m] += A[i+k*m] * A[j+k*m];
                C[i+(j+1)*m] += A[index] * A[j+1+k*m];
                C[i+(j+2)*m] += A[index] * A[j+2+k*m];
                C[i+(j+3)*m] += A[index] * A[j+3+k*m];
            }
            for (; j < m; j++) {
                C[i+j*m] += A[i+k*m] * A[j+k*m];
            }
        }
    }
}

// Unroll k 3 times
void dgemm(int m, int n, float *A, float *C) {
    int k;
    for(int i = 0; i < m; i++) {
        for(k = 0; k < n - 3; k += UNROLL) {
            for(int j = 0; j < m; j++) {
                int index = i+j*m;
                C[index] += A[i+k*m] * A[j+k*m];
                C[index] += A[i+(k+1)*m] * A[j+(k+1)*m];
                C[index] += A[i+(k+2)*m] * A[j+(k+2)*m];
                C[index] += A[i+(k+3)*m] * A[j+(k+3)*m];
            }
        }
        for(; k < n; k++) {
            for (int j = 0; j < m; j++) {
                C[i+j*m] += A[i+k*m] * A[j+k*m];
            }
        }
    }
}

// Unroll i 3 times
void dgemm(int m, int n, float *A, float *C) {
    int i;
    for(i = 0; i < m - 3; i += UNROLL) {
        for(int k = 0; k < n; k++) {
            for(int j = 0; j < m; j++) {
                int index = j+k*m;
                C[i+j*m] += A[i+k*m] * A[index];
                C[i+1+j*m] += A[i+1+k*m] * A[index];
                C[i+2+j*m] += A[i+2+k*m] * A[index];
                C[i+3+j*m] += A[i+3+k*m] * A[index];
            }
        }
    }
    for (; i < m; i++) {
        for (int k = 0; k < n; k++) {
            for (int j = 0; j < m; j++) {
                C[i+j*m] += A[i+k*m] * A[j+k*m];
            }
        }
    }
}
*/

/// Blocking (extra)
/*
// Version 1
void dgemm(int m, int n, float *A, float *C) {
    for (int ii = 0; ii < m; ii += BLOCKSIZE) {
        for (int kk = 0; kk < n; kk += BLOCKSIZE) {
            for (int i = ii; i < min(ii+BLOCKSIZE, m); i++) {
                for (int k = kk; k < min(kk+BLOCKSIZE, n); k++) {
                    for (int j = 0; j < m; j++) {
                        C[i+j*m] += A[i+k*m] * A[j+k*m];
                    }
                }
            }
        }
    }
}

// Version 2
void do_block (int m, int n, int si, int sj, int sk, float *A, float *C) {
    for (int i = si; i < min(si+BLOCKSIZE, m); i++) {
        for (int k = sk; k < min(sk+BLOCKSIZE, n); k++) {
            for(int j = sj; j < min(sj+BLOCKSIZE, m); j++) {
                C[i+j*m] += A[i+k*m] * A[j+k*m];
            }
        }
    }
} 

void dgemm (int m, int n, float *A, float *C) { 
    for (int i = 0; i < m; i += BLOCKSIZE)
        for (int k = 0; k < n; k += BLOCKSIZE) 
            for (int j = 0; j < m; j += BLOCKSIZE)
               do_block(m, n, i, j, k, A, C); 
} 
*/

// Reorder (Extra)
/*
void dgemm( int m, int n, float *A, float *C ) {
    for( int k = 0; k < n; k++ ) 
        for( int i = 0; i < m; i++ ) 
            for( int j = 0; j < m; j++ )
                C[i+j*m] += A[i+k*m] * A[j+k*m];
}
*/

