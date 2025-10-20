#/bin/bash
#emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build -Dkas_debug=ON -G Ninja
emcmake cmake -S . -B build -Dkas_debug=ON
cd build
cmake --build . --parallel
emrun engine_sandbox.html
#node engine_sandbox.js
cd ..
