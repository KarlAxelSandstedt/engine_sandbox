#/bin/bash
cmake -S . -B build -Dapply_optimization_options=ON
cd build
cmake --build . --parallel
./engine_sandbox
cd ..
