#/bin/bash
cmake -S . -B build -Dundefined_behaviour_sanitizer=ON -Dapply_optimization_options=ON
cd build
cmake --build . --parallel
./engine_sandbox
cd ..
