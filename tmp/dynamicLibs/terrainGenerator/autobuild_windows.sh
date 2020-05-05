cd `dirname $0`
rm -rf build
mkdir -p build
cd build
x86_64-w64-mingw32-cmake .. &&\
cmake --build . &&\
cp -f terrainGenerator.dll ../../../build/debug/dynamicLibraries/ &&\
scp terrainGenerator.dll WINDOWS_PC:/v4d_build/debug/dynamicLibraries/terrainGenerator.dll.tmp &&\
ssh WINDOWS_PC "ren \"C:\\v4d_build\\debug\\dynamicLibraries\\terrainGenerator.dll.tmp\" \"terrainGenerator.dll.new\""

while inotifywait -e MODIFY\
	../terrainGenerator.cpp\
;do
	ssh WINDOWS_PC "del /q \"C:\\v4d_build\\debug\\dynamicLibraries\\terrainGenerator.dll.new\""
	cmake --build . &&\
	cp -f terrainGenerator.dll ../../../build/debug/dynamicLibraries/ &&\
	scp terrainGenerator.dll WINDOWS_PC:/v4d_build/debug/dynamicLibraries/terrainGenerator.dll.tmp &&\
	ssh WINDOWS_PC "ren \"C:\\v4d_build\\debug\\dynamicLibraries\\terrainGenerator.dll.tmp\" \"terrainGenerator.dll.new\""
done;
