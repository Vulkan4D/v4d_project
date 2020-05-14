cd "`dirname $0`/../.."
PROJECT_DIR=`pwd`
set -e

# Cleanup symlink libs
find build/ -type l -name "*.so" -exec rm '{}' \;
if [[ -e "build/$1/libglfw.so.3" ]] ; then
	rm build/$1/libglfw.so.3 && mv build/$1/libglfw.so.3.3 build/$1/libglfw.so.3
fi

scp -rq build/$1/* WINDOWS_PC:/v4d_build/$1/

echo "
$1 crosscompile files copied to remote $(basename `dirname $0`) machine
"
