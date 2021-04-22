#pragma once

#include "bench/key_generator.h"
#include "bench/value_generator.h"
#include "index/interface.h"
#include "util/pair.h"
#include "util/config.h"
#include "pcm/cpucounters.h"

#include <iostream>
#include <cstdint>
#include <memory>
#include <vector>
#include <string>

enum class distribution_t: uint8_t{
    UNIFORM = 0,
    SELFSIMILAR = 1,
    ZIPFIAN = 2
};

template <typename Key_t>
class benchmark_t{
    public:
	benchmark_t(bool enable_pcm){
	    if(enable_pcm){
		pcm = PCM::getInstance();
		auto status = pcm->program();
		if(status != PCM::Success){
		    std::cout << "Error opening PCM: " << status << std::endl;
		    exit(0);
		}
	    }
	    else
		pcm = nullptr;
	}
	benchmark_t(distribution_t type, size_t key_space, size_t key_size, const std::string& prefix = ""){
	    switch(type){
		case distribution_t::UNIFORM:
		    std::cout << __func__ << ": uniform_key_generator" << std::endl;
		    key_generator_ = std::make_unique<uniform_key_generator_t>(key_space, key_size, prefix);
		    break;
		case distribution_t::SELFSIMILAR:
		    std::cout << __func__ << ": selfsimilar_key_generator" << std::endl;
		    key_generator_ = std::make_unique<selfsimilar_key_generator_t>(key_space, key_size, prefix);
		    break;
		case distribution_t::ZIPFIAN:
		    std::cout << __func__ << ": zipfian_key_generator" << std::endl;
		    key_generator_ = std::make_unique<zipfian_key_generator_t>(key_space, key_size, prefix);
		    break;
		default:
		    std::cerr << "[ERROR]: unknown distribution type" << std::endl;
		    exit(0);
	    }
	    value_generator_ = new value_generator_t((const uint32_t)(sizeof(uint64_t)));
       	}
	~benchmark_t(){ }

	std::unique_ptr<key_generator_t> key_generator_;
	value_generator_t* value_generator_;

	Hash<Key_t>* hashtable;
	PCM* pcm;

	size_t mem_usage(void);
	inline void microbench(int index_type, Pair<Key_t>* init_kv, int init_num, bool insert_only);
	inline void latency(int index_type, Pair<Key_t>* init_kv, int init_num, int num_threads);
	inline void ycsb_load(int workload_type, int index_type, Pair<Key_t>* init_kv, int init_num, Pair<Key_t>* run_kv, int run_num, int* ops);
	inline void ycsb_exec(int workload_type, int index_type, Pair<Key_t>* init_kv, int init_num, Pair<Key_t>* run_kv, int run_num, int num_threads, int* ops, bool pcm_nabled);
	//inline void ycsb_exec(int workload_type, int index_type, Pair<Key_t>* init_kv, int init_num, Pair<Key_t>* run_kv, int run_num, int num_threads, int* ops, bool memory_bandwidth, bool numa);
    //private:
};
