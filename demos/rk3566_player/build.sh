#!/bin/bash
# Build script for RK3566 demo player

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
RIVE_ROOT="$(dirname "$(dirname "$SCRIPT_DIR")")"
BUILD_TYPE="${1:-release}"

echo "=== Building RK3566 Demo Player ==="
echo "Build type: $BUILD_TYPE"
echo ""

# Check if Rive libraries are built
RIVE_LIB_DIR="$RIVE_ROOT/out/arm64_$BUILD_TYPE"
if [ ! -d "$RIVE_LIB_DIR" ]; then
    echo "Error: Rive libraries not found at $RIVE_LIB_DIR"
    echo ""
    echo "Please build Rive first:"
    echo "  cd $RIVE_ROOT"
    echo "  ./build/cross_compile_rk3566.sh $BUILD_TYPE"
    exit 1
fi

echo "Using Rive libraries from: $RIVE_LIB_DIR"

# Check for sysroot
if [ -z "$SYSROOT" ]; then
    SYSROOT="$RIVE_ROOT/build/rk3566_sysroot"
fi

if [ ! -d "$SYSROOT" ]; then
    echo "Error: Sysroot not found at $SYSROOT"
    echo "Please set SYSROOT environment variable or run setup script."
    exit 1
fi

echo "Using sysroot: $SYSROOT"

# Setup cross-compilation environment
export CC="${CC:-aarch64-linux-gnu-gcc}"
export CXX="${CXX:-aarch64-linux-gnu-g++}"
export AR="${AR:-aarch64-linux-gnu-ar}"
export SYSROOT="$SYSROOT"

echo "Toolchain: $CC / $CXX"
echo ""

# Generate build files with premake5
cd "$SCRIPT_DIR"

# Find premake5
PREMAKE5="premake5"
if [ -f "$RIVE_ROOT/build/dependencies/premake-core/bin/v5.0.0-beta7_release/premake5" ]; then
    PREMAKE5="$RIVE_ROOT/build/dependencies/premake-core/bin/v5.0.0-beta7_release/premake5"
fi

echo "Generating build files with $PREMAKE5..."
$PREMAKE5 gmake2 --config=$BUILD_TYPE

# Build
echo ""
echo "Building..."
make config=$BUILD_TYPE -j$(nproc)

# Copy binary to root of demo directory for easy access
cp "bin/$BUILD_TYPE/rk3566_player" ./

echo ""
echo "=== Build Complete ==="
echo "Binary: $SCRIPT_DIR/rk3566_player"
echo ""
echo "To deploy to RK3566:"
echo "  scp rk3566_player root@<rk3566-ip>:/home/orangepi/"
echo ""
echo "To run on RK3566:"
echo "  ssh root@<rk3566-ip>"
echo "  cd /home/orangepi"
echo "  sudo ./rk3566_player /path/to/animation.riv"

