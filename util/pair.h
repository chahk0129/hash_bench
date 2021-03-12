#ifndef COMMON_H_
#define COMMON_H_

#define CAS(_p, _u, _v) (__atomic_compare_exchange_n (_p, _u, _v, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))

template <typename Key_t, typename Value_t>
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
	if constexpr(sizeof(Value_t) > 8)
	    memcpy(value, _value, sizeof(Value_t));
	else
	    memcpy(&value, &_value, sizeof(Value_t));
    }
};

template <typename Key_t>
Key_t INVALID;

template <typename Value_t>
Value_t NONE;

template <typename Key_t, typename Value_t>
void invalid_initialize(void){
    if constexpr(sizeof(Key_t) > 8)
	memset(INVALID<Key_t>, 0, sizeof(Key_t));
    else
	memset(&INVALID<Key_t>, 0, sizeof(Key_t));
    if constexpr(sizeof(Value_t) > 8)
	memset(NONE<Value_t>, 0, sizeof(Value_t));
    else
	memset(&NONE<Value_t>, 0, sizeof(Value_t));
}
#endif

