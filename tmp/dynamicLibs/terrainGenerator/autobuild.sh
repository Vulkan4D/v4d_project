cd `dirname $0`
rm -rf build
mkdir -p build ../../build/debug/dynamicLibraries
cd build
cmake .. &&\
cmake --build . &&\
cp -f terrainGenerator.so ../../../build/debug/dynamicLibraries/

while inotifywait -e MODIFY\
	../terrainGenerator.cpp\
;do
	cmake --build . &&\
	cp -f terrainGenerator.so ../../../build/debug/dynamicLibraries/terrainGenerator.so.new
done;
