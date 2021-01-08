cd `dirname $0`/build
set -e

cmake --build . --parallel 8
# x86_64-w64-mingw32-cmake --build .
# x86_64-w64-mingw32-make
