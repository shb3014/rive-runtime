#!/bin/bash
# Cross-compilation script for RK3566 (Orange Pi)
# This script sets up the environment for cross-compiling Rive for ARM64 Linux

set -e

# Color output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== RK3566 Cross-Compilation Setup ===${NC}"

# Detect script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
RIVE_ROOT="$(dirname "$SCRIPT_DIR")"

# Configuration
SYSROOT_DIR="${SYSROOT_DIR:-$RIVE_ROOT/build/rk3566_sysroot}"
TOOLCHAIN_PREFIX="${TOOLCHAIN_PREFIX:-aarch64-linux-gnu}"
BUILD_TYPE="${1:-release}"

echo -e "${YELLOW}Configuration:${NC}"
echo "  SYSROOT_DIR: $SYSROOT_DIR"
echo "  TOOLCHAIN_PREFIX: $TOOLCHAIN_PREFIX"
echo "  BUILD_TYPE: $BUILD_TYPE"
echo ""

# Check for cross-compiler
if ! command -v ${TOOLCHAIN_PREFIX}-gcc &> /dev/null; then
    echo -e "${RED}ERROR: Cross-compiler not found!${NC}"
    echo "Please install ARM64 cross-compiler:"
    echo "  Ubuntu/Debian: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
    echo "  Or install from ARM's official toolchain"
    exit 1
fi

echo -e "${GREEN}✓ Cross-compiler found: $(${TOOLCHAIN_PREFIX}-gcc --version | head -n1)${NC}"

# Check for sysroot
if [ ! -d "$SYSROOT_DIR" ]; then
    echo -e "${YELLOW}WARNING: Sysroot not found at $SYSROOT_DIR${NC}"
    echo "Creating sysroot directory structure..."
    mkdir -p "$SYSROOT_DIR"/{lib,usr/{lib,include}}
    
    echo ""
    echo -e "${YELLOW}Please populate the sysroot with files from your Ubuntu 24.04 device:${NC}"
    echo "1. Use the setup script:"
    echo "   ./build/setup_rk3566_sysroot.sh <device-ip>"
    echo ""
    echo "2. Or manually copy libraries:"
    echo "   scp -r ubuntu@<device-ip>:/usr/lib/aarch64-linux-gnu $SYSROOT_DIR/usr/lib/"
    echo "   scp -r ubuntu@<device-ip>:/lib/aarch64-linux-gnu $SYSROOT_DIR/lib/"
    echo ""
    echo "3. Copy headers:"
    echo "   scp -r ubuntu@<device-ip>:/usr/include/EGL $SYSROOT_DIR/usr/include/"
    echo "   scp -r ubuntu@<device-ip>:/usr/include/GLES2 $SYSROOT_DIR/usr/include/"
    echo "   scp -r ubuntu@<device-ip>:/usr/include/GLES3 $SYSROOT_DIR/usr/include/"
    echo "   scp -r ubuntu@<device-ip>:/usr/include/KHR $SYSROOT_DIR/usr/include/"
    echo "   scp -r ubuntu@<device-ip>:/usr/include/drm $SYSROOT_DIR/usr/include/"
    echo "   scp -r ubuntu@<device-ip>:/usr/include/libdrm $SYSROOT_DIR/usr/include/"
    echo "   scp ubuntu@<device-ip>:/usr/include/gbm.h $SYSROOT_DIR/usr/include/"
    echo ""
    echo -e "${YELLOW}Note: Ubuntu 24.04 uses Mesa/Panfork drivers (no libmali.so)${NC}"
    echo -e "${YELLOW}After copying, re-run this script.${NC}"
    exit 1
fi

# Verify critical libraries exist
CRITICAL_LIBS=("libEGL.so" "libGLESv2.so" "libdrm.so" "libgbm.so")
MISSING_LIBS=0

for lib in "${CRITICAL_LIBS[@]}"; do
    if ! find "$SYSROOT_DIR" -name "$lib*" | grep -q .; then
        echo -e "${RED}✗ Missing library: $lib${NC}"
        MISSING_LIBS=$((MISSING_LIBS + 1))
    else
        echo -e "${GREEN}✓ Found: $lib${NC}"
    fi
