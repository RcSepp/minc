#include <cstdio>

int main()
{
	int* arr = new int[3];
	int i1 = 1;
	arr[0] = 123;
	arr[i1] = 456;
	printf("%i %i %i\n", arr[0], arr[1], arr[2]);

	return arr[1];
}