ping -c 1 WINDOWS_PC > /dev/null
if [[ $? == 0 ]] ; then

	cd `dirname $0`/../../build

	ssh WINDOWS_PC "START /wait taskkill /f /im demo.exe" &&\
	ssh WINDOWS_PC "START /wait taskkill /f /im tests.exe" &&\
	ssh WINDOWS_PC "del /q /s C:\\v4d_build\\$1\\*.exe"

	echo "Copying files to remote windows machine..."

	cd $1/modules
	for d in *; do
		ssh WINDOWS_PC "if not exist C:\\v4d_build\\$1\\modules\\$d mkdir C:\\v4d_build\\$1\\modules\\$d"
	done
	cd ..

	find -type f -name "*.exe" -exec scp -rq {''} WINDOWS_PC:/v4d_build/$1/{''} \;
	find -type f -name "*.dll" -exec scp -rq {''} WINDOWS_PC:/v4d_build/$1/{''} \;
	find -type d -name "assets" -exec scp -rq {''} WINDOWS_PC:/v4d_build/$1/{''} \;

	echo "Ready!"

fi
