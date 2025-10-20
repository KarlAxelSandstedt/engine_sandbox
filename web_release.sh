#/bin/bash
emcmake cmake -S . -B build -Dapply_optimization_options=ON
cd build
cmake --build . --parallel
emrun engine_sandbox.html
cd ..
