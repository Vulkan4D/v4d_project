# This script is similar to autobuild.sh but also uploads it to a remote windows machine through ssh for instant reload within the app

cd `dirname $0`
rm -rf build
mkdir build
cd build

# Same settings as in the CMakeLists.txt
ModuleVendor='V4D'
ModuleName='planetdemo'
ClassName='TerrainGenerator'
# one level higher than in the CMakeLists.txt
V4D_PROJECT_DIR="../../../../../.."
V4D_PROJECT_BUILD_DIR="$V4D_PROJECT_DIR/build"

OutputLibName="${ModuleVendor}_$ModuleName"
OutputLibFile="$OutputLibName.$ClassName.dll"
OutputLibPath="$V4D_PROJECT_BUILD_DIR/debug/modules/$OutputLibName/$OutputLibFile"

# Delete previous file
ssh WINDOWS_PC "del /q \"C:\\v4d_build\\debug\\modules\\$OutputLibName\\$OutputLibFile.new\""
# First build
x86_64-w64-mingw32-cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_SHARED_MODULE_SUFFIX_CXX=".dll.new" &&\
cmake --build . &&\
scp "$OutputLibPath.new" WINDOWS_PC:/v4d_build/ &&\
ssh WINDOWS_PC "move \"C:\\v4d_build\\$OutputLibFile.new\" \"C:\\v4d_build\\debug\\modules\\$OutputLibName\\$OutputLibFile.new\""

# Automatically rebuild when source file is modified
while inotifywait -e MODIFY\
	"../$OutputLibName.$ClassName.cpp"\
;do
	ssh WINDOWS_PC "del /q \"C:\\v4d_build\\debug\\modules\\$OutputLibName\\$OutputLibFile.new\""
	cmake --build . &&\
	scp "$OutputLibPath.new" WINDOWS_PC:/v4d_build/ &&\
	ssh WINDOWS_PC "move \"C:\\v4d_build\\$OutputLibFile.new\" \"C:\\v4d_build\\debug\\modules\\$OutputLibName\\$OutputLibFile.new\""
done;
