#/bin/bash
cmake -S . -B build -G Ninja
cd build
cmake --build .
./engine_sandbox &
sleep 1s
ProcID=$(pidof engine_sandbox)
perf top -p ${ProcID} -e mmap:vma_store
cd ..
