#pragma once

#include "bench/key_generator.h"
#include "bench/value_generator.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

enum class distribution_t: uint8_t{
    UNIFORM = 0,
    SELFSIMILAR = 1,
    ZIPFIAN = 2
};

class benchmark_t{
    public:
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
    //private:
};
