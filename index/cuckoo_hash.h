#pragma once
#include <stddef.h>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <iostream>  // debugging
#include <cstring>
#include <algorithm>
#include "util/pair.h"
#include "util/hash.h"
#include "util/interface.h"

using namespace std;

template <typename Key_t>
class CuckooHash : public Hash<Key_t> {
  size_t _seed = 0xc70f6907UL;
  const size_t kCuckooThreshold = 16;
  const size_t kNumHash = 2;
  const float kResizingFactor = 2;
  const size_t kMaxGrows = 128;

  public:
    CuckooHash(void): capacity{0}, table{nullptr} {
        memset(&pushed, 0, sizeof(Pair<Key_t>)*2);
    }

    CuckooHash(size_t _capacity): capacity{_capacity}, table{new Pair<Key_t>[capacity]} {
        memset(&pushed, 0, sizeof(Pair<Key_t>)*2);
        locksize = 256;
        nlocks = capacity / locksize + 1;
        mutex = new std::shared_mutex[nlocks];
    }

    ~CuckooHash(void){
        delete[] table;
    }

    void Insert(Key_t&, Value_t);
    bool Delete(Key_t&);
    char* Get(Key_t&);
    double Utilization(void);
    size_t Capacity(void){ return capacity;} 

  private:
    bool insert4resize(Key_t&, Value_t);
    bool resize(void);
    std::vector<std::pair<size_t, Key_t>> find_path(size_t);
    bool validate_path(std::vector<size_t>&);
    bool execute_path(std::vector<std::pair<size_t,Key_t>>&);
    bool execute_path(std::vector<std::pair<size_t,Key_t>>&, Key_t&, Value_t);

    size_t capacity;
    Pair<Key_t>* table;
    Pair<Key_t> pushed[2];
    Pair<Key_t> temp;

    size_t old_cap;
    Pair<Key_t>* old_tab;

    int resizing_lock = 0;
    std::shared_mutex *mutex;
    int nlocks;
    int locksize;
};

