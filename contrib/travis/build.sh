#!/bin/bash

BUILD_DIR=/root/zbar
SRC_DIR=$BUILD_DIR/zbarw

# win32
cd $SRC_DIR
source contrib/travis/version_env.sh

git clean -fd

autoreconf -i

./configure --host=i686-w64-mingw32 --prefix=/usr/local/win32 \
    --with-directshow \
    --without-gtk \
    --without-python \
    --without-qt --without-java \
    --without-imagemagick \
    --enable-pthread

make

make install

BUILD32=$BUILD_DIR/win32
mkdir -p $BUILD32/zbarw
cp /usr/local/win32/bin/libzbar-0.dll $BUILD32/zbarw
cp /usr/local/win32/bin/zbarcam.exe $BUILD32/zbarw
cp /usr/i686-w64-mingw32/bin/iconv.dll $BUILD32/zbarw
cp /usr/i686-w64-mingw32/lib/libwinpthread-1.dll $BUILD32/zbarw
cp /usr/lib/gcc/i686-w64-mingw32/7.3-win32/libgcc_s_sjlj-1.dll $BUILD_DIR/win32/zbarw
cd $BUILD32
zip -r $BUILD_DIR/zbarw-zbarcam-$ZBAR_VERSION-win32.zip zbarw

# win64
cd $SRC_DIR

git clean -fd

autoreconf -i

./configure --host=x86_64-w64-mingw32 --prefix=/usr/local/win64 \
    --with-directshow \
    --without-gtk \
    --without-python \
    --without-qt --without-java \
    --without-imagemagick \
    --enable-pthread

make

make install

BUILD64=$BUILD_DIR/win64
mkdir -p $BUILD64/zbarw
cp /usr/local/win64/bin/libzbar-0.dll $BUILD64/zbarw
cp /usr/local/win64/bin/zbarcam.exe $BUILD64/zbarw
cp /usr/x86_64-w64-mingw32/bin/iconv.dll $BUILD64/zbarw
cp /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll $BUILD64/zbarw
cd $BUILD64
zip -r $BUILD_DIR/zbarw-zbarcam-$ZBAR_VERSION-win64.zip zbarw
