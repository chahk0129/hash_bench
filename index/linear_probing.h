#ifndef LINEAR_HASH_H_
#define LINEAR_HASH_H_

#include <iostream>
#include <cstring>
#include <stddef.h>
#include <mutex>
#include <shared_mutex>

#include "util/hash.h"
#include "util/pair.h"
#include "util/interface.h"

using namespace std;

template <typename Key_t>
class LinearProbingHash : public Hash <Key_t> {
  const float kResizingFactor = 2;
  const float kResizingThreshold = 0.95;
  public:
    LinearProbingHash(void): capacity{0}, dict{nullptr}{ }
    LinearProbingHash(size_t _capacity): capacity{_capacity}, dict{new Pair<Key_t>[capacity]} {
	locksize = 256;
	nlocks = capacity / locksize + 1;
	mutex = new shared_mutex[nlocks];
    }
    ~LinearProbingHash(void){
	if(dict != nullptr) delete[] dict;
    }

    void Insert(Key_t&, Value_t);
    bool Delete(Key_t&);
    char* Get(Key_t&);
    void findanyway(Key_t&);
    double Utilization(void){
	size_t size = 0;
	for(int i=0; i<capacity; i++){
	    if constexpr(sizeof(Key_t) > 8){
		if(memcmp(dict[i].key, INVALID<Key_t>, sizeof(Key_t)) != 0)
		    size++;
	    }
	    else{
		if(memcmp(&dict[i].key, &INVALID<Key_t>, sizeof(Key_t)) != 0)
		    size++;
	    }
	}
	return ((double)size) / ((double)capacity)*100;
    }

    size_t Capacity(void) {
      return capacity;
    }

  private:
    void resize(size_t);
    size_t getLocation(size_t, size_t, Pair<Key_t>*);

    size_t capacity;
    Pair<Key_t>* dict;

    size_t old_cap;
    Pair<Key_t>* old_dic;

    size_t size = 0;

    int resizing_lock = 0;
    std::shared_mutex *mutex;
    int nlocks;
    int locksize;
};

template <typename Key_t>
void LinearProbingHash<Key_t>::Insert(Key_t& key, Value_t value){
    uint64_t key_hash;
    if constexpr(sizeof(Key_t) > 8)
	key_hash = h(key, sizeof(Key_t));
    else
	key_hash = h(&key, sizeof(Key_t));

RETRY:
    while(resizing_lock){
	asm("nop");
    }

    auto loc = key_hash;
    if(size < capacity*kResizingThreshold){
	int i = 0;
	while(i < capacity){
	    auto slot = (loc + i) % capacity;
	    unique_lock<shared_mutex> lock(mutex[slot/locksize]);
	    do{
		if constexpr(sizeof(Key_t) > 8){
		    if(memcmp(dict[slot].key, INVALID<Key_t>, sizeof(Key_t)) == 0){
			memcpy(dict[slot].key, key, sizeof(Key_t));
			memcpy(&dict[slot].value, &value, sizeof(Value_t));
			auto _size = size;
			while(!CAS(&size, &_size, _size+1)){
			    _size = size;
			}
			return;
		    }
		}
		else{
		    if(memcmp(&dict[slot].key, &INVALID<Key_t>, sizeof(Key_t)) == 0){
			memcpy(&dict[slot].key, &key, sizeof(Key_t));
			memcpy(&dict[slot].value, &value, sizeof(Value_t));
			auto _size = size;
			while(!CAS(&size, &_size, _size+1)){
			    _size = size;
			}
			return;
		    }
		}
		i++;
		slot = (loc + i) % capacity;
		if(!(i < capacity)) break;
	    }while(slot % locksize != 0);
	}
    }else{
	auto unlocked = 0;
	if(CAS(&resizing_lock, &unlocked, 1)){
	    resize(capacity * kResizingFactor);
	    resizing_lock = 0;
	}
    }
    goto RETRY;
}

template <typename Key_t>
bool LinearProbingHash<Key_t>::Delete(Key_t& key){
    uint64_t key_hash;
    if constexpr(sizeof(Key_t) > 8)
	key_hash = h(key, sizeof(Key_t));
    else
	key_hash = h(&key, sizeof(Key_t));

RETRY:
    while(resizing_lock){
	asm("nop");
    }
    for(int i=0; i<capacity; i++){
	auto loc = (key_hash + i) % capacity;
	unique_lock<shared_mutex> lock(mutex[loc/locksize]);
	if constexpr(sizeof(Key_t) > 8){
	    if(memcmp(dict[loc].key, key, sizeof(Key_t)) == 0){
		memcpy(dict[loc].key, INVALID<Key_t>, sizeof(Key_t));
		return true;
	    }
	}
	else{
	    if(memcmp(&dict[loc].key, &key, sizeof(Key_t)) == 0){
		memcpy(&dict[loc].key, &INVALID<Key_t>, sizeof(Key_t));
		return true;
	    }
	}
    }
    return false;
}

