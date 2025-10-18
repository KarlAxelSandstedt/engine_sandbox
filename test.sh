#/bin/bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Dkas_test_correctness=ON -Dthread_sanitizer=ON -Dkas_debug=ON -Dapply_optimization_options=ON -GNinja 
#cmake -S . -B build -Dkas_test_performance=ON -Dapply_optimization_options=ON -GNinja 
cd build
cmake --build .
#ctest -V CTestTestfile.cmake
#./game_1
gdb ./engine_sandbox
cd ..
