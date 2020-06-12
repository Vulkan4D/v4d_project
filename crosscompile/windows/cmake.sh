cd `dirname $0`
mkdir -p build
cd build
set -e

x86_64-w64-mingw32-cmake -DCMAKE_BUILD_TYPE=$1 -DCOMPILE_ASSETS=OFF ../../..

