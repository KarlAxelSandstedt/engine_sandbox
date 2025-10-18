#/bin/bash
cmake -S . -B build -Dundefined_behaviour_sanitizer=ON -Dapply_optimization_options=ON -G Ninja
cd build
cmake --build .
./engine_sandbox
cd ..
