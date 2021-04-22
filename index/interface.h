#ifndef HASH_INTERFACE_H_
#define HASH_INTERFACE_H_

#define CAS(_p, _u, _v)  (__atomic_compare_exchange_n (_p, _u, _v, false, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE))

#include "util/pair.h"
#include "util/timer.h"

uint64_t split_time = 0;
uint64_t cuckoo_time = 0;

struct timespec t_start, t_end;

template <typename Key_t>
class Hash {
  public:
    Hash(void) = default;
    ~Hash(void) = default;
    virtual void Insert(Key_t&, Value_t) = 0;
    virtual bool Update(Key_t&, Value_t) = 0;
    virtual bool Delete(Key_t&) = 0;
    virtual char* Get(Key_t&) = 0;
    virtual double Utilization(void) = 0;
    virtual size_t Capacity(void) = 0;
};


#endif  // _HASH_INTERFACE_H_
