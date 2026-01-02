#/bin/bash
#emcmake cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build -Dkas_debug=ON -G Ninja

if !(command -v cmake > /dev/null 2>&1); then
	echo "Error: CMake is not installed."
fi

if (command -v ninja > /dev/null 2>&1); then
	CMAKE_GENERATOR="Ninja"
else
	CMAKE_GENERATOR="Unix Makefiles"
fi

emcmake cmake -S . -B build -Dkas_debug=ON -DCMAKE_BUILD_TYPE=Debug -G $CMAKE_GENERATOR
cd build
cmake --build . --parallel

TRACY_PROFILER="../lib/tracy/profiler/build/tracy-profiler"
if [ -z $(pgrep -f "tracy-profiler")]; then
	nohup "$TRACY_PROFILER" > /dev/null 2>&1 &
fi

#emrun engine_sandbox.html
#node engine_sandbox.js
cd ..
