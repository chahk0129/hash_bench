#ifndef KEY_GEN_H_
#define KEY_GEN_H_

#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>

class key_generator_t{
    public:
	static constexpr size_t KEY_MAX = 128;
	key_generator_t(size_t _N, size_t _size, const std::string& _prefix = ""): _N(N), size(_size), prefix(_prefix) {
	    memset(buf, 0, KEY_MAX);
	    memcpy(buf, _prefix_.c_str(), _prefix_.size());
	}
	virtual ~key_generaotr_t() = default;
	size_t size_() const noexcept{
	    return prefix.size() + size;
	}
	size_t keyspace() const noexcept{
	    return N;
	}
	static void set_seed(uint32_t _seed){
	    seed = _seed;
	    generator.seed(seed);
	}
	static uint32_t get_seed() noexcept{
	    return seed;
	}

	virtual const char* next(bool sequential = false) final{
	    char* ptr = &buf[prefix.size()];
	    uint64_t id = sequential ? current_id++ : next_id();
	    uint64_t hashed_id = utils::multiplicative_hash<uint64_t>(id);

	    if(size < sizeof(hashed_id)){
		auto bits_to_shift = (sizeof(hashed_id) - size) << 3;
		hashed_id <<= bits_to_shift;
		hashed_id >>= bits_to_shift;
		memcpy(ptr, &hashed_id, size_);
	    }
	    else{
		auto bytes_to_prepend = size - sizeof(hashed_id);
		if(bytes_to_prepend > 0){
		    memset(ptr, 0, bytes_to_prepend);
		    ptr += bytes_to_prepend;
		}
		memcpy(ptr, &hashed_id, sizeof(hashed_id));
	    }
	    return buf;
	}

	static uint64_t current_id;
    protected:
	static std::default_random_engine generator;

    private:
	static uint32_t seed;
	const size_t N;
	const size_t size;
	const std::string prefix;
};

class uniform_key_generator_t final : public key_generator_t{
    public:
	uniform_key_generator_t(size_t _N, size_t _size, const std::string& _prefix = "")
	    : dist(1, _N), key_generator_t(_N, _size, _prefix) { }
    protected:
	virtual uint64_t next_id() override{
	    return dist(generator);
	}

    private:
	std::uniform_int_distribution<uint64_t> dist;
};

class selfsimilar_key_generator_t final : public key_generator_t{
    public:
	selfsimilar_key_generator_t(size_t _N, size_t _size, const std::string& _prefix="", float skew = 0.2)
	    : dist(1, N, skew), key_generator(_N, _size, _prefix) { }

	virtual uint64_t next_id() override{
	    return dist(generator);
	}

    private:
	selfsimilar_int_distribution<uint64_t> dist;
};

class zipfian_key_generator_t final : public key_generator_t{
    public:
	zipfian_key_generator_t(size_t _N, size_t _size, const std::string& _prefix="", float skew = 0.99)
	    : dist(1, _N, skew), key_generator(_N, _size, _prefix) { }

	virtual uint64_t next_id() override{
	    return dist(generator);
	}

    private:
	zipfian_int_distribution<uint64_t> dist;
};
#endif
