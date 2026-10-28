#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2024-2026 do-i and thumbgrid contributors
# Part of thumbgrid, a fork of easymodo/qimgv (GPLv3).
# This builds a complete thumbgrid-x64 package. Result is placed in <repo>/thumbgrid-x64_<version>
# NOTE: the easymodo/qimgv-deps-bin and easymodo/qt-builds URLs below are upstream
# prebuilt build dependencies (Qt/mpv), not product branding; left as-is.
# Warning: Some stuff will be left over behind after building (C:/qt)

#CFL='-ffunction-sections -fdata-sections -march=native -mtune=native -O3 -pipe'
CFL='-ffunction-sections -fdata-sections -O3 -pipe'
LDFL='-Wl,--gc-sections'

if [[ -z "$RUNNER_TEMP" ]]; then
  MSYS_DIR="C:/msys64/mingw64"
else
  MSYS_DIR="$(cd "$RUNNER_TEMP" && pwd)/msys64/mingw64"
fi
CUSTOM_QT_DIR="C:/qt/5.15.3-mingw64-slim"
SCRIPTS_DIR=$(dirname $(readlink -f $0)) # this file's location (/path/to/thumbgrid/scripts)
SRC_DIR=$(dirname $SCRIPTS_DIR)
BUILD_DIR=$SRC_DIR/build
EXT_DIR=$SRC_DIR/_external
rm -rf "$EXT_DIR"
mkdir "$EXT_DIR"
MPV_DIR=$EXT_DIR/mpv

# ------------------------------------------------------------------------------
echo "PREPARING BUILD DIR"
rm -rf $BUILD_DIR
mkdir -p $BUILD_DIR

# ------------------------------------------------------------------------------
echo "UPDATING DEPENDENCY LIST"
wget --progress=dot:mega -O $BUILD_DIR/msys2-build-deps.txt https://raw.githubusercontent.com/easymodo/qimgv-deps-bin/main/msys2-build-deps.txt
wget --progress=dot:mega -O $BUILD_DIR/msys2-dll-deps.txt https://raw.githubusercontent.com/easymodo/qimgv-deps-bin/main/msys2-dll-deps.txt

# ------------------------------------------------------------------------------
echo "INSTALLING MSYS2 BUILD DEPS"
MSYS_DEPS=$(cat $BUILD_DIR/msys2-build-deps.txt | sed 's/\n/ /')
pacman -S $MSYS_DEPS --noconfirm --needed

# ------------------------------------------------------------------------------
echo "GETTING Qt"
mkdir C:/qt
cd C:/qt
wget --progress=dot:mega -O 5.15.3-mingw64-slim.7z https://github.com/easymodo/qt-builds/releases/download/5.15.3-mingw64-slim/5.15.3-mingw64-slim.7z
7z x 5.15.3-mingw64-slim.7z -y
rm 5.15.3-mingw64-slim.7z

# ------------------------------------------------------------------------------
echo "GETTING MPV"
mkdir $MPV_DIR
cd $MPV_DIR
wget --progress=dot:mega -O mpv-x64.7z https://github.com/easymodo/qimgv-deps-bin/releases/download/x64/mpv-x86_64-20230402-git-0f13c38.7z
7z x mpv-x64.7z -y
rm mpv-x64.7z

# ------------------------------------------------------------------------------
# ------------------------------------------------------------------------------
echo "BUILDING"
#rm -rf $BUILD_DIR
cmake -S $SRC_DIR -B $BUILD_DIR -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR \
    -DMPV_DIR=$MPV_DIR \
    -DCMAKE_CXX_FLAGS="$CFL" -DCMAKE_EXE_LINKER_FLAGS="$LDFL"
ninja -C $BUILD_DIR

# ------------------------------------------------------------------------------
echo "BUILDING IMAGEFORMATS"
# We are using libjxl from MSYS2 but feel free to compile instead
#cd $EXT_DIR
#git clone --depth 1 https://github.com/libjxl/libjxl.git --recursive
#cd libjxl
#rm -rf build
#cmake -S . -B build -G "Ninja" \
#    -DCMAKE_INSTALL_PREFIX=$MSYS_DIR \
#    -DCMAKE_BUILD_TYPE=Release -DJPEGXL_ENABLE_PLUGINS=OFF \
#    -DBUILD_TESTING=OFF -DJPEGXL_WARNINGS_AS_ERRORS=OFF \
#    -DJPEGXL_ENABLE_SJPEG=OFF -DJPEGXL_ENABLE_BENCHMARK=OFF \
#    -DJPEGXL_ENABLE_EXAMPLES=OFF -DJPEGXL_ENABLE_MANPAGES=OFF \
#    -DJPEGXL_FORCE_SYSTEM_BROTLI=ON
#ninja install -C build

