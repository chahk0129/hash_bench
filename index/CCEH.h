#pragma once

#include <iostream>
#include <cmath>
#include <thread>
#include <bitset>
#include <cassert>
#include <unordered_map>
#include <sys/types.h>
#include <mutex>
#include <shared_mutex>
#include "util/pair.h"
#include "util/hash.h"
#include "util/interface.h"

#define f_seed 0xc70697UL
#define s_seed 0xc70697UL

const size_t kMask = 256-1;
const size_t kShift = 8;
const size_t kNumPairPerCacheLine = 4;
const size_t kNumCacheLine = 8;

using namespace std;

template <typename Key_t>
struct Segment{
    static const size_t kNumSlot = 1024;

    Segment(void): local_depth(0){ }
    Segment(size_t depth): local_depth(depth) { }
    ~Segment(void) { }
    
    bool Insert4split(Key_t&, Value_t, size_t);
    Segment<Key_t>** Split(void);

    Pair<Key_t> _[kNumSlot];
    size_t local_depth;
    shared_mutex mutex;
};

template <typename Key_t>
struct Directory{
    static const size_t kDefaultDepth = 10;
    Segment<Key_t>** _;
    int64_t sema;
    size_t capacity;
    size_t depth;

    bool suspend(void){
	int64_t val;
	do{
	    val = sema;
	    if(val < 0) return false;
	}while(!CAS(&sema, &val, -1));

	int64_t wait = 0 - val - 1;
	while(val && sema != wait){
	    asm("nop");
	}
	return true;
    }

    bool lock(void){
	int64_t val = sema;
	while(val > -1){
	    if(CAS(&sema, &val, val+1)) return true;
	    val = sema;
	}
	return false;
    }

    void unlock(void){
	int64_t val = sema;
	while(!CAS(&sema, &val, val-1))
	    val = sema;
    }

    Directory(void): depth(kDefaultDepth), sema(0){
	_ = new Segment<Key_t>*[capacity];
    }
    Directory(size_t _depth): depth(_depth), sema(0){
	_ = new Segment<Key_t>*[capacity];
    }
    ~Directory(void) { }
};

template <typename Key_t>
class CCEH : public Hash<Key_t>{
    public:
	CCEH(void);
	CCEH(size_t);
	~CCEH(void);
	void Insert(Key_t&, Value_t);
	bool Delete(Key_t&);
	char* Get(Key_t&);
	double Utilization(void);
	size_t Capacity(void);
	
    private:
	Directory<Key_t>* dir;
};

template <typename Key_t>
bool Segment<Key_t>::Insert4split(Key_t& key, Value_t value, size_t loc) {
    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i) {
	auto slot = (loc+i) % kNumSlot;
	if constexpr(sizeof(Key_t) > 8){
	    if(memcmp(_[slot].key, INVALID<Key_t>, sizeof(Key_t)) == 0){
		memcpy(_[slot].key, key, sizeof(Key_t));
		memcpy(&_[slot].value, &value, sizeof(Value_t));
		return true;
	    }
	}
	else{
	    if(memcmp(&_[slot].key, &INVALID<Key_t>, sizeof(Key_t)) == 0){
		memcpy(&_[slot].key, &key, sizeof(Key_t));
		memcpy(&_[slot].value, &value, sizeof(Value_t));
		return true;
	    }
	}
    }
    return false;
}