template <typename Key_t>
void CuckooHash<Key_t>::Insert(Key_t& key, Value_t value) {
  size_t f_hash, s_hash;
  if constexpr(sizeof(Key_t) > 8){
      f_hash = hash_funcs[0](key, sizeof(Key_t), _seed);
      s_hash = hash_funcs[1](key, sizeof(Key_t), _seed);
  }
  else{
      f_hash = hash_funcs[0](&key, sizeof(Key_t), _seed);
      s_hash = hash_funcs[1](&key, sizeof(Key_t), _seed);
  }


RETRY:
  while (resizing_lock == 1) {
    asm("nop");
  }
  auto f_idx = f_hash % capacity;
  auto s_idx = s_hash % capacity;


  {
    unique_lock<shared_mutex> f_lock(mutex[f_idx/locksize]);
    if constexpr(sizeof(Key_t) > 8){
	if(memcmp(table[f_idx].key, INVALID<Key_t>, sizeof(Key_t)) == 0){
	    memcpy(table[f_idx].key, key, sizeof(Key_t));
	    memcpy(&table[f_idx].value, &value, sizeof(Value_t));
	    return;
	}
    }
    else{
	if(memcmp(&table[f_idx].key, &INVALID<Key_t>, sizeof(Key_t)) == 0){
	    memcpy(&table[f_idx].key, &key, sizeof(Key_t));
	    memcpy(&table[f_idx].value, &value, sizeof(Value_t));
	    return;
	}
    }
  }
  {
    unique_lock<shared_mutex> s_lock(mutex[s_idx/locksize]);
    if constexpr(sizeof(Key_t) > 8){
	if(memcmp(table[s_idx].key, INVALID<Key_t>, sizeof(Key_t)) == 0){
	    memcpy(table[s_idx].key, key, sizeof(Key_t));
	    memcpy(&table[s_idx].value, &value, sizeof(Value_t));
	    return;
	}
    }
    else{
	if(memcmp(&table[s_idx].key, &INVALID<Key_t>, sizeof(Key_t)) == 0){
	    memcpy(&table[s_idx].key, &key, sizeof(Key_t));
	    memcpy(&table[s_idx].value, &value, sizeof(Value_t));
	    return;
	}
    }

  }

  { // Failed to insert... Doing Cuckooing...
    int unlocked = 0;
    if (CAS(&resizing_lock, &unlocked, 1)) {
PATH_RETRY:
      auto path1 = find_path(f_idx);
      auto path2 = find_path(s_idx);
      if (path1.size() != 0 || path2.size() != 0) {
        auto path = &path1;
	if constexpr(sizeof(Key_t) > 8){
	    if(path1.size() == 0
	        || (path2.size() != 0 && path2.size() < path1.size())
		|| (path2.size() != 0 && memcmp(path1[0].second, INVALID<Key_t>, sizeof(Key_t)) == 0)){
		path = &path2;
	    }
	}
	else{
	    if(path1.size() == 0
	        || (path2.size() != 0 && path2.size() < path1.size())
		|| (path2.size() != 0 && memcmp(&path1[0].second, &INVALID<Key_t>, sizeof(Key_t)) == 0)){
		path = &path2;
	    }
	}

        auto id = 0;
        vector<size_t> lock_loc;
        for (auto p: *path) {
          lock_loc.push_back(p.first/locksize);
        }
        sort(begin(lock_loc), end(lock_loc));
        lock_loc.erase( unique( lock_loc.begin(), lock_loc.end() ), lock_loc.end() );
        unique_lock<shared_mutex> *lock[kCuckooThreshold];
        for (auto i :lock_loc) {
          lock[id++] = new unique_lock<shared_mutex>(mutex[i]);
        }
        for (auto p : *path) {
	  if constexpr(sizeof(Key_t) > 8){
	      if(memcmp(table[p.first].key, p.second, sizeof(Key_t)) != 0){
		  for(int i=0; i<id; i++)
		      delete lock[i];
		  goto PATH_RETRY;
	      }
	  }
	  else{
	      if(memcmp(&table[p.first].key, &p.second, sizeof(Key_t)) != 0){
		  for(int i=0; i<id; i++)
		      delete lock[i];
		  goto PATH_RETRY;
	      }
	  }

        }
        resizing_lock = 0;
        execute_path(*path, key, value);
        for (int i = 0; i < id; ++i) {
          delete lock[i];
        }

        return;
      } else {
        resize();
        resizing_lock = 0;
      }
    }
  }
  goto RETRY;
}

