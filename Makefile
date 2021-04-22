.PHONY: all clean

CXX := g++
CXXFLAGS := -std=c++17 -O3 #-g
LDLIBS := -lpthread -I. 

all: cuckoo linear extendible

hash_bench: test/integer.cpp test/string.cpp pcm/pcm-memory.cpp pcm/pcm-numa.cpp pcm/libPCM.a
	$(CXX) $(CXXFLAGS) -o bin/int_microbench test/integer.cpp $(LDLIBS) pcm/libPCM.a -DMICROBENCH
	$(CXX) $(CXXFLAGS) -o bin/int_breakdown test/integer.cpp $(LDLIBS) pcm/libPCM.a -DBREAKDOWN -DMICROBENCH
	$(CXX) $(CXXFLAGS) -o bin/int_ycsbbench test/integer.cpp $(LDLIBS) pcm/libPCM.a
	$(CXX) $(CXXFLAGS) -o bin/int_ycsblatency test/integer.cpp $(LDLIBS) pcm/libPCM.a -DLATENCY
	$(CXX) $(CXXFLAGS) -o bin/str_microbench test/string.cpp $(LDLIBS) pcm/libPCM.a -DMICROBENCH
	$(CXX) $(CXXFLAGS) -o bin/str_breakdown test/string.cpp $(LDLIBS) pcm/libPCM.a -DBREAKDOWN -DMICROBENCH
	$(CXX) $(CXXFLAGS) -o bin/str_ycsbbench test/string.cpp $(LDLIBS) pcm/libPCM.a
	$(CXX) $(CXXFLAGS) -o bin/str_ycsblatency test/string.cpp $(LDLIBS) pcm/libPCM.a -DLATENCY

cuckoo: index/cuckoo_hash.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/cuc test/hashtable_test.cpp $(LDLIBS)

linear: index/linear_probing.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/lin test/hashtable_test.cpp $(LDLIBS) -DLIN

extendible: index/extendible_hash.h test/hashtable_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/ext test/hashtable_test.cpp $(LDLIBS) -DEXT

key_gen: bench/benchmark.h bench/selfsimilar_distribution.h bench/zipfian_distribution.h bench/key_generator.h test/key_test.cpp
	$(CXX) $(CXXFLAGS) -o bin/key test/key_test.cpp $(LDLIBS)
clean:
	rm bin/*
