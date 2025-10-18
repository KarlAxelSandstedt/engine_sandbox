cmake -S . -B build -Dkas_debug=ON  -DCMAKE_BUILD_TYPE=Debug -G Ninja
cd build
cmake --build .
raddbg engine_sandbox
cd ..