template <typename Key_t>
char* LinearProbingHash<Key_t>::Get(Key_t& key){
    uint64_t key_hash;
    if constexpr(sizeof(Key_t) > 8)
	key_hash = h(key, sizeof(Key_t));
    else
	key_hash = h(&key, sizeof(Key_t));

RETRY:
    while(resizing_lock){
	asm("nop");
    }
    for(int i=0; i<capacity; i++){
	auto loc = (key_hash + i) % capacity;
	shared_lock<shared_mutex> lock(mutex[loc/locksize]);
	if constexpr(sizeof(Key_t) > 8){
	    if(memcmp(dict[loc].key, key, sizeof(Key_t)) == 0)
		return (char*)dict[loc].value;
	}
	else{
	    if(memcmp(&dict[loc].key, &key, sizeof(Key_t)) == 0)
		return (char*)dict[loc].value;
	}
    }
    return (char*)NONE;
}

template <typename Key_t>
size_t LinearProbingHash<Key_t>::getLocation(size_t hash_value, size_t _capacity, Pair<Key_t>* _dict){
    size_t cur = hash_value;
    int i = 0;
    if constexpr(sizeof(Key_t) > 8){
	while(memcmp(_dict[cur].key, INVALID<Key_t>, sizeof(Key_t)) != 0){
	    cur = (cur+1) % _capacity;
	    i++;
	    if(!(i < _capacity))
		return -1;
	}
	return cur;
    }
    else{
	while(memcmp(&_dict[cur].key, &INVALID<Key_t>, sizeof(Key_t)) != 0){
	    cur = (cur+1) % _capacity;
	    i++;
	    if(!(i < _capacity))
		return -1;
	}
	return cur;
    }
}

template <typename Key_t>
void LinearProbingHash<Key_t>::resize(size_t _capacity){
	cout << "RESIZE" << endl;
    unique_lock<shared_mutex>* lock[nlocks];
    for(int i=0; i<nlocks; i++){
	lock[i] = new unique_lock<shared_mutex>(mutex[i]);
    }
    int prev_nlocks = nlocks;
    nlocks = _capacity / locksize + 1;
    shared_mutex* old_mutex = mutex;

    Pair<Key_t>* new_dict = new Pair<Key_t>[_capacity];
    for(int i=0; i<capacity; i++){
	if constexpr(sizeof(Key_t) > 8){
	    if(memcmp(dict[i].key, INVALID<Key_t>, sizeof(Key_t)) != 0){
		auto key_hash = h(dict[i].key, sizeof(Key_t)) % _capacity;
		auto loc = getLocation(key_hash, _capacity, new_dict);
		memcpy(&new_dict[loc], &dict[i], sizeof(Pair<Key_t>));
	    }
	}
	else{
	    if(memcmp(&dict[i].key, &INVALID<Key_t>, sizeof(Key_t)) != 0){
		auto key_hash = h(&dict[i].key, sizeof(Key_t)) % _capacity;
		auto loc = getLocation(key_hash, _capacity, new_dict);
		memcpy(&new_dict[loc], &dict[i], sizeof(Pair<Key_t>));
	    }
	}
    }
    mutex = new shared_mutex[nlocks];
    old_cap = capacity;
    old_dic = dict;
    capacity = _capacity;
    dict = new_dict;
    auto tmp = old_dic;
    old_cap = 0;
    old_dic = nullptr;
    for(int i=0; i<prev_nlocks; i++){
	delete lock[i];
    }
    delete[] old_mutex;
    //delete[] tmp;
}

template <typename Key_t>
void LinearProbingHash<Key_t>::findanyway(Key_t& key){
	for(int i=0; i<capacity; i++){
		if constexpr(sizeof(Key_t) > 8){
			if(memcmp(dict[i].key, key, sizeof(Key_t)) == 0){
				cout << "FOUND: " << dict[i].key << "\t" << key << endl;
				return;
			}
		}
		else{
			if(memcmp(&dict[i].key, &key, sizeof(Key_t)) == 0){
				cout << "FOUND: " << dict[i].key << "\t" << key << endl;
				return;
			}
		}
	}
	cout << "NOT FOUND for key " << key << endl;
}

#endif  // LINEAR_HASH_H_
