# Hash-bench

Hash-bench is a benchmark for concurrent in-memory hash tables.
The goal is to build a comprehensive benchmark interface that can be generally applied to not only hash tables but also other indexing structures.
It includes extensive evaluation cases such as throughputs, tail latency and memory utilization with different workloads.
Utility functions are also included to help understand the behavior of the indexes.

## Building
To run micro-benchmarks,
```bash
$ make
$ ./bin/$(key_type)_microbench $(index_type) $(num_data) $(num_threads) $(run_mode)
```
To run YCSB-benchmarks,
```bash
$ make
$ ./bin/$(key_type)_ycsbbench $(workload_type) $(index_type) $(num_threads)
```

## Contributor
* Hokeun Cha (hcha@cs.wisc.edu)
Hash-bench has been developed for CS764-Spring2021 project submission. 
