#!/bin/sh
rm mxlog.txt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-spatzBenchmarks-dp-mxfmatmul-m4n4k4-b2_M16_N16_K16 >> mxlog.txt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-spatzBenchmarks-dp-mxfmatmul-m4n4k4-b4_M16_N16_K16 >> mxlog.txt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-spatzBenchmarks-dp-mxfmatmul-m4n4k8-b2_M16_N16_K16 >> mxlog.txt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-spatzBenchmarks-dp-mxfmatmul-m4n4k8-b4_M16_N16_K16 >> mxlog.txt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-spatzBenchmarks-dp-mxfmatmul-m8n4k4-b2_M16_N16_K16 >> mxlog.txt
./bin/spatz_cluster.vsim ./sw/build/spatzBenchmarks/test-spatzBenchmarks-dp-mxfmatmul-m8n4k4-b4_M16_N16_K16 >> mxlog.txt
cnt=$(grep -c "SUCCESS" mxlog.txt)
echo "Total $cnt / 6 TESTS PASS"