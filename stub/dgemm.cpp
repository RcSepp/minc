#include <stdlib.h>
#include <stdio.h>

int main()
{
	int N = 512;//1024;
	int NN = N*N;

	// --j--   --k--   --j--
	// i C | = i A | * k B |
	// -----   -----   -----

	double* A = (double*)malloc(NN * 8);
	double* B = (double*)malloc(NN * 8);
	double* C = (double*)malloc(NN * 8);
	for (int i = 0; i != NN; i = i + 1) {
		A[i] = i;
	}
	for (int i = 0; i != NN; i = i + 1) {
		B[i] = i;
	}
	for (int i = 0; i != NN; i = i + 1) {
		C[i] = 0.0;
	}

	for (int i = 0; i != N; i = i + 1) {
		for (int j = 0; j != N; j = j + 1) {
			for (int k = 0; k != N; k = k + 1) {
				C[i * N + j] = C[i * N + j] + A[i * N + k] * B[k * N + j];
			}
		}
	}

	double fresult = C[N] * 0.000000001;
	int result = fresult;
	//free(toCharPtr(ptr));
	return result;
}