template <typename Key_t>
Segment<Key_t>** Segment<Key_t>::Split(void){
    Segment<Key_t>** split = new Segment<Key_t>*[2];
    split[0] = this;
    split[1] = new Segment<Key_t>(local_depth+1);

    auto pattern = ((size_t)1 << (sizeof(size_t)*8 - local_depth - 1));
    for (unsigned i = 0; i < kNumSlot; ++i) {
	size_t f_hash;
	if constexpr(sizeof(Key_t) > 8)
	    f_hash = hash_funcs[0](_[i].key, sizeof(Key_t), f_seed);
	else
	    f_hash = hash_funcs[0](&_[i].key, sizeof(Key_t), f_seed);

	if(f_hash & pattern){
	    if(!split[1]->Insert4split(_[i].key, _[i].value, (f_hash & kMask)*kNumPairPerCacheLine)){
		size_t s_hash;
		if constexpr(sizeof(Key_t) > 8)
		    s_hash = hash_funcs[2](_[i].key, sizeof(Key_t), s_seed);
		else
		    s_hash = hash_funcs[2](&_[i].key, sizeof(Key_t), s_seed);
		if(!split[1]->Insert4split(_[i].key, _[i].value, (s_hash & kMask)*kNumPairPerCacheLine)){
		    cerr << "[" << __func__ << "]: something wrong -- need to adjust probing distance" << endl;
		}
	    }
	}
    }

    return split;
}

    template <typename Key_t>
CCEH<Key_t>::CCEH(void)
    : dir{new Directory<Key_t>(0)}
{
    for (unsigned i = 0; i < dir->capacity; ++i) {
	dir->_[i] = new Segment<Key_t>(0);
    }

}

    template <typename Key_t>
CCEH<Key_t>::CCEH(size_t initCap)
    : dir{new Directory<Key_t>(static_cast<size_t>(log2(initCap)))}
{
    for (unsigned i = 0; i < dir->capacity; ++i) {
	dir->_[i] = new Segment<Key_t>(static_cast<size_t>(log2(initCap)));
    }
}

    template <typename Key_t>
CCEH<Key_t>::~CCEH(void)
{ }


