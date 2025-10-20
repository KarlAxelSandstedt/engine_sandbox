#/bin/bash
cmake -S . -B build -Dkas_debug=ON  -DCMAKE_BUILD_TYPE=Debug
cd build
cmake --build . --parallel
gdb ./engine_sandbox
cd ..
