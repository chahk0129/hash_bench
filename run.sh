#!/bin/bash
"
# breakdown
for i in 1 2 3; do
	for index in ext lin cuc; do
		./bin/int_breakdown $index 10000000 1 1 1 >> output/int/breakdown/${index}
	done
done
"
# microbench
for i in 1 2 3; do
	for index in ext lin cuc; do
#		./bin/int_microbench $index 10000000 1 1 >> output/int/microbench/${index}
		./bin/str_microbench $index 10000000 1 1 >> output/str/microbench/${index}
		echo "\n" >> output/str/microbench/${index}
	done
done
"
# latency
for i in 1 2 3; do
	for index in ext lin cuc; do
		for t in 1 2 4 8 16 32; do
			./bin/int_microbench $index 10000000 $t 2 >> output/int/latency/${index}_${t}
		done
	done
done

# mixed
for i in 1 2 3; do
	for index in ext lin cuc; do
		for t in 1 2 4 8 16 32; do
			./bin/int_microbench $index 50000000 $t 3 >> output/int/mixed/${index}_${t}
		done
	done
done
"
# ycsb
for i in 1 2 3; do
	for index in ext cuc; do
		for wk in a b c; do
			for t in 1 2 4 8 16 32; do
				./bin/int_ycsbbench $wk $index $t --hyper >> output/int/ycsb/${wk}_${index}_${t}
				./bin/str_ycsbbench $wk $index $t --hyper >> output/str/ycsb/${wk}_${index}_${t}
			done
		done
	done
done

for i in 1 2 3; do
	for wk in a b c; do
		for t in 1 2 4 8 16 32; do
			./bin/int_ycsbbench $wk lin $t --hyper >> output/int/ycsb/${wk}_lin_${t}
			./bin/str_ycsbbench $wk lin $t --hyper >> output/str/ycsb/${wk}_lin_${t}
		done
	done
done
