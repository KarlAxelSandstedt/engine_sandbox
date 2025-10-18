cmake -S . -B build -D apply_optimization_options=ON -D CMAKE_BUILD_TYPE=Release -G Ninja
cd build
cmake --build .
engine_sandbox.exe
cd ..
