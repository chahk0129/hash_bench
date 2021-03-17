#ifndef COMMON_H_
#define COMMON_H_
#define CAS(_p, _u, _v) (__atomic_compare_exchange_n (_p, _u, _v, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))
#include <cstdlib>
#include <cstring>

typedef int64_t Value_t;

template <typename Key_t>
struct Pair{
    Key_t key;
    Value_t value;

    Pair(void){
	if constexpr(sizeof(Key_t) > 8)
	    memset(key, 0, sizeof(Key_t));
	else
	    memset(&key, 0, sizeof(Key_t));
    }

    Pair(Key_t _key, Value_t _value){
	if constexpr(sizeof(Key_t) > 8)
	    memcpy(key, _key, sizeof(Key_t));
	else
	    memcpy(&key, &_key, sizeof(Key_t));
	memcpy(&value, &_value, sizeof(Value_t));
    }
};

template <typename Key_t>
Key_t INVALID;

const Value_t NONE = 0;

template <typename Key_t>
void invalid_initialize(void){
    if constexpr(sizeof(Key_t) > 8)
	memset(INVALID<Key_t>, 0, sizeof(Key_t));
    else
	memset(&INVALID<Key_t>, 0, sizeof(Key_t));
}
#endif