template <typename Key_t>
bool CuckooHash<Key_t>::insert4resize(Key_t& key, Value_t value) {
  size_t f_hash, s_hash;
  if constexpr(sizeof(Key_t) > 8){
      f_hash = hash_funcs[0](key, sizeof(Key_t), _seed);
      s_hash = hash_funcs[1](key, sizeof(Key_t), _seed);
  }
  else{
      f_hash = hash_funcs[0](&key, sizeof(Key_t), _seed);
      s_hash = hash_funcs[1](&key, sizeof(Key_t), _seed);
  }

  auto f_idx = f_hash % capacity;
  auto s_idx = s_hash % capacity;

  if constexpr(sizeof(Key_t) > 8){
      if(memcmp(table[f_idx].key, INVALID<Key_t>, sizeof(Key_t)) == 0){
	  memcpy(table[f_idx].key, key, sizeof(Key_t));
	  memcpy(&table[f_idx].value, &value, sizeof(Value_t));
      }
      else if(memcmp(table[s_idx].key, INVALID<Key_t>, sizeof(Key_t)) == 0){
	  memcpy(table[s_idx].key, key, sizeof(Key_t));
	  memcpy(&table[s_idx].value, &value, sizeof(Value_t));
      }
      else{
	  auto path1 = find_path(f_idx);
	  auto path2 = find_path(s_idx);
	  memcpy(pushed[0].key, key, sizeof(Key_t));
	  memcpy(&pushed[0].value, &value, sizeof(Value_t));
	  if(path1.size() == 0 && path2.size() == 0)
		  return false;
	  else{
		  if(path1.size() == 0)
			  execute_path(path2);
		  else if(path2.size() == 0)
			  execute_path(path1);
		  else if(path1.size() < path2.size())
			  execute_path(path1);
		  else
			  execute_path(path2);
	  }
      }
  }
  else{
      if(memcmp(&table[f_idx].key, &INVALID<Key_t>, sizeof(Key_t)) == 0){
	  memcpy(&table[f_idx].key, &key, sizeof(Key_t));
	  memcpy(&table[f_idx].value, &value, sizeof(Value_t));
      }
      else if(memcmp(&table[s_idx].key, &INVALID<Key_t>, sizeof(Key_t)) == 0){
	  memcpy(&table[s_idx].key, &key, sizeof(Key_t));
	  memcpy(&table[s_idx].value, &value, sizeof(Value_t));
      }
      else{
	  auto path1 = find_path(f_idx);
	  auto path2 = find_path(s_idx);
	  memcpy(&pushed[0].key, &key, sizeof(Key_t));
	  memcpy(&pushed[0].value, &value, sizeof(Value_t));
	  if(path1.size() == 0 && path2.size() == 0)
		  return false;
	  else{
		  if(path1.size() == 0)
			  execute_path(path2);
		  else if(path2.size() == 0)
			  execute_path(path1);
		  else if(path1.size() < path2.size())
			  execute_path(path1);
		  else
			  execute_path(path2);
	  }
      }
  }

  return true;
}

template <typename Key_t>
vector<pair<size_t,string>> CuckooHash<Key_t>::find_path(size_t target, bool flag) {
  vector<pair<size_t,string>> path;
  path.reserve(kCuckooThreshold);
  string_view key_v(table[target].key);
  path.emplace_back(target, key_v);
  auto cur = target;
  auto i = 0;
  do {
    size_t f_hash, s_hash;
    string_view key(table[cur].key);
    if(memcmp(key.c_str(), INVALID<Key_t>, sizeof(Key_t)) == 0) break;
    f_hash = hash_funcs[0](key.c_str(), sizeof(Key_t), _seed);
    s_hash = hash_funcs[1](key.c_str(), sizeof(Key_t), _seed);

    auto f_idx = f_hash % capacity;
    auto s_idx = s_hash % capacity;

    if (f_idx == cur) {
	    string_view s_idx_key(table[s_idx].key);
      path.emplace_back(s_idx, s_idx_key);
      cur = s_idx;
    } else if (s_idx == cur) {
	    string_view f_idx_key(table[f_idx].key);
      path.emplace_back(f_idx, f_idx_key);
      cur = f_idx;
    } else {  // something terribly wrong
      cout << "E: " << f_idx << " " << s_idx << " " << cur << " " << target << endl;
      cout << key << endl;
      exit(1);
    }
    i++;
  } while (i < kCuckooThreshold);

  if (i == kCuckooThreshold) {
    path.resize(0);
  }

  return move(path);
}



template <typename Key_t>
vector<pair<size_t,Key_t>> CuckooHash<Key_t>::find_path(size_t target) {
  vector<pair<size_t,Key_t>> path;
  path.reserve(kCuckooThreshold);
  path.emplace_back(target, table[target].key);
  auto cur = target;
  auto i = 0;
  do {
    Key_t* key;
    size_t f_hash, s_hash;
    key = &table[cur].key;
	    if(memcmp(key, &INVALID<Key_t>, sizeof(Key_t)) == 0) break;
	    f_hash = hash_funcs[0](key, sizeof(Key_t), _seed);
	    s_hash = hash_funcs[1](key, sizeof(Key_t), _seed);

    auto f_idx = f_hash % capacity;
    auto s_idx = s_hash % capacity;

    if (f_idx == cur) {
      path.emplace_back(s_idx, table[s_idx].key);
      cur = s_idx;
    } else if (s_idx == cur) {
      path.emplace_back(f_idx, table[f_idx].key);
      cur = f_idx;
    } else {  // something terribly wrong
      cout << "E: " << f_idx << " " << s_idx << " " << cur << " " << target << endl;
      cout << *key << endl;
      exit(1);
    }
    i++;
  } while (i < kCuckooThreshold);

  if (i == kCuckooThreshold) {
    path.resize(0);
  }

  return move(path);
}

