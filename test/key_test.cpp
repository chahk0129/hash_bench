#include <unordered_set>
#include <set>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>

#include "bench/benchmark.h"
#include "bench/utils.h"



void clear_cache(){
    int* dummy = new int[1024*1024*256];
    for(int i=0; i<1024*1024*256; i++){
	dummy[i] = i+1;
    }
    for(int i=100; i<1024*1024*256-100; i++){
	dummy[i] = dummy[i-rand()%100] + dummy[i+rand()%100];
    }
    delete[] dummy;
}

void SimpleTest(distribution_t type, size_t N, size_t size, const std::string& prefix = ""){
    auto bench = new benchmark_t(type, N, size, prefix);
    assert(bench->key_generator_->keyspace() == N);
    std::cout << "key_size: " << size << "\tbench_key_size: " << bench->key_generator_->size() << std::endl;
    assert(bench->key_generator_->size() == size+prefix.size());
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

void WorkloadTest(distribution_t type, size_t N, size_t size, const std::string& prefix = ""){

    auto bench = new benchmark_t(type, N, size, prefix);
    assert(bench->key_generator_->keyspace() == N);
    std::cout << "key_size: " << size << "\tbench_key_size: " << bench->key_generator_->size() << std::endl;
    assert(bench->key_generator_->size() == size+prefix.size());
    assert(bench->key_generator_->get_seed() == 0);
    bench->key_generator_->set_seed(1729);
    assert(bench->key_generator_->get_seed() == 1729);
    bench->value_generator_->set_seed(1729);
    assert(bench->value_generator_->get_seed() == 1729);

    switch(type){
	case distribution_t::UNIFORM:
	    std::cout << "UNIFORM workload" << std::endl;
	    break;
	case distribution_t::SELFSIMILAR:
	    std::cout << "SELFSIMILAR workload" << std::endl;
	    break;
	case distribution_t::ZIPFIAN:
	    std::cout << "ZIPFIAN workload" << std::endl;
	    break;
	default:
	    std::cout << "unknown workload type" << std::endl;
	    exit(0);
    }


    clear_cache();
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<N; i++){
	const char* key = bench->key_generator_->next(false);
	const char* value = bench->value_generator_->next();
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed = (end.tv_nsec - start.tv_nsec) + (end.tv_sec - start.tv_sec)*1000000000;

    std::cout << "elapsed time(msec): " << elapsed/1000000.0 << std::endl;
    std::cout << "throghput: " <<  (uint64_t)(1000000*(N/(elapsed/1000.0))) << "\tops/sec" << std::endl;

}

 

int main(int argc, char* argv[]){
    distribution_t type = (distribution_t)atoi(argv[1]); // distribution type
    size_t N = atoi(argv[2]); // maximum boundary
    size_t size = atoi(argv[3]); // key size
    std::string prefix = "";
    if(argc > 4){
	prefix = argv[4];
	std::cout << "argv[4]: " << argv[4] << "\tprefix: " << prefix << std::endl;
    }
    WorkloadTest(type, N, size, prefix);
    //SimpleTest(type, N, size, prefix);
    return 0;
}
