#include "bench/benchmark.h"
//#include "pcm/pcm-memory.cpp"
//#include "pcm/pcm-numa.cpp"

#include <fstream>

template <typename Key_t>
size_t benchmark_t<Key_t>::mem_usage(void){
    FILE* fp = fopen("/proc/self/statm", "r");
    if(!fp){
	fprintf(stderr, "Cannot open /proc/self/statm to read memory usage\n");
	exit(1);
    }
    unsigned long unused, rss;
    if(fscanf(fp, "%ld %ld %ld %ld %ld %ld %ld", &unused, &rss, &unused, &unused, &unused, &unused, &unused) != 7){
	perror("");
	exit(1);
    }

    (void)unused;
    fclose(fp);

    return rss * (4096 / 1024); // in KiB
}


template <typename Key_t>
inline void benchmark_t<Key_t>::microbench(int index_type, Pair<Key_t>* init_kv, int init_num, bool insert_only){
    Hash<Key_t>* hashtable = getInstance<Key_t>(index_type);
    
    for(int i=0; i<init_num; i++){
	init_kv[i].key = i+1;
	init_kv[i].value = i+1;
    }
    std::random_shuffle(init_kv, init_kv+init_num);
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<init_num; i++){
	hashtable->Insert(init_kv[i].key, init_kv[i].value);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    uint64_t elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    uint64_t throughput = (uint64_t)init_num / (elapsed/1000.0) * 1000000;
    std::cout << "\033[1;32m";
    std::cout << "Insert Throughput(Ops/sec): " << throughput << std::endl;
#ifdef BREAKDOWN
    std::cout << "Elapsed time(usec): " << elapsed/1000.0 << "\033[0m" << std::endl;;
    std::cout << "split_time(usec): " << split_time/1000.0 << std::endl;
    std::cout << "cuckoo_time(usec): " << cuckoo_time/1000.0 << std::endl;
    std::cout << "cuckoo_time(usec): " << cuckoo_time << std::endl;
    std::cout << "traversal_time(usec): " << (elapsed - split_time - cuckoo_time)/1000.0 << std::endl;
#endif
    if(insert_only)
	return;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<init_num; i++){
	auto ret = hashtable->Get(init_kv[i].key);
	assert((uint64_t)ret == init_kv[i].value);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    throughput = (uint64_t)init_num / (elapsed/1000.0) * 1000000;
    std::cout << "\033[1;32m";
    std::cout << "Search Throughput(Ops/sec): " << throughput << "\033[0m" << std::endl;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<init_num/30; i++){
	auto ret = hashtable->Update(init_kv[i].key, init_kv[i].value);
	assert(ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    throughput = (uint64_t)init_num/30 / (elapsed/1000.0) * 1000000;
    std::cout << "\033[1;32m";
    std::cout << "Update Throughput(Ops/sec): " << throughput << "\033[0m" << std::endl;

    clock_gettime(CLOCK_MONOTONIC, &start);
    for(int i=0; i<init_num/20; i++){
	auto ret = hashtable->Delete(init_kv[i].key);
	assert(ret);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    elapsed = end.tv_nsec - start.tv_nsec + (end.tv_sec - start.tv_sec)*1000000000;
    throughput = (uint64_t)(init_num/20) / (elapsed/1000.0) * 1000000;
    std::cout << "\033[1;32m";
    std::cout << "Delete Throughput(Ops/sec): " << throughput << "\033[0m" << std::endl;


}

    
template <typename Key_t>
inline void benchmark_t<Key_t>::ycsb_load(int workload_type, int index_type, Pair<Key_t>* init_kv, int init_num, Pair<Key_t>* run_kv, int run_num, int* ops){
    std::string init_file;
    std::string txn_file;

    if(workload_type == WORKLOAD_A){
	init_file = "/hk/workloads/loada_zipfian_int_100M.dat";
	txn_file = "/hk/workloads/txnsa_zipfian_int_100M.dat";
    }
    else if(workload_type == WORKLOAD_B){
	init_file = "/hk/workloads/loadb_zipfian_int_100M.dat";
	txn_file = "/hk/workloads/txnsb_zipfian_int_100M.dat";
    }
    else if(workload_type == WORKLOAD_C){
	init_file = "/hk/workloads/loadc_zipfian_int_100M.dat";
	txn_file = "/hk/workloads/txnsc_zipfian_int_100M.dat";
    }
    else if(workload_type == WORKLOAD_D){
	init_file = "/hk/workloads/loadd_zipfian_int_100M.dat";
	txn_file = "/hk/workloads/txnsd_zipfian_int_100M.dat";
    }
    else{
	fprintf(stderr, "unkonw workload type %d\n", workload_type);
	exit(1);
    }

    uint64_t value = 0;
    void* base_ptr = malloc(8);
    uint64_t base = (uint64_t)base_ptr;
    free(base_ptr);

    std::ifstream infile_load;
    infile_load.open(init_file.c_str());
    if(!infile_load.is_open()){
	fprintf(stderr, "init file failed to open\n");
	exit(1);
    }

    std::string op;
    Key_t key;

    std::string insert("INSERT");
    std::string read("READ");
    std::string update("UPDATE");

    for(int i=0; i<init_num; i++){
	infile_load >> op >> key;
	if(op.compare(insert) != 0){
	    fprintf(stderr, "Reading load file failed\n");
	    exit(1);
	}
	init_kv[i].key = key;
	init_kv[i].value = key;
    }

    infile_load.close();

    Pair<Key_t>* init_kv_data = &init_kv[0];


    std::ifstream infile_txn;
    infile_txn.open(txn_file.c_str());
    if(!infile_txn.is_open()){
	fprintf(stderr, "txn file failed to open\n");
	exit(1);
    }
    
    for(int i=0; i<run_num; i++){
	infile_txn >> op >> key;
	if(op.compare(insert) == 0){
	    ops[i] = OP_INSERT;
	    run_kv[i].key = key;
	    run_kv[i].value = key;
	}
	else if(op.compare(read) == 0){
	    ops[i] = OP_READ;
	    run_kv[i].key = key;
	}
	else if(op.compare(update) == 0){
	    ops[i] = OP_UPDATE;
	    run_kv[i].key = key;
	    run_kv[i].value = key;
	}
	else{
	    fprintf(stderr, "unknown operation type\n");
	    exit(1);
	}
    }

    infile_txn.close();
}

template <typename Key_t>
inline void benchmark_t<Key_t>::ycsb_exec(int workload_type, int index_type, Pair<Key_t>* init_kv, int init_num, Pair<Key_t>* run_kv, int run_num, int num_threads, int* ops, bool pcm_enabled){
    /*
    if(memory_bandwidth){
	if(geteuid() != 0){
	    fprintf(stderr, "Must be root user to measure memory bandwidth\n");
	    exit(0);
	}
	PCM_memory::InitMemoryMonitor();
    }

    if(numa){
	if(geteuid() != 0){
	    fprintf(stderr, "Must be root user to measure NUMA operations\n");
	    exit(0);
	}
	PCM_NUMA::InitNumaMonitor();
    }*/

    Hash<Key_t>* hashtable = getInstance<Key_t>(index_type);

    std::vector<uint64_t> load_latency[num_threads];
    std::vector<uint64_t> exec_latency[num_threads];

    auto load_func = [&hashtable, &init_kv, init_num, num_threads, &load_latency](uint64_t thread_id, bool){
	size_t total_num = init_num;
	size_t chunk_size = total_num / num_threads;
	size_t from = chunk_size * thread_id;
	size_t to = chunk_size * (thread_id+1);
#ifdef LATENCY
	struct timespec start, end;
	uint64_t elapsed;
#endif

	for(size_t i=from; i<to; i++){
#ifdef LATENCY
	    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
	    hashtable->Insert(init_kv[i].key, init_kv[i].value);
#ifdef LATENCY
	    clock_gettime(CLOCK_MONOTONIC, &end);
	    elapsed = end.tv_nsec - start.tv_nsec + 1000000000*(end.tv_sec - start.tv_sec);
	    load_latency[thread_id].push_back(elapsed);
#endif

	}
	return;
    };

    /*
    if(memory_bandwidth)
	PCM_memory::StartMemoryMonitor();

    if(numa)
	PCM_NUMA::StartNUMAMonitor();
	*/
    std::unique_ptr<SystemCounterState> before;
    if(pcm_enabled){
	before = std::make_unique<SystemCounterState>();
	*before = getSystemCounterState();
    }
    double start_time = get_now();
    start_threads(hashtable, num_threads, load_func, false);
    double end_time = get_now();

    std::unique_ptr<SystemCounterState> after;
    if(pcm_enabled){
	after = std::make_unique<SystemCounterState>();
	*after = getSystemCounterState();
    }
    /*
    if(memory_bandwidth)
	PCM_memory::EndMemoryMonitor();

    if(numa)
	PCM_NUMA::EndNUMAMonitor();
	*/
    double throughput = init_num / (end_time - start_time) / 1000000; // MOps/sec
    std::cout << "\033[1;32m";
    std::cout << "Insert " << throughput << "\033[0m" << std::endl;

    if(pcm_enabled){
	std::cout << "PCM Metrics:\n"
	    	  << "\tL3 misses: " << getL3CacheMisses(*before, *after) << "\n"
		  << "\tReads(bytes): " << getBytesReadFromMC(*before, *after) << "\n"
		  << "\tWrites(byes): " << getBytesWrittenToMC(*before, *after) << std::endl;
    }
#ifdef LATENCY
    std::vector<uint64_t> aggregated_load_latency;
    for(int i=0; i<num_threads; i++){
	if(!load_latency[i].empty()){
	    for(auto it=load_latency[i].begin(); it!=load_latency[i].end(); it++)
		aggregated_load_latency.push_back(*it);
	}
    }
    std::sort(aggregated_load_latency.begin(), aggregated_load_latency.end());
    std::cout << "Tail latency for load: " << aggregated_load_latency.back()/1000 << " usec" << std::endl;
#endif

    auto exec_func = [&hashtable, &run_kv, run_num, &ops, num_threads, &exec_latency](uint64_t thread_id, bool){
	size_t total_num = run_num;
	size_t chunk_size = total_num / num_threads;
	size_t from = chunk_size * thread_id;
	size_t to = chunk_size * (thread_id+1);
#ifdef LATENCY
	struct timespec start, end;
	uint64_t elapsed;
#endif

	for(size_t i=from; i<to; i++){
#ifdef LATENCY
	    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
	    if(ops[i] == OP_INSERT){
		hashtable->Insert(run_kv[i].key, run_kv[i].value);
	    }
	    else if(ops[i] == OP_READ){
		auto ret = hashtable->Get(run_kv[i].key);
		assert((Key_t)ret == run_kv[i].key);
	    }
	    else if(ops[i] == OP_UPDATE){
		auto ret = hashtable->Update(run_kv[i].key, run_kv[i].value);
		assert(ret);
	    }
#ifdef LATENCY
	    clock_gettime(CLOCK_MONOTONIC, &end);
	    elapsed = end.tv_nsec - start.tv_nsec + 1000000000*(end.tv_sec - start.tv_sec);
	    exec_latency[thread_id].push_back(elapsed);
#endif
	}
	return;
    };

    /*
    if(memory_bandwidth)
	PCM_memory::StartMemoryMonitor();

    if(numa)
	PCM_NUMA::StartNUMAMonitor();
    */
    if(pcm_enabled){
	*before = getSystemCounterState();
    }

    start_time = get_now();
    start_threads(hashtable, num_threads, exec_func, false);
    end_time = get_now();

    if(pcm_enabled){
	*after = getSystemCounterState();
    }

    /*
    if(memory_bandwidth)
	PCM_memory::EndMemoryMonitor();

    if(numa)
	PCM_NUMA::EndNUMAMonitor();
    */
    throughput = run_num / (end_time - start_time) / 1000000; // MOps/sec
    std::cout << "\033[1;31m";
    if(workload_type == WORKLOAD_A)
	std::cout << "Read/Update " << throughput << "\033[0m" << std::endl;
    else if(workload_type == WORKLOAD_B)
	std::cout << "Read/Update " << throughput << "\033[0m" << std::endl;
    else if(workload_type == WORKLOAD_C)
	std::cout << "Read " << throughput << "\033[0m" << std::endl;
    else if(workload_type == WORKLOAD_D)
	std::cout << "Read/Update " << throughput << "\033[0m" << std::endl;

    if(pcm_enabled){
	std::cout << "PCM Metrics:\n"
	    	  << "\tL3 misses: " << getL3CacheMisses(*before, *after) << "\n"
		  << "\tReads(bytes): " << getBytesReadFromMC(*before, *after) << "\n"
		  << "\tWrites(byes): " << getBytesWrittenToMC(*before, *after) << std::endl;
    }
#ifdef LATENCY
    std::vector<uint64_t> aggregated_exec_latency;
    for(int i=0; i<num_threads; i++){
	if(!exec_latency[i].empty()){
	    for(auto it=exec_latency[i].begin(); it!=exec_latency[i].end(); it++)
		aggregated_exec_latency.push_back(*it);
	}
    }
    std::sort(aggregated_exec_latency.begin(), aggregated_exec_latency.end());
    std::cout << "Tail latency for exec: " << aggregated_exec_latency.back()/1000 << " usec" << std::endl;
#endif

}