template <typename Key_t>
void CCEH<Key_t>::Insert(Key_t& key, Value_t value) {
    size_t f_hash;
    if constexpr(sizeof(Key_t) > 8)
	f_hash = hash_funcs[0](key, sizeof(Key_t), f_seed);
    else
	f_hash = hash_funcs[0](&key, sizeof(Key_t), f_seed);
    auto f_idx = (f_hash & kMask) * kNumPairPerCacheLine;

RETRY:
    auto x = (f_hash >> (8*sizeof(f_hash) - dir->depth));
    auto target = dir->_[x];

    if(!target){
	std::this_thread::yield();
	goto RETRY;
    }

    /* acquire segment exclusive lock */
    if(!target->mutex.try_lock()){
	std::this_thread::yield();
	goto RETRY;
    }

    auto target_check = (f_hash >> (8*sizeof(f_hash) - dir->depth));
    if(target != dir->_[target_check]){
	target->mutex.unlock();
	std::this_thread::yield();
	goto RETRY;
    }

    auto target_local_depth = target->local_depth;
    auto pattern = (f_hash >> (8*sizeof(f_hash) - target->local_depth));
    for(unsigned i=0; i<kNumPairPerCacheLine * kNumCacheLine; ++i){
	auto loc = (f_idx + i) % Segment::kNumSlot;
	if constexpr(sizeof(Key_t) > 8){
	    if((((hash_funcs[0](target->_[loc].key, sizeof(Key_t), f_seed) >> (8*sizeof(f_hash)-target_local_depth)) != pattern) ||
			(memcmp(target->_[loc].key, INVALID<Key_t>, sizeof(Key_t)) == 0))){
		memcpy(target->_[loc].key, key, sizeof(Key_t));
		memcpy(&target->_[loc].value, &value, sizeof(Value_t));
		target->mutex.unlock();
		return;
	    }
	}
	else{
	    if((((hash_funcs[0](&target->_[loc].key, sizeof(Key_t), f_seed) >> (8*sizeof(f_hash)-target_local_depth)) != pattern) ||
			(memcmp(&target->_[loc].key, &INVALID<Key_t>, sizeof(Key_t)) == 0))){
		memcpy(&target->_[loc].key, &key, sizeof(Key_t));
		memcpy(&target->_[loc].value, &value, sizeof(Value_t));
		target->mutex.unlock();
		return;
	    }
	}

    }

    size_t s_hash;
    if constexpr(sizeof(Key_t) > 8)
	s_hash = hash_funcs[2](key, sizeof(Key_t), s_seed);
    else
	s_hash = hash_funcs[2](&key, sizeof(Key_t), s_seed);
    auto s_idx = (s_hash & kMask) * kNumPairPerCacheLine;

    for(unsigned i=0; i<kNumPairPerCacheLine * kNumCacheLine; ++i){
	auto loc = (s_idx + i) % Segment::kNumSlot;
	if constexpr(sizeof(Key_t) > 8){
	    if((((hash_funcs[0](target->_[loc].key, sizeof(Key_t), f_seed) >> (8*sizeof(f_hash)-target_local_depth)) != pattern) ||
			(memcmp(target->_[loc].key, INVALID<Key_t>, sizeof(Key_t)) == 0))){
		memcpy(target->_[loc].key, key, sizeof(Key_t));
		memcpy(&target->_[loc].value, &value, sizeof(Value_t));
		target->mutex.unlock();
		return;
	    }
	}
	else{
	    if((((hash_funcs[0](&target->_[loc].key, sizeof(Key_t), f_seed) >> (8*sizeof(f_hash)-target_local_depth)) != pattern) ||
			(memcmp(&target->_[loc].key, &INVALID<Key_t>, sizeof(Key_t)) == 0))){
		memcpy(&target->_[loc].key, &key, sizeof(Key_t));
		memcpy(&target->_[loc].value, &value, sizeof(Value_t));
		target->mutex.unlock();
		return;
	    }
	}
    }

    // COLLISION!!
    /* need to split segment but release the exclusive lock first to avoid deadlock */

    /* need to check whether the target segment has been split */
    Segment<Key_t>** s = target->Split();

DIR_RETRY:
    /* need to double the directory */
    if(target_local_depth == dir->depth){
	if(!dir->suspend()){
	    std::this_thread::yield();
	    goto DIR_RETRY;
	}

	x = (f_hash >> (8*sizeof(f_hash) - dir->depth));
	auto dir_old = dir;
	auto d = dir->_;
	auto _dir = new Directory<Key_t>(dir->depth+1);
	for(unsigned i = 0; i < dir->capacity; ++i){
	    if (i == x){
		_dir->_[2*i] = s[0];
		_dir->_[2*i+1] = s[1];
	    }
	    else{
		_dir->_[2*i] = d[i];
		_dir->_[2*i+1] = d[i];
	    }
	}
	dir = _dir;
	s[0]->local_depth++;
	s[0]->mutex.unlock();
    }
    else{ // normal segment split
	while(!dir->lock()){
	    asm("nop");
	}

	x = (f_hash >> (8 * sizeof(f_hash) - dir->depth));
	if(dir->depth == target_local_depth + 1){
	    if(x%2 == 0){
		dir->_[x+1] = s[1];
	    }
	    else{
		dir->_[x] = s[1];
	    }	    
	    dir->unlock();
	    s[0]->local_depth++;
	    /* release target segment exclusive lock */
	    s[0]->mutex.unlock();
	}
	else{
	    int stride = pow(2, dir->depth - target_local_depth);
	    auto loc = x - (x%stride);
	    for(int i=0; i<stride/2; ++i){
		dir->_[loc+stride/2+i] = s[1];
	    }
	    dir->unlock();
	    s[0]->local_depth++;
	    /* release target segment exclusive lock */
	    s[0]->mutex.unlock();
	}
    }
    std::this_thread::yield();
    goto RETRY;
}

// TODO
template <typename Key_t>
bool CCEH<Key_t>::Delete(Key_t& key) {
    return false;
}

