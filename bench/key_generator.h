#ifndef KEY_GENERATOR_H_
#define KEY_GENERATOR_H_

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

#include "bench/selfsimilar_distribution.h"
#include "bench/zipfian_distribution.h"
#include "bench/utils.h"

class key_generator_t{
    public:
	static constexpr size_t KEY_MAX = 128;
	key_generator_t(size_t N, size_t size, const std::string& prefix = "")
	    : _N(N), _size(size), _prefix(prefix) {
	    memset(_buf, 0, KEY_MAX);
	    memcpy(_buf, _prefix.c_str(), _prefix.size());
	}
	virtual ~key_generator_t() = default;
	size_t size() const noexcept{
	    return _prefix.size() + _size;
	}
	size_t keyspace() const noexcept{
	    return _N;
	}
	static void set_seed(uint32_t seed){
	    _seed = seed;
	    generator.seed(_seed);
	}
	static uint32_t get_seed() noexcept{
	    return _seed;
	}

	virtual const char* next(bool sequential = false) final{
	    char* ptr = &_buf[_prefix.size()];
	    uint64_t id = sequential ? current_id++ : next_id();
	    uint64_t hashed_id = utils::multiplicative_hash<uint64_t>(id);

	    if(_size < sizeof(hashed_id)){
		auto bits_to_shift = (sizeof(hashed_id) - _size) << 3;
		hashed_id <<= bits_to_shift;
		hashed_id >>= bits_to_shift;
		memcpy(ptr, &hashed_id, _size);
	    }
	    else{
		auto bytes_to_prepend = _size - sizeof(hashed_id);
		if(bytes_to_prepend > 0){
		    memset(ptr, 0, bytes_to_prepend);
		    ptr += bytes_to_prepend;
		}
		memcpy(ptr, &hashed_id, sizeof(hashed_id));
	    }
	    return _buf;
	}

	static uint64_t current_id;
    protected:
	virtual uint64_t next_id() = 0;
	static std::default_random_engine generator;

    private:
	static uint32_t _seed;
	static char _buf[KEY_MAX];
	const size_t _N;
	const size_t _size;
	const std::string _prefix;
};

class uniform_key_generator_t final : public key_generator_t{
    public:
	uniform_key_generator_t(size_t N, size_t size, const std::string& prefix = "")
	    : dist(1, N), key_generator_t(N, size, prefix) { }
    protected:
	virtual uint64_t next_id() override{
	    return dist(generator);
	}

    private:
	std::uniform_int_distribution<uint64_t> dist;
};

class selfsimilar_key_generator_t final : public key_generator_t{
    public:
	selfsimilar_key_generator_t(size_t N, size_t size, const std::string& prefix="", float skew = 0.2)
	    : dist(1, N, skew), key_generator_t(N, size, prefix) { }

	virtual uint64_t next_id() override{
	    return dist(generator);
	}

    private:
	selfsimilar_int_distribution<uint64_t> dist;
};

class zipfian_key_generator_t final : public key_generator_t{
    public:
	zipfian_key_generator_t(size_t _N, size_t _size, const std::string& _prefix="", float skew = 0.99)
	    : dist(1, _N, skew), key_generator_t(_N, _size, _prefix) { }

	virtual uint64_t next_id() override{
	    return dist(generator);
	}

    private:
	zipfian_int_distribution<uint64_t> dist;
};
#endif