template <typename Key_t>
bool CuckooHash<Key_t>::execute_path(vector<pair<size_t,Key_t>>& path) {
  auto i = 0;
  auto j = (i+1)%2;

  for (auto p: path) {
	  memcpy(&pushed[j], &table[p.first], sizeof(Pair<Key_t>));
	  memcpy(&table[p.first], &pushed[i], sizeof(Pair<Key_t>));
    //pushed[j] = table[p.first];
    //table[p.first] = pushed[i];
    i = (i+1)%2;
    j = (i+1)%2;
  }
  return true;
}

template <typename Key_t>
bool CuckooHash<Key_t>::execute_path(vector<pair<size_t,Key_t>>& path, Key_t& key, Value_t value) {
  for (int i = path.size()-1; i > 0; --i) {
	  memcpy(&table[path[i].first], &table[path[i-1].first], sizeof(Pair<Key_t>));
    //table[path[i].first] = table[path[i-1].first];
  }
  memcpy(&table[path[0].first].value, &value, sizeof(Value_t));
  if constexpr(sizeof(Key_t) > 8)
	  memcpy(table[path[0].first].key, key, sizeof(Key_t));
  else
	  memcpy(&table[path[0].first].key, &key, sizeof(Key_t));
  return true;
}

template <typename Key_t>
bool CuckooHash<Key_t>::Delete(Key_t& key) {
    size_t f_hash, s_hash;
    if constexpr(sizeof(Key_t) > 8){
        f_hash = hash_funcs[0](key, sizeof(Key_t), _seed);
        s_hash = hash_funcs[1](key, sizeof(Key_t), _seed);
    }
    else{
        f_hash = hash_funcs[0](&key, sizeof(Key_t), _seed);
        s_hash = hash_funcs[1](&key, sizeof(Key_t), _seed);
    }

RETRY:
    while(resizing_lock){
        asm("nop");
    }

    auto f_idx = f_hash % capacity;
    auto s_idx = s_hash % capacity;

    { // try first hashing
        unique_lock<shared_mutex> lock(mutex[f_idx/locksize]);
        if constexpr(sizeof(Key_t) > 8){
            if(memcmp(table[f_idx].key, key, sizeof(Key_t)) == 0){
                memcpy(table[f_idx].key, INVALID<Key_t>, sizeof(Key_t));
                return true;
            }
        }
        else{
            if(memcmp(&table[f_idx].key, &key, sizeof(Key_t)) == 0){
                memcpy(&table[f_idx].key, &INVALID<Key_t>, sizeof(Key_t));
                return true;
            }
        }
    }

    { // try second hashing
        unique_lock<shared_mutex> lock(mutex[s_idx/locksize]);
        if constexpr(sizeof(Key_t) > 8){
            if(memcmp(table[s_idx].key, key, sizeof(Key_t)) == 0){
                memcpy(table[s_idx].key, INVALID<Key_t>, sizeof(Key_t));
                return true;
            }
        }
        else{
            if(memcmp(&table[s_idx].key, &key, sizeof(Key_t)) == 0){
                memcpy(&table[s_idx].key, &INVALID<Key_t>, sizeof(Key_t));
                return true;
            }
        }
    }
                                                                           
  return false;
}

