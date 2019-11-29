cd `dirname $0`/../../build
set -e

rm -f CMakeCache.txt &&\
x86_64-w64-mingw32-cmake -DCMAKE_BUILD_TYPE=$1 -DCOMPILE_ASSETS=OFF .. &&\
cmake --build . --parallel 8
