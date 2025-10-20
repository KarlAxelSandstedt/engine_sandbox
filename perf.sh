#/bin/bash

if !(command -v cmake > /dev/null 2>&1); then
	echo "Error: CMake is not installed."
fi

if (command -v ninja > /dev/null 2>&1); then
	echo "NINJA FOUND"
	CMAKE_GENERATOR="Ninja"
else
	CMAKE_GENERATOR="Unix Makefiles"
fi

cmake -S . -B build -Dapply_optimization_options=ON -G $CMAKE_GENERATOR
cd build
cmake --build . --parallel
./engine_sandbox &
sleep 1s
ProcID=$(pidof engine_sandbox)
perf top -p ${ProcID} -e mmap:vma_store
cd ..
