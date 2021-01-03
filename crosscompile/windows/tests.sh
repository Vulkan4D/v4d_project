ping -c 1 WINDOWS_PC > /dev/null
if [[ $? == 0 ]] ; then

	set -e

	echo "Running unit tests DEBUG for Windows..."
	ssh WINDOWS_PC "cd /v4d_build/debug/ && tests.exe"

	echo "Running unit tests RELEASE for Windows..."
	ssh WINDOWS_PC "cd /v4d_build/release/ && tests.exe"

fi
