#/bin/bash
cmake -S . -B build -Daddress_sanitizer=ON -Dkas_debug=ON -G Ninja
cd build
cmake --build .
./engine_sandbox
cd ..
