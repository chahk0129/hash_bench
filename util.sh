#!/bin/bash
for index in ext cuc lin; do
	./bin/int_microbench $index 100000 1 4 >> output/util/$index
done
