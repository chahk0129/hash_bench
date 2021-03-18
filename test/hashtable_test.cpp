#ifdef LIN
#include "index/linear_probing.h"
#elif defined EXT
#include "index/extendible_hash.h"
#else
#include "index/cuckoo_hash.h"
#endif
#include "util/pair.h"

#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <iterator>

using Key = char[64];
using Value = int64_t; 

int myrand(int i){
    return std::rand() % i;
}
template <typename Key_t>
void generate_int_workloads(char* arr, int num){
    struct Pair<Key_t>* input = (struct Pair<Key_t>*)arr;
    for(int i=0; i<num; i++){
	input[i].key = i+1;
	input[i].value = i+1;
    }
    std::random_shuffle(arr, arr+num);
}

template <typename Key_t>
void generate_string_workloads(char* arr, int num){
	struct Pair<Key_t>* input = (struct Pair<Key_t>*)arr;
	char s[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	for(int i=0; i<num; i++){
		int j;
		int key_len = rand() % sizeof(Key_t);
		if(key_len < 2) key_len = 2;
		for(j=0; j<key_len-1; j++)
			input[i].key[j] = s[rand() % (sizeof(s) -1)];
		input[i].key[j] = 0;
		input[i].value = i+1;
	}
}

int main(int argc, char* argv[]){
    const size_t initialTableSize = 1024 * 16;
    int numData = atoi(argv[1]);
    int numThreads = atoi(argv[2]);
    struct Pair<Key>* input = new struct Pair<Key>[numData];
#ifdef LIN
    Hash<Key>* hashtable = new LinearProbingHash<Key>(initialTableSize);
#elif defined EXT
    Hash<Key>* hashtable = new ExtendibleHash<Key>(initialTableSize/Segment<Key>::kNumSlot);
#else
    Hash<Key>* hashtable = new CuckooHash<Key>(initialTableSize);
#endif

    invalid_initialize<Key>();
    if constexpr(sizeof(Key) > 8)
	    generate_string_workloads<Key>((char*)input, numData);
    else
	    generate_int_workloads<int64_t>((char*)input, numData);

    vector<thread> inserts;
    vector<thread> searchs;
    vector<int> fail(numThreads);

    auto insert = [&hashtable, &input](int from, int to){
	for(int i=from; i<to; i++){
	    hashtable->Insert(input[i].key, input[i].value);
	}
    };

    auto search = [&hashtable, &input, &fail](int from, int to, int tid){
	int failed = 0;
	for(int i=from; i<to; i++){
	    auto ret = hashtable->Get(input[i].key);
	    if((Value_t)ret != input[i].value)
		failed++;
	}
	fail[tid] = failed;
    };

    int chunk_size = numData / numThreads;
    for(int i=0; i<numThreads; i++){
	if(i != numThreads-1)
	    inserts.emplace_back(thread(insert, chunk_size*i, chunk_size*(i+1)));
	else
	    inserts.emplace_back(thread(insert, chunk_size*i, numData));
    }
    for(auto& t: inserts) t.join();

    for(int i=0; i<numThreads; i++){
	if(i != numThreads-1)
	    searchs.emplace_back(thread(search, chunk_size*i, chunk_size*(i+1), i));
	else
	    searchs.emplace_back(thread(search, chunk_size*i, numData, i));
    }
    for(auto& t: searchs) t.join();

    int failedSearch = 0;
    for(auto& it: fail) failedSearch += it;
    std::cout << "failedSearhc: " << failedSearch << std::endl;


    return 0;
}
