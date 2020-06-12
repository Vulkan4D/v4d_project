cd "`dirname $0`/../.."
PROJECT_DIR=`pwd`
cd "`dirname $0`"
set -e

# Kill potentially running process on remote windows pc
ssh WINDOWS_PC "START /wait taskkill /f /im tests.exe"
ssh WINDOWS_PC "START /wait taskkill /f /im demo.exe"

# Delete build on remote windows pc
ssh WINDOWS_PC "for /d %i in (\v4d_build\debug\*) do @rmdir /s /q \"%i\" > NUL"
ssh WINDOWS_PC "del /q /s \v4d_build\debug\* > NUL"
ssh WINDOWS_PC "for /d %i in (\v4d_build\release\*) do @rmdir /s /q \"%i\" > NUL"
ssh WINDOWS_PC "del /q /s \v4d_build\release\* > NUL"

# Cleanup local build
rm -rf build
mkdir build

# Copy global DLLs to Remote Windows PC
scp -rq dll/*.dll WINDOWS_PC:/v4d_build/debug/
scp -rq dll/*.dll WINDOWS_PC:/v4d_build/release/

