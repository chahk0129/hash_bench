.PHONY: all clean

CXX := clang++ #g++
CXXFLAGS := -std=c++17 -g
LDLIBS := -lpthread -I. -libstd=libc++

all: cuckoo

cuckoo: index/cuckoo_hash.h bench/test.cpp
	$(CXX) $(CXXFLAGS) -o bin/cuckoo bench/test.cpp $(LDLIBS)

clean:
	rm bench/*.o bin/*
