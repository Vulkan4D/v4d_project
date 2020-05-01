cd "`dirname $0`/../.."

inotifywait -e close_write "$1.meta"
if [[ -e "$1.meta" ]] ; then
	dir="`dirname $1`/"
	scp -rq "$1".*.spv WINDOWS_PC:/v4d_$dir
	scp -rq "$1.meta" WINDOWS_PC:/v4d_$dir
	sh -c "$0 \"$1\"" &
fi
