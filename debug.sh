#/bin/bash

if !(command -v cmake > /dev/null 2>&1); then
	echo "Error: CMake is not installed."
fi

if (command -v ninja > /dev/null 2>&1); then
	echo "NINJA FOUND"
	CMAKE_GENERATOR="Ninja"
else
	CMAKE_GENERATOR="Unix Makefiles"
fi

cmake -S . -B build -Dkas_debug=ON  -DCMAKE_BUILD_TYPE=Debug -G $CMAKE_GENERATOR
cd build
cmake --build . --parallel
gdb ./engine_sandbox
cd ..
