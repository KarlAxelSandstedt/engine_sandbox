#/bin/bash
cmake -S . -B build -Dthread_sanitizer=ON -Dkas_debug=ON
cd build
cmake --build . --parallel
./engine_sandbox
cd ..
