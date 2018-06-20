#!/bin/bash
set -ev

BUILD_REPO_URL=https://github.com/zebra-lucky/zbarw

cd build

git clone --branch $TRAVIS_TAG $BUILD_REPO_URL zbarw

docker run --rm \
    -v $(pwd):/root/zbar \
    -t zebralucky/zbarw-build \
    /root/zbar/zbarw/contrib/travis/build.sh
