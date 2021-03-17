.PHONY: all clean

CXX := g++
CXXFLAGS := -std=c++17 -g
LDLIBS := -lpthread -I. 

all: cuckoo linear extendible

cceh: index/CCEH.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/cceh test/hashtable_test.cpp $(LDLIBS) -Dcceh
cuckoo: index/cuckoo_hash.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/cuc test/hashtable_test.cpp $(LDLIBS)

linear: index/linear_probing.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/lin test/hashtable_test.cpp $(LDLIBS) -DLIN

extendible: index/extendible_hash.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/ext test/hashtable_test.cpp $(LDLIBS) -DEXT

key_gen: bench/key_generator.h test/key_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/key test/key_test.cpp $(LDLIBS)
clean:
	rm bin/*