template <typename Key_t>
Value_t CCEH::Get(Key_t& key) {
    size_t f_hash;
    if constexpr(sizeof(Key_t) > 8)
	f_hash = hash_funcs[0](key, sizeof(Key_t), f_seed);
    else
	f_hash = hash_funcs[0](&key, sizeof(Key_t), f_seed);
    auto f_idx = (f_hash & kMask) * kNumPairPerCacheLine;

RETRY:
    while(dir->sema < 0){
	asm("nop");
    }

    auto x = (f_hash >> (8*sizeof(f_hash) - dir->depth)); 
    auto target = dir->_[x];

    if(!target){
	std::this_thread::yield();
	goto RETRY;
    }

    /* acquire segment shared lock */
    if(!target->mutex.try_lock_shared()){
	std::this_thread::yield();
	goto RETRY;
    }

    auto target_check = (f_hash >> (8*sizeof(f_hash) - dir->depth));
    if(target != dir->_[target_check]){
	target->mutex.unlock();
	std::this_thread::yield();
	goto RETRY;
    }

    for (unsigned i = 0; i < kNumPairPerCacheLine * kNumCacheLine; ++i) {
	auto loc = (f_idx+i) % Segment::kNumSlot;
	if constexpr(sizeof(Key_t)>8){
	    if(memcmp(target->_[loc].key, key, sizeof(Key_t)) == 0){
		Value_t v = target->_[loc].value;
		target->mutex.unlock();
		return v;
	    }
	}
	else{
	    if(memcmp(&target->_[loc].key, &key, sizeof(Key_t)) == 0){
		Value_t v = target->_[loc].value;
		target->mutex.unlock();
		return v;
	    }
	}
    }

    size_t s_hash;
    if constexpr(sizeof(Key_t) > 8)
	s_hash = hash_funcs[2](key, sizeof(Key_t), s_seed);
    else
	s_hash = hash_funcs[2](&key, sizeof(Key_t), s_seed);
    auto s_idx = (s_hash & kMask) * kNumPairPerCacheLine;

    for(unsigned i=0; i<kNumPairPerCacheLine * kNumCacheLine; ++i){
	auto loc = (s_idx+i) % Segment::kNumSlot;
	if constexpr(sizeof(Key_t)>8){
	    if(memcmp(target->_[loc].key, key, sizeof(Key_t)) == 0){
		Value_t v = target->_[loc].value;
		target->mutex.unlock();
		return v;
	    }
	}
	else{
	    if(memcmp(&target->_[loc].key, &key, sizeof(Key_t)) == 0){
		Value_t v = target->_[loc].value;
		target->mutex.unlock();
		return v;
	    }
	}

    }

    target->mutex.unlock();
    return NONE;
}

template <typename Key_t>
double CCEH<Key_t>::Utilization(void){
    size_t sum = 0;
    size_t cnt = 0;
    for(size_t i=0; i<dir->capacity; cnt++){
	auto target = dir->_[i];
	auto stride = pow(2, dir->depth - target->local_depth);
	auto pattern = (i >> (dir->depth - target->local_depth));
	for(unsigned j=0; j<Segment::kNumSlot; ++j){
	    size_t key_hash;
	    if constexpr(sizeof(Key_t) > 8){
		key_hash = hash_funcs[0](target->_[j].key, sizeof(Key_t) ,f_seed);
		if(((key_hash >> (8*sizeof(size_t) - target->local_depth)) == pattern) && (memcmp(target->_[j].key, INVALID<Key_t>, sizeof(Key_t)) != 0)){
		    sum++;
		}
	    }
	    else{
		key_hash = hash_funcs[0](&target->_[j].key, sizeof(Key_t), f_seed);
		if(((key_hash >> (8*sizeof(key_hash)-target->local_depth)) == pattern) && (memcmp(&target->_[j].key, &INVALID<Key_t>, sizeof(Key_t)) != 0)){
		    sum++;
		}
	    }
	    i += stride;
	}
	return ((double)sum) / ((double)cnt * Segment::kNumSlot)*100.0;
    }


    template <typename Key_t>
	size_t CCEH::Capacity(void) {
	    size_t cnt = 0;
	    for(int i=0; i<dir->capacity; cnt++){
		auto target = dir->_[i];
		auto stride = pow(2, dir->depth - target->local_depth);
		i += stride;
	    }
	    return cnt * Segment::kNumSlot;
	}
