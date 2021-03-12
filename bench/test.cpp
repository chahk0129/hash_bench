#include "index/cuckoo_hash.h"

using Key_t = char[64];
using Value_t = char[64];

int main(){
    const size_t initialTableSize = 1024 * 16;
    Hash<Key_t, Value_t>* hashtable = new CuckooHash<Key_t, Value_t>(initialTableSize);
    return 0;
}
