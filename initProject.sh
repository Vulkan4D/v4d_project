#!/bin/bash
cd "`dirname $0`"
PROJECT_DIR="`pwd`"
set -e

git submodule update --init tools
tools/gitPullSubmodules.sh

tools/cleanbuild.sh
