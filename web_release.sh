#/bin/bash

if !(command -v cmake > /dev/null 2>&1); then
	echo "Error: CMake is not installed."
fi

if (command -v ninja > /dev/null 2>&1); then
	CMAKE_GENERATOR="Ninja"
else
	CMAKE_GENERATOR="Unix Makefiles"
fi

emcmake cmake -S . -B build -Dapply_optimization_options=ON -DCMAKE_BUILD_TYPE=Release -G $CMAKE_GENERATOR
cd build
cmake --build . --parallel
emrun engine_sandbox.html
cd ..
