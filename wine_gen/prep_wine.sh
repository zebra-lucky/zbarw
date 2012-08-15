#!/bin/bash

# The script to prepare wine dependencies for zbar, based on Klaus' notes.txt
# Copyright (C) 2012 Jarek Czekalski <jarekczek@poczta.onet.pl>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

# adjust variables below
zbarDir=`dirname $0`/..
wineSrcDir=/usr/src/winezbar

curDir=`dirname $0`
wineSrcArch=wine-1.4.tar.bz2
wineUrl=http://dfn.dl.sourceforge.net/project/wine/Source/$wineSrcArch

idlFiles="
qedit.idl
strmif.idl
amvideo.idl
amstream.idl
austream.idl
control.idl
ddstream.idl
mmstream.idl
msxml.idl
oaidl.idl
objidl.idl
ocidl.idl
oleidl.idl
propidl.idl
unknwn.idl
urlmon.idl
wtypes.idl
"

# if you prefer git over source archive
#git clone git://source.winehq.org/git/wine.git $wineSrcDir
#git reset --hard
#git checkout wine-1.4

if [[ ! -d $wineSrcDir ]]; then
  # unpack original wine source
  if [[ ! -f wine-1.4.tar.bz2 ]]; then
    echo "Get $wineSrcArch file first"
    echo "wget $wineUrl"
    exit 1
  fi
  mkdir $wineSrcDir
  cd $wineSrcDir
  tar -xj --strip-components=1 --wildcards -f $curDir/$wineSrcArch */include
else
  echo Using existing $wineSrcDir
fi

rm -rf $zbarDir/zbar/video/wine-1.4-zbar || exit $!
mkdir $zbarDir/zbar/video/wine-1.4-zbar
cp -R $wineSrcDir/include $zbarDir/zbar/video/wine-1.4-zbar/include
cd $zbarDir/zbar/video/wine-1.4-zbar
patch -p1 <$zbarDir/wine_gen/wine-1.4-zbar.patch
rm -rf $zbarDir/zbar/video/wine-1.4-zbar-dshow
mkdir $zbarDir/zbar/video/wine-1.4-zbar-dshow
cd $zbarDir/zbar/video/wine-1.4-zbar-dshow
for idlFile in $idlFiles; do
  widl --win32 -h -u -I "$zbarDir/zbar/video/wine-1.4-zbar/include" $zbarDir/zbar/video/wine-1.4-zbar/include/$idlFile
done

# The following part only to speed up repeated process of rebuilding wine
# and zbar. Normally one runs configure to generate automake deps files.
if [[ "x$1" == "xgen_deps" ]]; then
  depsDir=$zbarDir/zbar/video/wine-1.4-zbar-dshow/.deps
  if [[ ! -d $depsDir ]]; then
    mkdir $depsDir
    for plo in zbar_libzbar_la-control_i.Plo zbar_libzbar_la-oaidl_i.Plo \
    zbar_libzbar_la-qedit_i.Plo zbar_libzbar_la-strmif_i.Plo \
    zbar_libzbar_la-unknwn_i.Plo \
    ; do
      touch $depsDir/$plo
    done
  fi
fi

