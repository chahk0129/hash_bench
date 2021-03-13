#ifndef EXTENDIBLE_PTR_H_
#define EXTENDIBLE_PTR_H_

#include <iostream>
#include <cmath>
#include <bitset>
#include <unordered_map>
#include <cstring>
#include <vector>
#include <thread>
#include <shared_mutex>
#include "util/pair.h"
#include "util/hash.h"
#include "util/interface.h"

using namespace std;

const size_t kMask = 256-1;
const size_t kShift = 8;

template <typename Key_t, typename Value_t>
struct Block {
  // static const size_t kBlockSize = 256; // 4 - 1
  // static const size_t kBlockSize = 1024; // 16 - 1
  // static const size_t kBlockSize = 4*1024; // 64 - 1
  // static const size_t kBlockSize = 16*1024; // 256 - 1
  // static const size_t kBlockSize = 64*1024; // 1024 - 1
  static const size_t kBlockSize = 256*1024; // 4096 - 1
  static const size_t kNumSlot = kBlockSize/sizeof(Pair<Key_t, Value_t>);

  Block(void)
  : local_depth{0}
  { }

  Block(size_t depth)
  :local_depth{depth}
  { }

  ~Block(void) {
  }

  Block<Key_t, Value_t>** Split(void);

  Pair<Key_t, Value_t> _[kNumSlot];
  size_t local_depth;
  std::shared_mutex mutex;
  size_t numElem(void); 
};

template <typename Key_t, typename Value_t>
struct Directory {
  static const size_t kDefaultDepth = 10;
  Block<Key_t, Value_t>** _;
  size_t global_depth;
  size_t capacity;
  int64_t sema = 0 ;

  Directory(void) {
    global_depth = kDefaultDepth;
    capacity = pow(2, global_depth);
    _ = new Block<Key_t, Value_t>*[capacity];
    sema = 0;
  }

  Directory(size_t depth) {
    global_depth = depth;
    capacity = pow(2, global_depth);
    _ = new Block<Key_t, Value_t>*[capacity];
    sema = 0;
  }

  ~Directory(void) {
    delete [] _;
  }

  void SanityCheck(void*);
  void LSBUpdate(int, int, int, int, Block<Key_t, Value_t>**);

  bool suspend(void){
      int64_t val;
      do{
	  val = sema;
	  if(val < 0)
	      return false;
      }while(!CAS(&sema, &val, -1));

      int64_t wait = 0 - val -1;
      while(val && sema != wait){
	  asm("nop");
      }
      return true;
  }

  bool acquire(void){
      int64_t val = sema;
      while(val > -1){
	  if(CAS(&sema, &val, val+1))
	      return true;
	  val = sema;
      }
      return false;
  }

  void release(void){
      int64_t val = sema;
      while(!CAS(&sema, &val, val-1)){
	  val = sema;
      }
  }
};

template <typename Key_t, typename Value_t>
class ExtendibleHash : public Hash<Key_t, Value_t> {
  public:
    ExtendibleHash(void): dir{new Directory<Key_t, Value_t>(0)}, {
	for(int i=0; i<dir->capacity; i++){
	    dir->_[i] = new Block<Key_t, Value_t>(0);
	}
    }

