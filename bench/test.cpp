#ifdef LIN
#include "index/linear_probing.h"
#elif defined EXT
#include "index/extendible_hash.h"
#else
#include "index/cuckoo_hash.h"
#endif
using Key_t = char[64];
using Value_t = char[64];

int main(){
    const size_t initialTableSize = 1024 * 16;
#ifdef LIN
    Hash<Key_t, Value_t>* hashtable = new LinearProbingHash<Key_t, Value_t>(initialTableSize);
#elif defined EXT
    Hash<Key_t, Value_t>* hashtable = new ExtendibleHash<Key_t, Value_t>(initialTableSize/Block<Key_t,Value_t>::kNumSlot);
#else
    Hash<Key_t, Value_t>* hashtable = new CuckooHash<Key_t, Value_t>(initialTableSize);
#endif
    return 0;
}
