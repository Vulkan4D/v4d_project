# This script will rebuild a module, adding the .new suffix to prevent replacing the currently used library, and in some cases it can be automatically reloaded in the app if implemented

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
OutputLibFile="$OutputLibName.$ClassName.so"
OutputLibPath="$V4D_PROJECT_BUILD_DIR/debug/modules/$OutputLibName/$OutputLibFile"

# Delete previous file
rm "$OutputLibPath.new"

# First build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_SHARED_MODULE_SUFFIX_CXX=".so.tmp" && cmake --build . &&\
mv "$OutputLibPath.tmp" "$OutputLibPath.new"

# Automatically rebuild when source file is modified
while inotifywait -e MODIFY\
	"../${ModuleVendor}_$ModuleName.$ClassName.cpp"\
;do
	rm "$OutputLibPath.new"
	cmake --build . &&\
	mv "$OutputLibPath.tmp" "$OutputLibPath.new"
done
