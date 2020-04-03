cd "`dirname $0`/../.."
PROJECT_DIR=`pwd`
set -e

scp -rq build/$1/* WINDOWS_PC:/v4d_build/$1/

echo "
$(basename `dirname $0`) $1 crosscompile files copied to remote machine
"
