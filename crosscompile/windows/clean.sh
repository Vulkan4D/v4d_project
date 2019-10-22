cd "`dirname $0`/../.."
PROJECT_DIR=`pwd`

# Kill potentially running process on remote windows pc
ssh WINDOWS_PC "START /wait taskkill /f /im tests.exe"
ssh WINDOWS_PC "START /wait taskkill /f /im demo.exe"

# Delete build on remote windows pc
ssh WINDOWS_PC "rmdir /q /s \v4d_build\debug > NUL"
ssh WINDOWS_PC "rmdir /q /s \v4d_build\release > NUL"
ssh WINDOWS_PC "mkdir \v4d_build\debug"
ssh WINDOWS_PC "mkdir \v4d_build\release"

# Copy global DLLs to Remote Windows PC
scp -rq dll/* WINDOWS_PC:/v4d_build/debug/
scp -rq dll/* WINDOWS_PC:/v4d_build/release/

