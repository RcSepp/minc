/*#include <fstream>

int main()
{
	std::ifstream f("foo.txt");
	f.close();
	return 0;
}*/

#include <stdio.h>

int main()
{
	FILE* f = fopen("foo.txt", "r");
	fclose(f);
	return 0;
}