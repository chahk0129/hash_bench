#include <unordered_set>
#include <set>
#include <cassert>

#include "bench/key_generator.h"
#include "bench/utils.h"

template <class T>
class KeyGeneratorTest{
    public:
	KeyGeneratorTest(){ }
	~KeyGeneratorTest(){ }
	void Setup(){
	    seed_ = key_generator_t::get_seed();
	    current_id_ = key_generator_t::current_id;
	}

	void Teardown(){
	    key_generator_t::set_seed(seed_);
	    key_generator_t::current_id = current_id_;
	}

	std::unique_ptr<key_generator_t> Instantiate(size_t N, size_t size, const std::string& prefix = ""){
	    return std::make_unique<T>(N, size, prefix);
	}
    private:
	uint32_t seed_;
	uint64_t current_id_;

};

using uniform = uniform_key_generator_t;
using zipfian = zipfian_key_generator_t;
using selfsimilar = selfsimilar_key_generator_t;

template <typename T>
struct Type{
    using type__ = 
enum type_{
    UNIFORM,
    ZIPFIAN,
    SELFSIMILAR
};

void SimpleTest(int type, size_t N, size_t size, const std::string& prefix = ""){
    KeyGeneratorTest<shared_ptr<uniform, zipfian, selfsimilar>> test;
    char* test;
    if(type == UNIFORM)
	test = reinterpret_cast<KeyGeneratorTest<uniform>>(test);
    else if(type == ZIPFIAN)
	test = reinterpret_cast<KeyGeneratorTest<zipfian>>(test);
    else
	test = reinterpret_cast<KeyGeneratorTest<selfsimilar>>(test);

    //KeyGeneratorTest<distribution> test;
    auto gen = test.Instantiate(N, size, prefix);
    assert(gen->keyspace() == N);
    assert(gen->size() == size);
    assert(gen->get_seed() == 0);
    gen->set_seed(1729);
    assert(gen->get_seed() == 1729);

    std::unordered_set<uint64_t> key_space;
    // check if keys are generated in sequence
    for(int i=1; i<=size; i++){
	const char* key = gen->next(true); 
	uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
	assert(key_int == utils::multiplicative_hash<uint64_t>(i));

	auto ret = key_space.insert(key_int);
	assert(ret.second == true);
    }

    // check that each generated key belongs to expected key space 
    for(int i=0; i<1e3; i++){
	const char* key = gen->next(false); 
	uint64_t key_int = *reinterpret_cast<const uint64_t*>(key);
	assert(key_space.count(key_int) == 1);
    }

}

int main(int argc, char* argv[]){
    SimpleTest();
}
