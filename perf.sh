#/bin/bash
cmake -S . -B build -Dapply_optimization_options=ON 
cd build
cmake --build . --parallel
./engine_sandbox &
sleep 1s
ProcID=$(pidof engine_sandbox)
perf top -p ${ProcID} -e mmap:vma_store
cd ..