done

# Note: This device uses Mesa drivers instead of libmali
echo -e "${YELLOW}Note: Using Mesa drivers (no libmali.so required)${NC}"

if [ $MISSING_LIBS -gt 0 ]; then
    echo -e "${RED}ERROR: $MISSING_LIBS critical libraries missing from sysroot${NC}"
    echo "Please populate the sysroot as described above."
    exit 1
fi

echo ""
echo -e "${GREEN}=== Pre-building Premake (natively) ===${NC}"
# Build premake natively BEFORE setting cross-compile environment
cd "$RIVE_ROOT"
mkdir -p build/dependencies
cd build/dependencies
RIVE_PREMAKE_TAG="v5.0.0-beta7"
PREMAKE_INSTALL_DIR="$RIVE_ROOT/build/dependencies/premake-core/bin/${RIVE_PREMAKE_TAG}_release"
if [ ! -f "$PREMAKE_INSTALL_DIR/premake5" ]; then
    echo "Building Premake natively for host..."
    rm -fr premake-core
    git clone --depth 1 --branch $RIVE_PREMAKE_TAG https://github.com/premake/premake-core.git
    cd premake-core
    make -f Bootstrap.mak linux
    mkdir -p bin/${RIVE_PREMAKE_TAG}_release
    cp bin/release/premake5 bin/${RIVE_PREMAKE_TAG}_release/
    cd ..
    echo -e "${GREEN}✓ Premake built successfully${NC}"
else
    echo -e "${GREEN}✓ Premake already built${NC}"
fi
cd "$RIVE_ROOT"

echo ""

# Export cross-compilation environment variables
export CC="${TOOLCHAIN_PREFIX}-gcc"
export CXX="${TOOLCHAIN_PREFIX}-g++"
export AR="${TOOLCHAIN_PREFIX}-ar"
export RANLIB="${TOOLCHAIN_PREFIX}-ranlib"
export STRIP="${TOOLCHAIN_PREFIX}-strip"
export SYSROOT="$SYSROOT_DIR"

# Add custom flags for cross-compilation
export CFLAGS="--sysroot=$SYSROOT_DIR -march=armv8-a"
export CXXFLAGS="--sysroot=$SYSROOT_DIR -march=armv8-a"
export LDFLAGS="--sysroot=$SYSROOT_DIR -Wl,-rpath-link,$SYSROOT_DIR/usr/lib/aarch64-linux-gnu:$SYSROOT_DIR/lib/aarch64-linux-gnu"

echo ""
echo -e "${GREEN}=== Environment configured for cross-compilation ===${NC}"
echo "CC: $CC"
echo "CXX: $CXX"
echo ""

# Build Rive runtime and renderer
cd "$RIVE_ROOT"

echo -e "${GREEN}=== Building Rive Runtime and Renderer ===${NC}"
export PATH="$SCRIPT_DIR:$PATH"

# Create a marker file to indicate cross-compilation
export RIVE_CROSS_COMPILE_RK3566=1

# Note: We define RIVE_ANDROID to use the EGL/GLES code paths
# This is appropriate for embedded Linux with Mali GPU
# Skip text/layout support to avoid HarfBuzz compiler issues with older GCC
RIVE_PREMAKE_ARGS=""

echo "Running build script with RK3566 cross-compilation settings..."
echo "Note: Building without text/layout support to avoid compiler compatibility issues"
./build/build_rive.sh "$BUILD_TYPE" --arch=arm64 $RIVE_PREMAKE_ARGS

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo "Libraries are in: $(realpath out/arm64_$BUILD_TYPE)"
echo ""
echo "Next steps:"
echo "1. Build the demo player: cd demos/rk3566_player && ./build.sh"
echo "2. Deploy to device: scp rk3566_player ubuntu@<device-ip>:~/"
echo "3. Run on device: ssh ubuntu@<device-ip> 'sudo ./rk3566_player /path/to/file.riv'"

