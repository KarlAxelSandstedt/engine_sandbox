cmake -S . -B build -D CMAKE_BUILD_TYPE=Release -D apply_optimization_options=ON -G Ninja 
cd build
cmake --build .
cpack --config CPackConfig.cmake
::cpack --config CPackSourceConfig.cmake
