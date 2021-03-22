#include <unordered_set>
#include <set>
#include <cassert>
#include <iostream>
#include <memory>

#include "bench/benchmark.h"
#include "bench/utils.h"



enum Type{
    UNIFORM,
    ZIPFIAN,
    SELFSIMILAR
};

void SimpleTest(distribution_t type, size_t N, size_t size, const std::string& prefix = ""){
    auto bench = new benchmark_t(type, N, size, prefix);
    assert(bench->key_generator_->keyspace() == N);
    assert(bench->key_generator_->size() == size);
    assert(bench->key_generator_->get_seed() == 0);
    bench->key_generator_->set_seed(1729);
    assert(bench->key_generator_->get_seed() == 1729);
    
    std::unordered_set<uint64_t> key_space;
    // check if keys are key_generator_erated in sequence
    for(int i=1; i<=size; i++){
	const char* key = bench->key_generator_->next(true); 
	std::cout << "key: " << key << std::endl;
	uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
	std::cout << "keyint: " << key_int << std::endl;
	//assert(key_int == utils::multiplicative_hash<uint64_t>(i));

	auto ret = key_space.insert(key_int);
	assert(ret.second == true);
    }

    // check that each key_generator_erated key belongs to expected key space 
    for(int i=0; i<1e3; i++){
	const char* key = bench->key_generator_->next(false); 
	uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
	assert(key_space.count(key_int) == 1);
    }
}


int main(int argc, char* argv[]){
    distribution_t type = (distribution_t)atoi(argv[1]); // distribution type
    size_t N = atoi(argv[2]); // maximum boundary
    size_t size = atoi(argv[3]); // key size
    std::string prefix = "";
    if(argc > 4){
	prefix = argv[4];
    }
    SimpleTest(type, N, size, prefix);
    return 0;
}
