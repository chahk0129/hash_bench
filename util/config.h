#ifndef CONFIG_H__
#define CONFIG_H__

#include <thread>
#include <sys/time.h>
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "index/cuckoo_hash.h"
#include "index/linear_probing.h"
#include "index/extendible_hash.h"
using keytype = uint64_t;

bool hyperthreading = true;

enum{
    TYPE_EXTENDIBLE_HASH,
    TYPE_LINEAR_HASH,
    TYPE_CUCKOO_HASH
};

enum{
    OP_INSERT,
    OP_READ,
    OP_UPDATE,
    OP_DELETE
};

enum{
    WORKLOAD_A,
    WORKLOAD_B,
    WORKLOAD_C,
    WORKLOAD_D
};

enum{
    NUMERIC_KEY,
    STRING_KEY
};

template <std::size_t keySize>
class GenericKey {
    public:
	char data[keySize];
    public:
	inline void setFromString(std::string key) {
	    memset(data, 0, keySize);
	    if(key.size() >= keySize) {
		memcpy(data, key.c_str(), keySize - 1);
		data[keySize - 1] = '\0';
	    } else {
		strcpy(data, key.c_str());
	    }

	    return;
	}

	// Constructor - Fills it with 0x00
	// This is for the skiplist to initialize an empty node
	GenericKey(int) { memset(data, 0x00, keySize); }
	GenericKey() { memset(data, 0x00, keySize); }
	// Copy constructor
	GenericKey(const GenericKey &other) { memcpy(data, other.data, keySize); }
	inline GenericKey &operator=(const GenericKey &other) {
	    memcpy(data, other.data, keySize);
	    return *this;
	}

	inline bool operator<(const GenericKey<keySize> &other) { return strcmp(data, other.data) < 0; }
	inline bool operator>(const GenericKey<keySize> &other) { return strcmp(data, other.data) > 0; }
	inline bool operator==(const GenericKey<keySize> &other) { return strcmp(data, other.data) == 0; }
	// Derived operators
	inline bool operator!=(const GenericKey<keySize> &other) { return !(*this == other); }
	inline bool operator<=(const GenericKey<keySize> &other) { return !(*this > other); }
	inline bool operator>=(const GenericKey<keySize> &other) { return !(*this < other); }
};


template <typename Key_t>
Hash<Key_t>* getInstance(const int index_type){
    const size_t initialTableSize = 1024*16;
    if(index_type == TYPE_EXTENDIBLE_HASH)
	return new ExtendibleHash<Key_t>(initialTableSize/Segment<Key_t>::kNumSlot);
    else if(index_type == TYPE_LINEAR_HASH)
	return new LinearProbingHash<Key_t>(initialTableSize);
    else if(index_type == TYPE_CUCKOO_HASH)
	return new CuckooHash<Key_t>(initialTableSize);
    else
	fprintf(stderr, "unkown index type %d\n", index_type);
    return nullptr;
}

inline void clear_cache(void){
#ifndef DEBUG
    int* dummy = new int[1024*1024*256];
    for(int i=0; i<1024*1024*256; i++){
	dummy[i] = i;
    }
    for(int i=100; i<1024*1024*256-100; i++){
	dummy[i] = dummy[i-rand()%100] + dummy[i+rand()%100];
    }

    delete[] dummy;
#endif
}

inline double get_now(void){
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static int core_alloc_map_numa[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26,
    1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27,
    28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54,
    29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55
};


static int core_alloc_map_hyper[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18,
    20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54,
    1, 3, 5, 7 ,9, 11, 13, 15, 17, 19,
    21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55
};

constexpr static size_t MAX_CORE_NUM = 56;

inline void pin_core(size_t tid){
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);

    size_t core_id = tid % MAX_CORE_NUM;
    if(hyperthreading)
	CPU_SET(core_alloc_map_hyper[core_id], &cpu_set);
    else
	CPU_SET(core_alloc_map_numa[core_id], &cpu_set);

    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set), &cpu_set);
    if(ret != 0){
	fprintf(stderr, "%s returns non-zero\n", __func__);
	exit(1);
    }
    return;
}

template <typename Key_t, typename Fn, typename... Args>
void start_threads(Hash<Key_t> *hash_p, uint64_t num_threads, Fn &&fn, Args &&...args) {
    std::vector<std::thread> thread_group;

    auto fn2 = [hash_p, &fn](uint64_t thread_id, Args ...args) {
	pin_core(thread_id);
	fn(thread_id, args...);
	return;
    };

    for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
	thread_group.push_back(std::thread{fn2, thread_itr, std::ref(args...)});
    }

    for (uint64_t thread_itr = 0; thread_itr < num_threads; ++thread_itr) {
	thread_group[thread_itr].join();
    }
    return;
}
#endif
