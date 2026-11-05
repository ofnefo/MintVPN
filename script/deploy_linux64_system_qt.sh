#!/bin/bash
set -e

rm -rf $DEST
mkdir -p $DEST

#### copy binary ####
cp $GITHUB_WORKSPACE/build/Quattro $DEST

#### copy Quattro.png ####
cp $GITHUB_WORKSPACE/res/public/Quattro.png $DEST

#### copy Core ####
source "$(dirname "$0")/extract_core_artifact.sh"
cp deployment/${DEST_SUFFIX%-system-qt}/QuattroCore $DEST
rm -rf deployment/${DEST_SUFFIX%-system-qt}

# handle debug info
objcopy --only-keep-debug $DEST/Quattro $DEST/Quattro.debug
strip --strip-debug --strip-unneeded $DEST/Quattro
objcopy --add-gnu-debuglink=$DEST/Quattro.debug $DEST/Quattro
