
#include "codearray.h"
#include "utils/utest.h"
#include <vector>

using namespace std;
using namespace mscds;

void test_rng() {
	const unsigned len = 10000;
	DeltaCodeArrBuilder bd;
	vector<unsigned> V;
	for (unsigned i = 0; i < len; ++i) {
		unsigned v = (rand() % 1000000) + 1;
		V.push_back(v);
		bd.add(v);
	}

	DeltaCodeArr a;
	bd.build(&a);
	for (unsigned i = 0; i < len; ++i) {
		if (V[i] != a[i]) {
			cout << V[i] << "  " << a[i] << endl;
			ASSERT_EQ(V[i], a[i]);
		}
	}
}

void test_rng2() {
	const unsigned len = 10000;
	DiffDeltaArrBuilder bd;
	vector<unsigned> V;
	for (unsigned i = 0; i < len; ++i) {
		unsigned v = (rand() % 1000000) + 1;
		V.push_back(v);
		bd.add(v);
	}

	DiffDeltaArr a;
	bd.build(&a);
	for (unsigned i = 0; i < len; ++i) {
		if (V[i] != a[i]) {
			cout << V[i] << "  " << a[i] << endl;
			ASSERT_EQ(V[i], a[i]);
		}
	}

}

int main() {
	test_rng();
	test_rng2();
	return 0;
}