# qt-jpegxl-image-plugin
cd $EXT_DIR
git clone --depth 1 https://github.com/novomesk/qt-jpegxl-image-plugin.git
cd qt-jpegxl-image-plugin
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_MAJOR_VERSION=5 \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR
ninja -C build

# qt-avif-image-plugin
cd $EXT_DIR
git clone https://github.com/novomesk/qt-avif-image-plugin
cd qt-avif-image-plugin
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DQT_MAJOR_VERSION=5 \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR
ninja -C build

# QtApng
cd $EXT_DIR
git clone https://github.com/Skycoder42/QtApng.git
cd QtApng
rm -rf build
mkdir build && cd build
$CUSTOM_QT_DIR/bin/qmake.exe ..
mingw32-make -j4

# qt-heif-image-plugin
cd $EXT_DIR
git clone https://github.com/jakar/qt-heif-image-plugin.git
cd qt-heif-image-plugin
rm -rf build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$CUSTOM_QT_DIR
ninja -C build

# qtraw
cd $EXT_DIR
git clone https://gitlab.com/mardy/qtraw
cd qtraw
rm -rf build
mkdir build && cd build
$CUSTOM_QT_DIR/bin/qmake.exe .. DEFINES+="LIBRAW_WIN32_CALLS=1"
mingw32-make -j4

# ------------------------------------------------------------------------------
echo "PACKAGING"
# 0 - prepare dir
cd $SRC_DIR
BUILD_NAME=thumbgrid-x64_$(git describe --tags)
PACKAGE_DIR=$SRC_DIR/$BUILD_NAME
rm -rf $PACKAGE_DIR
mkdir $PACKAGE_DIR

# 1 - copy thumbgrid build
cp $BUILD_DIR/src/thumbgrid.exe $PACKAGE_DIR
mkdir $PACKAGE_DIR/plugins
cp $BUILD_DIR/plugins/player_mpv/player_mpv.dll $PACKAGE_DIR/plugins
cp -r $BUILD_DIR/src/translations/ $PACKAGE_DIR/

# 2 - copy qt dlls
cd $CUSTOM_QT_DIR/bin
cp Qt6Core.dll Qt6Gui.dll Qt6PrintSupport.dll Qt6Svg.dll Qt6Widgets.dll Qt6OpenGL.dll Qt6OpenGLWidgets.dll $PACKAGE_DIR
cd $CUSTOM_QT_DIR/plugins
cp -r iconengines imageformats printsupport styles $PACKAGE_DIR
mkdir $PACKAGE_DIR/platforms
cp platforms/qwindows.dll $PACKAGE_DIR/platforms

# 3 - copy msys dlls
MSYS_DLLS=$(cat $BUILD_DIR/msys2-dll-deps.txt | sed 's/\n/ /')
cd $MSYS_DIR/bin
cp $MSYS_DLLS $PACKAGE_DIR

# 4 - copy imageformats
cp $EXT_DIR/qt-jpegxl-image-plugin/build/bin/imageformats/libqjpegxl5.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/qt-avif-image-plugin/build/bin/imageformats/libqavif5.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/QtApng/build/plugins/imageformats/qapng.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/qt-heif-image-plugin/build/bin/imageformats/libqheif.dll $PACKAGE_DIR/imageformats
cp $EXT_DIR/qtraw/build/src/imageformats/qtraw.dll $PACKAGE_DIR/imageformats

# 5 - copy mpv
cd $MPV_DIR/bin/x86_64
cp mpv.exe libmpv-2.dll $PACKAGE_DIR

# 6 - misc
mkdir $PACKAGE_DIR/cache
mkdir $PACKAGE_DIR/conf
mkdir $PACKAGE_DIR/thumbnails
cp -r $SRC_DIR/src/distrib/mimedata/data $PACKAGE_DIR

cd $SRC_DIR
echo "PACKAGING DONE"
