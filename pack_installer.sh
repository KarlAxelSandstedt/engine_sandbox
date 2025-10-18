#/bin/bash
cmake -S . -B build -Dapply_optimization_options=ON -G Ninja
#cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -Dkas_debug=ON -G Ninja
cd build
cmake --build . 
cpack --config CPackConfig.cmake
cd ..
#readelf -d build/_CPack_Packages/Linux/DEB/game_1-0.1.0-Linux/opt/game_1/bin/game_1
