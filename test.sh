#/bin/bash

if !(command -v cmake > /dev/null 2>&1); then
	echo "Error: CMake is not installed."
fi

if (command -v ninja > /dev/null 2>&1); then
	CMAKE_GENERATOR="Ninja"
else
	CMAKE_GENERATOR="Unix Makefiles"
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Dkas_test_correctness=ON -Dthread_sanitizer=ON -Dkas_debug=ON -Dapply_optimization_options=ON -G $CMAKE_GENERATOR
#cmake -S . -B build -Dkas_test_performance=ON -Dapply_optimization_options=ON -GNinja 
cd build
cmake --build . --parallel
#ctest -V CTestTestfile.cmake
#./game_1
gdb ./engine_sandbox
cd ..
