cd "`dirname $0`/../.."
PROJECT_DIR=`pwd`
set -e

scp -rq build/debug/* WINDOWS_PC:/v4d_build/debug/
scp -rq build/release/* WINDOWS_PC:/v4d_build/release/

echo "
$(basename `dirname $0`) crosscompile files copied to remote machine
"
