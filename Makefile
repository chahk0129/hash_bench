.PHONY: all clean

CXX := g++
CXXFLAGS := -std=c++17 -g
LDLIBS := -lpthread -I. -libstd=libc++

all: cuckoo linear extendible

cuckoo: index/cuckoo_hash.h bench/test.cpp
	$(CXX) $(CXXFLAGS) -o bin/cuc bench/test.cpp $(LDLIBS)

linear: index/linear_probing.h bench/test.cpp
	$(CXX) $(CXXFLAGS) -o bin/lin bench/test.cpp $(LDLIBS) -DLIN

extendible: index/extendible_hash.h bench/test.cpp
	$(CXX) $(CXXFLAGS) -o bin/ext bench/test.cpp $(LDLIBS) -DEXT
clean:
	rm bench/*.o bin/*
