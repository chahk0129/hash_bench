#include "bench/bench.hpp"

using Key_t = uint64_t;
extern bool hyperthreading;

int main(int argc, char* argv[]){
    if(argc < 4){
	std::cout << "Usage: " << std::endl;
	std::cout << "1. workload type: a, b, c, d" << std::endl;
	std::cout << "2. index type: ext, cuc, lin" << std::endl;
	std::cout << "3. number of threads" << std::endl;
	std::cout << "   --hyper: whether to pin all threads on single NUMA node" << std::endl;
	std::cout << "   --mem: whether to monitor memory access" << std::endl;
	std::cout << "   --numa: whether to monitor NUMA throughput" << std::endl;
	return 1;
    }

    int workload_type;
    if(strcmp(argv[1], "a") == 0)
	workload_type = WORKLOAD_A;
    else if(strcmp(argv[1], "b") == 0)
	workload_type = WORKLOAD_B;
    else if(strcmp(argv[1], "c") == 0)
	workload_type = WORKLOAD_C;
    else if(strcmp(argv[1], "D") == 0)
	workload_type = WORKLOAD_D;
    else{
	fprintf(stderr, "unknown workload type %s\n", argv[1]);
	return 1;
    }

    int index_type;
    if(strcmp(argv[2], "ext") == 0)
	index_type = TYPE_EXTENDIBLE_HASH;
    else if(strcmp(argv[2], "cuc") == 0)
	index_type = TYPE_CUCKOO_HASH;
    else if(strcmp(argv[2], "lin") == 0)
	index_type = TYPE_LINEAR_HASH;
    else{
	fprintf(stderr, "unkown index type %s\n", argv[2]);
	return 1;
    }

    int num_threads = atoi(argv[3]);
    if(num_threads < 1){
	fprintf(stderr, "number of threads must be larger than 1 (not supporting %d\n", num_threads);
	return 1;
    }

    char** argv_end = argv + argc;
    for(char** v=argv+4; v!=argv_end; v++){
	if(strcmp(*v, "--hyper") == 0)
	    hyperthreading = true;
	else if(strcmp(*v, "--mem") == 0)
	    memory_bandwidth = true;
	else if(strcmp(*v, "--numa") == 0)
	    numa = true;
	else{
	    fprintf(stderr, "unkown option: %s\n", *v);
	    return 1;
	}
    }

    if(memory_bandwidth){
	if(geteuid() != 0){
	    fprintf(stderr, "Must be the root user to measure memory bandwidth\n");
	    return 1;
	}
	fprintf(stderr, "Measuring memory bandwidth\n");
	PCM_Memory::InitMemoryMonitor();
    }

    if(numa){
	if(geteuid() != 0){
	    fprintf(stderr, "Must be the root user to measure NUMA operations\n");
	    return 1;
	}
	fprintf(stderr, "Measuring NUMA operations\n");
	PCM_NUMA::InitNUMAMonitor();
    }

    int init_num = 50000000;
    int run_num = 50000000;
    benchmark_t<Key_t>* bench = new benchmark_t<Key_t>();

    kvpair_t<Key_t>* init_kv = new kvpair_t<Key_t>[init_num];
    kvpair_t<Key_t>* run_kv = new kvpair_t<Key_t>[run_num];
    int* ops = new int[run_num];




