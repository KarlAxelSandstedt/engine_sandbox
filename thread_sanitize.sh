#/bin/bash
cmake -S . -B build -Dthread_sanitizer=ON -Dkas_debug=ON -G Ninja
cd build
cmake --build .
./engine_sandbox
cd ..
