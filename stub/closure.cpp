int main()
{
	int i = 456;
	int x = 1;

	auto bar = [&]() {
		i = 789;
		x = 2;
	};
	bar();

	return i;
}