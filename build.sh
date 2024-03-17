#!/usr/bin/env bash
cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null

SANITIZER_FLAGS="-fno-omit-frame-pointer -fsanitize=address -fsanitize=undefined"
GENERATOR="-G=Ninja"

cmake --build build --target clean

#todo fix
if [ "$1" == "--with-sanitizers" ]; then
	CMAKE_CXX_FLAGS="-DCMAKE_CXX_FLAGS='$SANITIZER_FLAGS'"
        CMAKE_EXE_LINKER_FLAGS="-DCMAKE_EXE_LINKER_FLAGS='$SANITIZER_FLAGS -stdlib=libc++ -lc++abi'"

	CMAKE_CXX_COMPILER="-DCMAKE_CXX_COMPILER=clang++"
	CMAKE_C_COMPILER="-DCMAKE_C_COMPILER=clang"
fi

cmake -B build -S . $GENERATOR $CMAKE_CXX_FLAGS $CMAKE_EXE_LINKER_FLAGS $CMAKE_C_COMPILER $CMAKE_CXX_COMPILER
cmake --build build --config Release --parallel
