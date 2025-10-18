#/bin/bash
cmake -S . -B build -Dapply_optimization_options=ON -G Ninja
cd build
cmake --build . 
./engine_sandbox
cd ..
