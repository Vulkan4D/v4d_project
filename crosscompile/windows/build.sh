cd `dirname $0`/build
set -e

cmake --build . --parallel 8