    ExtendibleHash(size_t _capacity): dir{new Directory<Key_t, Value_t>(log2(_capacity)} {
	for(int i=0; i<dir->capacity; i++){
	    dir->_[i] = new Block<Key_t, Value_t>(static_cast<size_t>(log2(_capacity)));
	}
    }

    ~ExtendibleHash(void){ }
    void Insert(Key_t&, Value_t);
    bool Delete(Key_t&);
    char* Get(Key_t&);
    double Utilization(void){
	size_t sum = 0;
	unordered_map<Block<Key_t, Value_t>*, bool> set;
	for(int i=0; i<dir->capacity; i++)
	    set[dir->_[i]] = true;
	for(auto& iter: set){
	    for(int i=0; i<Block<Key_t, Value_t>::kNumSlot; i++){
		if constexpr(sizeof(Key_t) > 8){
		    if(memcmp(iter.first->_[i].key, INVALID<Key_t>, sizeof(Key_t)) != 0)
			sum++;
		}
		else{
		    if(memcmp(&iter.first->_[i].key, &INVALID<Key_t>, sizeof(Key_t)) != 0)
			sum++;
		}
	    }
	}
	return ((double)sum) / ((double)set.size() * Block<Key_t, Value_t>::kNumSlot) * 100.0;
    }

    size_t Capacity(void){
	unordered_map<Block<Key_t, Value_t>*, bool> set;
	for(int i=0; i<dir->capacity; i++)
	    set[dir->_[i]] = true;
	return set.size() * Block<Key_t,Value_t>::kNumSlot;
    }


  private:
    Directory<Key_t, Value_t>* dir;
};


template <typename Key_t, typename Value_t>
Block<Key_t, Value_t>** Block<Key_t, Value_t>::Split(void){
     Block<Key_t, Value_t>** s = new Block<Key_t, Value_t>*[2];
     s[0] = new Block<Key_t, Value_t>(local_depth+1);
     s[1] = new Block<Key_t, Value_t>(local_depth+1);

     auto pattern = ((size_t)1 << local_depth);
     int left =0, right = 0;
     for(int i=0; i<kNumSlot; i++){
	 size_t key_hash;
	 if constexpr(sizeof(Key_t) > 8){
	     key_hash = h(_[i].key, sizeof(Key_t));
	     if(key_hash & pattern){
		 memcpy(s[1]->_[right].key, _[i].key, sizeof(Key_t));
		 if constexpr(sizeof(Value_t) > 8)
		     memcpy(s[1]->_[right].value, _[i].value, sizeof(Value_t));
		 else
		     memcpy(&s[1]->_[right].value, &_[i].value, sizeof(Value_t));
		 right++;
	     }
	     else{
		 memcpy(s[0]->_[left].key, _[i].key, sizeof(Key_t));
		 if constexpr(sizeof(Value_t) > 8)
		     memcpy(s[0]->_[left].value, _[i].value, sizeof(Value_t));
		 else
		     memcpy(&s[0]->_[left].value, &_[i].value, sizeof(Value_t));
		 left++;
	     }
	 }
	 else{
	     key_hash = h(&_[i].key, sizeof(Key_t));
	     if(key_hash & pattern){
		 memcpy(&s[1]->_[right].key, &_[i].key, sizeof(Key_t));
		 if constexpr(sizeof(Value_t) > 8)
		     memcpy(s[1]->_[right].value, _[i].value, sizeof(Value_t));
		 else
		     memcpy(&s[1]->_[right].value, &_[i].value, sizeof(Value_t));
		 right++;
	     }
	     else{
		 memcpy(&s[0]->_[left].key, &_[i].key, sizeof(Key_t));
		 if constexpr(sizeof(Value_t) > 8)
		     memcpy(s[0]->_[left].value, _[i].value, sizeof(Value_t));
		 else
		     memcpy(&s[0]->_[left].value, &_[i].value, sizeof(Value_t));
		 left++;
	     }
	 }
     }
     return s;
}

   
template <typename Key_t, typename Value_t>
void ExtendibleHash<Key_t, Value_t>::Insert(Key_t& key, Value_t value){
    size_t key_hash;
    if constexpr(sizeof(Key_t) > 8)
	key_hash = h(key, sizeof(Key_t));
    else
	key_hash = h(&key, sizeof(Key_t));

RETRY:
    auto x = (key_hash % dir->capacity);
    auto target = dir->_[x];

    if(!target->mutex.try_lock()){
	std::this_thread::yield();
	goto RETRY;
    }

    auto target_check = (key_hash % dir->capacity);
    if(target != dir->_[target_check]){
	target->mutex.unlock();
	std::this_thread::yield();
	goto RETRY;
    }

    auto pattern = (key_hash % (size_t)pow(2, target->local_depth));
    for(int i=0; i<kNumSlot; i++){
	auto loc = (key_hash + i) % Block<Key_t, Value_t>::kNumSlot;
	if constexpr(sizeof(Key_t) > 8){
	    if(((h(target->_[loc].key, sizeof(Key_t)) % (size_t)pow(2, target->local_depth)) != pattern)
		|| (memcmp(target->_[loc].key, INVALID<Key_t>, sizeof(Key_t)) == 0)){
		memcpy(target->_[loc].key, key, sizeof(Key_t));
		if constexpr(sizeof(Value_t) > 8)
		    memcpy(target->_[loc].value, value, sizeof(Value_t));
		else
		    memcpy(&target->_[loc].value, &value, sizeof(Value_t));
		target->mutex.unlock();
		return;
	    }
	}
	else{
	    if(((h(&target->_[loc].key, sizeof(Key_t)) % (size_t)pow(2, target->local_depth)) != pattern)
		|| (memcmp(&target->_[loc].key, &INVALID<Key_t>, sizeof(Key_t)) == 0)){
		memcpy(&target->_[loc].key, &key, sizeof(Key_t));
		if constexpr(sizeof(Value_t) > 8)
		    memcpy(target->_[loc].value, value, sizeof(Value_t));
		else
		    memcpy(&target->_[loc].value, &value, sizeof(Value_t));
		target->mutex.unlock();
		return;
	    }
	}
    }

    Block<Key_t, Value_t>** s = target->Split();

DIR_RETRY:
    if(target->local_depth == dir->global_depth){
        if(!dir->suspend()){
	    std::this_thread::yield();
	    goto DIR_RETRY;
	}

	x = (key_hash % dir->capacity);
	auto dir_old = dir;
	auto blocks = dir->_;
	auto _dir = new Directory<Key_t, Value_t>(dir->global_depth+1);
	memcpy(_dir->_, blocks, sizeof(Block<Key_t, Value_t>*)*dir->capacity);
	memcpy(_dir->_+dir->capacity,  blocks, sizeof(Block<Key_t, Value_t>*)*dir->capacity);
	_dir[x] = s[0];
	_dir[x+dir->capacity] = s[1];

	dir = _dir;
	delete dir_old;
    }
    else{
	if(!dir->acquire()){
	    std::this_thread::yield();
	    goto DIR_RETRY;
	}

	x = (key_hash % dir->capacity);
	dir->LSBUpdate(s[0]->local_depth, dir->capacity, x, s);
	dir->release();
    }
    goto RETRY;
}


template <typename Key_t, typename Value_t>
void Directory<Key_t, Value_t>::LSBUpdate(int local_depth, int dir_cap, int x, Block<Key_t, Value_t>** s){
    int depth_diff = global_depth - local_depth;
    if(depth_diff == 0){
	if((x % dir_cap) >= dir_cap/2){
	    _[x-dir_cap/2] = s[0];
	    _[x] = s[1];
	}
	else{
	    _[x] = s[0];
	    _[x-dir_cap/2] = s[1];
	}
    }
    else{
	if((x % dir_cap) >= dir_cap/2){
	    LSBUpdate(local_depth+1, dir_cap/2, x-dir_cap/2, s);
	    LSBUpdate(local_depth+1, dir_cap/2, x, s);
	}
	else{
	    LSBUpdate(local_depth+1, dir_cap/2, x, s);
	    LSBUpdate(local_depth+1, dir_cap/2, x-dir_cap/2, s);
	}
    }
}

template <typename Key_t, typename Value_t>
char* ExtendibleHash<Key_t, Value_t>::Get(Key_t& key){
    size_t key_hash;
    if constexpr(sizeof(Key_t) > 8)
	key_hash = h(key, sizeof(Key_t));
    else
	key_hash = h(&key, sizeof(Key_t));

RETRY:
    auto x = (key_hash % dir->capacity);
    auto target = dir->_[x];
    if(!target->mutex.try_lock_shared()){
	std::this_thread::yield();
	goto RETRY;
    }

    auto target_check = (key_hash % dir->capacity);
    if(target != dir->_[target_check]){
	std::this_thread::yield();
	goto RETRY;
    }

    for(int i=0; i<Block<Key_t, Value_t>::kNumSlot; i++){
	if constexpr(sizeof(Key_t) > 8){
	    if(memcmp(dir->_[i].key, key, sizeof(Key_t))){
		target->unlock();
		return (char*)dir->_[i].value;
	    }
	}
	else{
	    if(memcmp(&dir->_[i].key, &key, sizeof(Key_t))){
		target->unlock();
		return (char*)dir->_[i].value;
	    }
	}
    }
    target->unlock();
    return (char*)NONE<Value_t>;
}

	
template <typename Key_t, typename Value_t>
bool ExtendibleHash<Key_t, Value_t::Delete(Key_t& key){
    return true;
}


#endif  // EXTENDIBLE_PTR_H_
