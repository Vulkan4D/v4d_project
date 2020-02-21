cd `dirname $0`
find build/debug -type f -name "*.watch.sh" -exec sh -c '{} &' \;
