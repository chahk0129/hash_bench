#include "bench/benchmark.h"
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
inline void benchmark_t<Key_t>::ycsb_exec(int workload_type, int index_type, Pair<Key_t>* init_kv, int init_num, Pair<Key_t>* run_kv, int run_num, int num_threads, int* ops){
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

    double start_time = get_now();
    start_threads(hashtable, num_threads, load_func, false);
    double end_time = get_now();

    double throughput = init_num / (end_time - start_time) / 1000000; // MOps/sec
    std::cout << "\033[1;32m";
    std::cout << "Insert " << throughput << "\033[0m" << std::endl;

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

    start_time = get_now();
    start_threads(hashtable, num_threads, exec_func, false);
    end_time = get_now();

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


