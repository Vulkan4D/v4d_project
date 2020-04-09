cd "`dirname $0`/../.."

while inotifywait -e close_write '/home/olivier/projects/v4d_project/build/debug/modules/test_planets_rtx/assets/shaders/planets.meta'; do 
	scp -rq build/debug/modules/test_planets_rtx/assets/shaders/*.spv WINDOWS_PC:/v4d_build/debug/modules/test_planets_rtx/assets/shaders/
	scp -rq build/debug/modules/test_planets_rtx/assets/shaders/*.meta WINDOWS_PC:/v4d_build/debug/modules/test_planets_rtx/assets/shaders/
done
