#!/bin/bash
set -e
cd "$(dirname "$0")"

echo "=== Building Face Tracker Demo ==="

RIVE_ROOT=../..
RIVE_OUT=$RIVE_ROOT/out/release
RENDERER_OUT=$RIVE_ROOT/renderer/out/release

CXXFLAGS="-std=c++17 -O3 -DNDEBUG -DRIVE_ANDROID"
CXXFLAGS="$CXXFLAGS -I$RIVE_ROOT/include"
CXXFLAGS="$CXXFLAGS -I$RIVE_ROOT/renderer/include"
CXXFLAGS="$CXXFLAGS -I$RIVE_ROOT/renderer/src"
CXXFLAGS="$CXXFLAGS -I/usr/include/libdrm"

LDFLAGS="-fuse-ld=lld"
LDFLAGS="$LDFLAGS -Wl,--start-group"
LDFLAGS="$LDFLAGS $RENDERER_OUT/librive_pls_renderer.a"
LDFLAGS="$LDFLAGS $RENDERER_OUT/librive_decoders.a"
LDFLAGS="$LDFLAGS $RENDERER_OUT/liblibpng.a"
LDFLAGS="$LDFLAGS $RENDERER_OUT/libzlib.a"
LDFLAGS="$LDFLAGS $RENDERER_OUT/liblibjpeg.a"
LDFLAGS="$LDFLAGS $RENDERER_OUT/liblibwebp.a"
LDFLAGS="$LDFLAGS $RIVE_OUT/librive.a"
LDFLAGS="$LDFLAGS $RIVE_OUT/librive_harfbuzz.a"
LDFLAGS="$LDFLAGS $RIVE_OUT/librive_sheenbidi.a"
LDFLAGS="$LDFLAGS $RIVE_OUT/librive_yoga.a"
LDFLAGS="$LDFLAGS -Wl,--end-group"
LDFLAGS="$LDFLAGS -lEGL -lGLESv2 -ldrm -lgbm -lpthread -ldl -lm"

mkdir -p bin/release

echo "Compiling face_tracker_demo.cpp..."
clang++ $CXXFLAGS -c face_tracker_demo.cpp -o bin/release/face_tracker_demo.o

if [ ! -f bin/release/drm_egl_context.o ]; then
    echo "Compiling drm_egl_context.cpp..."
    clang++ $CXXFLAGS -c drm_egl_context.cpp -o bin/release/drm_egl_context.o
fi

echo "Linking..."
clang++ bin/release/face_tracker_demo.o bin/release/drm_egl_context.o \
    $LDFLAGS -o bin/release/face_tracker_demo

echo ""
echo "=== Build Complete ==="
echo "Binary: bin/release/face_tracker_demo"
