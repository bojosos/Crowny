#include <iostream>
#include <string.h>

#define MAX_N 10000000

int NthPrime(int n)
{
	uint32_t primes = 1;
	if (n == 1)
		return 2;

	bool erat[MAX_N];

	memset(erat, false, MAX_N);
	
	for (int i = 0; i < MAX_N; i += 2)
	{
		erat[i] = true;
	}

	for (int i = 3; ; i+=2)
	{
		if (!erat[i])
		{
			for (int j = i; j < MAX_N; j += i)
				erat[j] = true;
			primes++;
		}
		
		if (primes == n)
			return i;
	}
	return 1;
}

int main ()
{
	int n;
	std::cin >> n;
	std::cout << NthPrime(n) << std::endl;

	return 0;
}
