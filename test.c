#include <assert.h>

#include "map.h"

int main (void) {
	Map map = MapAlloc(0);
	bool found;
	uint32_t keys[10000], cap, occ;

	// initialize keys
	for (uint32_t i = 0; i < 10000; ++i) keys[i] = i;

	// Test Put, Get, and Del without rehashing
	cap = map.cap;
	occ = map.rem - 1; // one more insertion would trigger a rehash
	for (uint32_t i = 0; i < occ; ++i) {
		Buf key = { sizeof keys[i], &keys[i] };
		Buf val;
		found = MapPut(&map, key, key);
		assert(!found);
		found = MapGet(map, key, &val);
		assert(found);
		assert(key.size == val.size);
		assert(key.p == val.p);
		assert(* (uint32_t *) val.p == i);
		found = MapPut(&map, key, key);
		assert(found);
	}
	assert(map.cap == cap);
	assert(map.rem == 1);
	assert(map.occ == occ);

	for (uint32_t i = 0; i < occ; ++i) {
		Buf key = { sizeof keys[i], &keys[i] };
		found = MapDel(&map, key);
		assert(found);
		found = MapGet(map, key, NULL);
		assert(!found);
	}
	assert(map.cap == cap);
	assert(map.rem == occ + 1);
	assert(map.occ == 0);

	// Test Put, Get, and Del with rehashing
	occ = 10000;
	for (uint32_t i = 0; i < occ; ++i) {
		Buf key = { sizeof keys[i], &keys[i] };
		Buf val;
		found = MapPut(&map, key, key);
		assert(!found);
		found = MapGet(map, key, &val);
		assert(found);
		assert(key.size == val.size);
		assert(key.p == val.p);
		assert(* (uint32_t *) val.p == i);
		found = MapPut(&map, key, key);
		assert(found);
	}
	assert(map.occ == occ);

	for (uint32_t i = 0; i < occ; ++i) {
		Buf key = { sizeof keys[i], &keys[i] };
		found = MapDel(&map, key);
		assert(found);
		found = MapGet(map, key, NULL);
		assert(!found);
	}
	assert(map.occ == 0);

	return 0;
}

