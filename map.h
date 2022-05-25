#pragma once

/* A pointer-indirect open-addressing hash map using the FNV-1 hash function,
Robin Hood linear probing, and true deletion. */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MapCapDefault 16 // Default initial capacity when calling MapAlloc(0)
#define MapCapFactor   2 // Capacity scaling factor
#define MapLoadHigh  .9f // Strictly less than 1! Rehash after load reaches this factor.
#define MapLoadLow   .5f // Scale capacity until load falls below this factor.
                         // (for example, doubling will reduce load from ~90% to ~45%)

typedef struct {
	uint32_t  size ; // in bytes
	void    * p    ;
} Buf;

typedef struct {
	Buf key ;
	Buf val ;
} Slot;

typedef struct {
	uint32_t  cap   ; // capacity
	uint32_t  occ   ; // occupancy
	uint32_t  rem   ; // remaining load (how many insertions before rehashing)
	uint8_t * probe ; // probe lengths (0 means empty, 1 means non-displaced)
	Slot    * slot  ; // slots have no memory initialization requirement
} Map;

Map MapAlloc (uint32_t cap) {
	if (!cap) cap = MapCapDefault;
	return (Map)
		{ .cap   = cap
		, .rem   = cap * MapLoadHigh
		, .probe = calloc(cap , sizeof (uint8_t)) // all slots start empty
		, .slot  = malloc(cap * sizeof (Slot)) };
}

bool MapCheck (Map map)
{ return map.slot && map.probe && map.rem + map.occ < map.cap; }

void MapFree (Map * pMap) {
	assert(MapCheck(*pMap));
	free(pMap->probe);
	free(pMap->slot);
}

uint64_t HashFNV1 (uint32_t size, uint8_t p[static const size]) {
	assert(!size || p);
	static uint64_t const FNVOffsetBasis = 1LLU; // any arbitrary non-zero value
	static uint64_t const FNVPrime64 = 0x100000001b3LLU; // freaking mathematics!
	uint64_t hash = FNVOffsetBasis;
	for (uint32_t i = 0; i < size; ++i) {
		hash *= FNVPrime64;
		hash ^= p[i];
	}
	return hash;
}

bool KeyMatch (Buf a, Buf b)
{ return a.size == b.size && !memcmp(a.p, b.p, b.size); }

bool MapSearch (Map map, Buf key, uint32_t * pI) {
	assert(MapCheck(map));
	bool found = true;
	uint32_t i = HashFNV1(key.size, key.p) % map.cap, probe = 1;
	/* Robin Hood search: if the probe is greater than that of the current slot,
	the search terminates.*/
	while (probe <= map.probe[i]) {
		assert(probe <= map.occ);
		if (KeyMatch(map.slot[i].key, key)) goto Found;
		i = (i + 1) % map.cap;
		++probe;
	}
	found = false;
	Found:
	if (pI) *pI = i;
	return found;
}

bool MapGet (Map map, Buf key, Buf * pVal) {
	uint32_t i;
	if (!MapSearch(map, key, &i)) return false;
	if (pVal) *pVal = map.slot[i].val;
	return true;
}

bool MapDel (Map * pMap, Buf key) {
	uint32_t i;
	if (!MapSearch(*pMap, key, &i)) return false;
	pMap->probe[i] = 0;
	/* Robin Hood deletion: move every displaced slot to the previous slot until
	a non-displaced or empty slot is encountered. Robin Hood insertion guarantees
	key hashes of displaced slots are not interleaved, such as is possible in
	FCFS insertion. */
	uint32_t j = (i + 1) % pMap->cap;
	while (pMap->probe[j] > 1) {
		pMap->slot[i] = pMap->slot[j];
		pMap->probe[i] = pMap->probe[j] - 1;
		pMap->probe[j] = 0;
		i = j;
		j = (j + 1) % pMap->cap;
	}
	--pMap->occ;
	++pMap->rem;
	return true;
}

void MapRehash (Map * pMap, uint32_t cap);

bool MapPut (Map * pMap, Buf key, Buf val) {
	uint32_t i;
	if (MapSearch(*pMap, key, &i)) {
		pMap->slot[i].val = val;
		return true;
	}
	struct { uint32_t probe; Slot slot; } swap;
	Slot slot = { key, val };
	uint32_t probe = 1;
	i = HashFNV1(key.size, key.p) % pMap->cap;
	/* Robin Hood insertion: if the probe is greater than that of the current
	slot, swap the probe and insertion slot for that of the curent slot. Continue
	to do so until an empty slot is found, finally inserting into the empty slot. */
	while (pMap->probe[i]) {
		assert(probe <= pMap->occ);
		if (probe > pMap->probe[i]) {
			swap.probe = pMap->probe[i];
			swap.slot = pMap->slot[i];
			pMap->probe[i] = probe;
			pMap->slot[i] = slot;
			probe = swap.probe;
			slot = swap.slot;
		}
		i = (i + 1) % pMap->cap;
		++probe;
	}
	pMap->probe[i] = probe;
	pMap->slot[i] = slot;
	++pMap->occ;
	// Rehash if there is no more remaining load capacity.
	if (pMap->rem <= 1) {
		uint32_t cap = pMap->cap;
		while ((float) pMap->occ / cap > MapLoadLow)
			cap *= MapCapFactor;
		MapRehash(pMap, cap);
	} else --pMap->rem;
	return false;
}

void MapRehash (Map * pMap, uint32_t cap) {
	assert(MapCheck(*pMap));
	assert(pMap->occ < cap);
	Map map = MapAlloc(cap);
	for (uint32_t i = 0; map.occ < pMap->occ; ++i) if (pMap->probe[i])
		MapPut(&map, pMap->slot[i].key, pMap->slot[i].val);
	assert(map.occ == pMap->occ);
	MapFree(pMap);
	*pMap = map;
}
