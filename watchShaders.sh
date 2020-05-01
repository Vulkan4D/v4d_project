cd `dirname $0`

#rebuild shaders
cd build && rm -f CMakeCache.txt ; 
find . -type d -name 'shaders' | xargs rm -rf ; 
killall inotifywait
sleep 1
cmake .. -DCMAKE_BUILD_TYPE=Debug ; 
make shaders -j8;

cd ..

for dir in `find build/debug -type d -name 'shaders'` ; do
	for f in `find "$dir" -type f -name '*.watch.sh'` ; do
		sh -c "$f" &
	done
	for f in `find "$dir" -type f -name '*.meta'` ; do
		shader=`echo "$f" | sed 's/\.meta$//'`
		sh -c "crosscompile/windows/copy_shaders.sh \"$shader\"" &
	done
done
