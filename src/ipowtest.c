#include <stdio.h>
#include "utils.h"
	
static uint64_t ipow_u64(uint64_t base, size_t exp){ \
	uint64_t fact = (uint32_t)1;
	if (exp == 0) return fact;
	while (exp != 1){
		if (exp & 1){ fact *= base; }
		base *= base;
		exp >>= 1u;
	}
	return base * fact;
}


int main(){
	double base = 10.0;
	for (size_t i=0; i!=20; i+=1){
		uint64_t res = ipow_u64(10, i);
		printf("%lf ** %zu = %lu\n", base, i, res);
	}
	return 0;
}
