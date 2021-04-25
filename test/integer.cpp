#include "bench/benchmark.hpp"
#include <unistd.h>
#include <sys/types.h>

//#include "pcm/pcm-memory.cpp"
//#include "pcm/pcm-numa.cpp"

using Key_t = uint64_t;
extern bool hyperthreading;

static bool pcm_enabled = false;
static bool memory_bandwidth = false;
static bool numa = false;
static bool insert_only = false;

int main(int argc, char* argv[]){
#ifdef MICROBENCH 
    if(argc < 2){
	std::cout << "Usage: " << std::endl;
	std::cout << "1. index type: ext, cuc, lin" << std::endl;
	std::cout << "2. numData" << std::endl;
	std::cout << "3. numThreads" << std::endl;
	std::cout << "4. runmode: 1(microbench), 2(latency) 3(mixed) 4(util)" << std::endl;
	std::cout << "5. insert-only" << std::endl;
	return 1;
    }

    int index_type;
    if(strcmp(argv[1], "ext") == 0)
	index_type = TYPE_EXTENDIBLE_HASH;
    else if(strcmp(argv[1], "cuc") == 0)
	index_type = TYPE_CUCKOO_HASH;
    else if(strcmp(argv[1], "lin") == 0)
	index_type = TYPE_LINEAR_HASH;
    else{
	fprintf(stderr, "unkown index type %s\n", argv[2]);
	return 1;
    }

    int init_num = atoi(argv[2]);
    int num_threads = atoi(argv[3]);
    int mode = atoi(argv[4]);
    if(argc > 5){
    	int insert_only_ = atoi(argv[5]);
    	if(insert_only_ > 0) insert_only = true;
    }
    benchmark_t<Key_t>* bench = new benchmark_t<Key_t>(pcm_enabled);

    Pair<Key_t>* init_kv = new Pair<Key_t>[init_num];
    if(mode == 1)
	bench->microbench(index_type, init_kv, init_num, insert_only);
    else if(mode == 2)
	bench->latency(index_type, init_kv, init_num, num_threads);
    else if(mode == 3)
	bench->mixed(index_type, init_kv, init_num, num_threads);
    else
	bench->utilization(index_type, init_kv, init_num);
    return 0;

#else

    if(argc < 4){
	std::cout << "Usage: " << std::endl;
	std::cout << "1. workload type: a, b, c, d" << std::endl;
	std::cout << "2. index type: ext, cuc, lin" << std::endl;
	std::cout << "3. number of threads" << std::endl;
	std::cout << "   --hyper: whether to pin all threads on single NUMA node" << std::endl;
	std::cout << "   --pcm: whether to use pcm to monitor memory patterns" << std::endl;
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
    else if(strcmp(argv[1], "d") == 0)
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
	else if(strcmp(*v, "--pcm") == 0)
	    pcm_enabled = true;
	else if(strcmp(*v, "--mem") == 0)
	    memory_bandwidth = true;
	else if(strcmp(*v, "--numa") == 0)
	    numa = true;
	else{
	    fprintf(stderr, "unkown option: %s\n", *v);
	    return 1;
	}
    }

    /*
    if(memory_bandwidth){
	if(geteuid() != 0){
	    fprintf(stderr, "Must be the root user to measure memory bandwidth\n");
	    return 1;
	}
	fprintf(stderr, "Measuring memory bandwidth\n");
	PCM_memory::InitMemoryMonitor();
    }

    if(numa){
	if(geteuid() != 0){
	    fprintf(stderr, "Must be the root user to measure NUMA operations\n");
	    return 1;
	}
	fprintf(stderr, "Measuring NUMA operations\n");
	PCM_NUMA::InitNUMAMonitor();
    }*/

    int init_num = 50000000;
    int run_num = 50000000;
    benchmark_t<Key_t>* bench = new benchmark_t<Key_t>(pcm_enabled);

    Pair<Key_t>* init_kv = new Pair<Key_t>[init_num];
    Pair<Key_t>* run_kv = new Pair<Key_t>[run_num];
    int* ops = new int[run_num];

    bench->ycsb_load(workload_type, index_type, init_kv, init_num, run_kv, run_num, ops);
    bench->ycsb_exec(workload_type, index_type, init_kv, init_num, run_kv, run_num, num_threads, ops, pcm_enabled);
    //bench->ycsb_exec(workload_type, index_type, init_kv, init_num, run_kv, run_num, num_threads, ops, memory_bandwidth, numa);
    return 0;
#endif
}



