#/bin/bash
emcmake cmake -S . -B build -Dapply_optimization_options=ON -G Ninja
cd build
cmake --build . 
emrun engine_sandbox.html
cd ..