template <typename Key_t>
char* CuckooHash<Key_t>::Get(Key_t& key) {
  if (resizing_lock) {
    for (unsigned i = 0; i < kNumHash; ++i) {
	    int idx;
	    if constexpr(sizeof(Key_t) > 8)
		    idx = hash_funcs[i](key, sizeof(Key_t), _seed) % old_cap;
	    else
		    idx = hash_funcs[i](&key, sizeof(Key_t), _seed) % old_cap;
      std::shared_lock<std::shared_mutex> lock(mutex[idx/locksize]);
      if constexpr(sizeof(Key_t) > 8){
	      if(memcmp(old_tab[idx].key, key, sizeof(Key_t)) == 0)
		      return (char*)old_tab[idx].value;
      }
      else
	      if(memcmp(&old_tab[idx].key, &key, sizeof(Key_t)) == 0)
		      return (char*)old_tab[idx].value;
      }

  } else {
    for (int i = 0; i < kNumHash; i++) {
	    int idx;
	    if constexpr(sizeof(Key_t) > 8)
		    idx = hash_funcs[i](key, sizeof(Key_t), _seed) % capacity;
	    else
		    idx = hash_funcs[i](&key, sizeof(Key_t), _seed) % capacity;
      std::shared_lock<std::shared_mutex> lock(mutex[idx/locksize]);
      if constexpr(sizeof(Key_t) > 8){
	      if(memcmp(table[idx].key, key, sizeof(Key_t)) == 0)
		      return (char*)table[idx].value;
      }
      else
	      if(memcmp(&table[idx].key, &key, sizeof(Key_t)) == 0)
		      return (char*)table[idx].value;
      }
    }
    return (char*)NONE;
}

template <typename Key_t>
double CuckooHash<Key_t>::Utilization(void) {
  size_t n = 0;
  for (int i = 0; i < capacity; i++) {
	  if constexpr(sizeof(Key_t) > 8){
		  if(memcmp(table[i].key, INVALID<Key_t>, sizeof(Key_t)) != 0)
			  n++;
	  }
	  else{
		  if(memcmp(&table[i].key, &INVALID<Key_t>, sizeof(Key_t)) != 0)
			  n++;
	  }
  }
  return ((double)n)/((double)capacity)*100;
}

template <typename Key_t>
bool CuckooHash<Key_t>::resize(void) {
  old_cap = capacity;
  old_tab = table;

  std::unique_lock<std::shared_mutex> *lock[nlocks];
  for(int i=0;i<nlocks;i++){
    lock[i] = new std::unique_lock<std::shared_mutex>(mutex[i]);
  }
  std::shared_mutex* old_mutex = mutex;

  int prev_nlocks = nlocks;

  bool success = true;
  size_t num_grows = 0;

  do {
    success = true;
    if (table != old_tab) delete [] table;
    capacity = capacity * kResizingFactor;
    table = new Pair<Key_t>[capacity];
    if (table == nullptr) {
      cerr << "error: memory allocation failed." << endl;
      exit(1);
    }

    for (unsigned i = 0; i < old_cap; ++i) {
	    if constexpr(sizeof(Key_t) > 8){
		    if(memcmp(old_tab[i].key, INVALID<Key_t>, sizeof(Key_t)) != 0){
			    if(!insert4resize(old_tab[i].key, old_tab[i].value)){
				    success = false;
				    break;
			    }
		    }
	    }
	    else{
		    if(memcmp(&old_tab[i].key, &INVALID<Key_t>, sizeof(Key_t)) != 0){
			    if(!insert4resize(old_tab[i].key, old_tab[i].value)){
				    success = false;
				    break;
			    }
		    }
	    }
    }
    ++num_grows;
  } while (!success && num_grows < kMaxGrows);

  for (int i = 0; i < prev_nlocks; ++i) {
    delete lock[i];
  }

  if (success) {
    nlocks = capacity/locksize+1;
    mutex = new std::shared_mutex[nlocks];
    delete old_mutex;
  } else {
    exit(1);
  }
  return success;
}
