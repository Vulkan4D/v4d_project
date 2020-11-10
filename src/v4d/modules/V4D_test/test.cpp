#define _V4D_MODULE
#include <v4d.h>

int var1 = 0;

extern "C" {
	void init() {
		var1 = 2;
	}
	int test1(double a) {
		var1 += 5;
		return a == 3.46 ? 0:1;
	}
	int test2() {
		return 7 - var1;
	}
}
