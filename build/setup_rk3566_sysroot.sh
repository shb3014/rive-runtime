#!/bin/bash
# Script to help setup RK3566 sysroot from target device
# Usage: ./setup_rk3566_sysroot.sh <rk3566-ip-address> [sysroot-path]

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

if [ -z "$1" ]; then
    echo -e "${RED}ERROR: No IP address provided${NC}"
    echo "Usage: $0 <rk3566-ip-address> [sysroot-path]"
    echo "Example: $0 192.168.1.100"
    exit 1
fi

RK3566_IP="$1"
RK3566_USER="${RK3566_USER:-ubuntu}"
RK3566_PASS="${RK3566_PASS:-shb084ww}"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
SYSROOT_DIR="${2:-$SCRIPT_DIR/rk3566_sysroot}"

echo -e "${GREEN}=== Setting up RK3566 Sysroot ===${NC}"
echo "Target device: $RK3566_IP"
echo "Sysroot path: $SYSROOT_DIR"
echo ""

# Create sysroot directory structure
mkdir -p "$SYSROOT_DIR"/{lib,usr/{lib,include}}
mkdir -p "$SYSROOT_DIR/usr/lib/aarch64-linux-gnu"
mkdir -p "$SYSROOT_DIR/lib/aarch64-linux-gnu"

echo -e "${YELLOW}Testing SSH connection...${NC}"
if ! sshpass -p "$RK3566_PASS" ssh -o ConnectTimeout=5 -o StrictHostKeyChecking=no $RK3566_USER@$RK3566_IP "echo 'Connection successful'"; then
    echo -e "${RED}ERROR: Cannot connect to $RK3566_USER@$RK3566_IP${NC}"
    echo "Please ensure:"
    echo "  1. RK3566 is powered on and connected to network"
    echo "  2. SSH is enabled on the device"
    echo "  3. You can login as $RK3566_USER user"
    exit 1
fi

echo -e "${GREEN}✓ SSH connection successful${NC}"
echo ""

# Copy libraries
echo -e "${YELLOW}Copying libraries from Ubuntu 24.04...${NC}"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/lib/aarch64-linux-gnu/{libEGL*.so*,libGLESv*.so*,libdrm*.so*,libgbm*.so*,libpthread.so*,libc.so*,libm.so*,libdl.so*,librt.so*} "$SYSROOT_DIR/usr/lib/aarch64-linux-gnu/" 2>/dev/null || echo "Some libraries not found (this may be OK)"

sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/lib/aarch64-linux-gnu/{libc.so*,libm.so*,libpthread.so*,libdl.so*,librt.so*,ld-linux-aarch64.so*} "$SYSROOT_DIR/lib/aarch64-linux-gnu/" 2>/dev/null || echo "Some libraries not found (this may be OK)"

# Copy headers
echo -e "${YELLOW}Copying headers from RK3566...${NC}"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/include/EGL "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "EGL headers not found"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/include/GLES2 "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "GLES2 headers not found"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/include/GLES3 "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "GLES3 headers not found"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/include/KHR "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "KHR headers not found"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/include/drm "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "DRM headers not found"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no -r $RK3566_USER@$RK3566_IP:/usr/include/libdrm "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "libdrm headers not found"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no $RK3566_USER@$RK3566_IP:/usr/include/gbm.h "$SYSROOT_DIR/usr/include/" 2>/dev/null || echo "gbm.h not found"

# Copy pkg-config files if available
echo -e "${YELLOW}Copying pkg-config files...${NC}"
mkdir -p "$SYSROOT_DIR/usr/lib/pkgconfig"
sshpass -p "$RK3566_PASS" scp -o StrictHostKeyChecking=no $RK3566_USER@$RK3566_IP:/usr/lib/aarch64-linux-gnu/pkgconfig/{egl.pc,glesv2.pc,gbm.pc,libdrm.pc} "$SYSROOT_DIR/usr/lib/pkgconfig/" 2>/dev/null || echo "Some pkg-config files not found (this may be OK)"

echo ""
echo -e "${GREEN}=== Sysroot setup complete ===${NC}"
echo "Sysroot location: $SYSROOT_DIR"
echo ""

# Verify critical files
echo -e "${YELLOW}Verifying critical files...${NC}"
CRITICAL_FILES=(
    "usr/lib/aarch64-linux-gnu/libEGL.so.1"
    "usr/lib/aarch64-linux-gnu/libGLESv2.so.2"
    "usr/include/EGL/egl.h"
    "usr/include/GLES3/gl3.h"
    "usr/include/drm/drm.h"
)

ALL_GOOD=1
for file in "${CRITICAL_FILES[@]}"; do
    if [ -f "$SYSROOT_DIR/$file" ] || [ -L "$SYSROOT_DIR/$file" ]; then
        echo -e "${GREEN}✓ $file${NC}"
    else
        echo -e "${RED}✗ $file (MISSING)${NC}"
        ALL_GOOD=0
    fi
done

if [ $ALL_GOOD -eq 1 ]; then
    echo ""
    echo -e "${GREEN}✓ All critical files present!${NC}"
    echo "You can now run: ./build/cross_compile_rk3566.sh release"
else
    echo ""
    echo -e "${YELLOW}WARNING: Some files are missing. Build may fail.${NC}"
    echo "You may need to manually install packages on Ubuntu 24.04:"
    echo "  sudo apt install libdrm-dev libgbm-dev libegl1-mesa-dev libgles2-mesa-dev"
    echo ""
    echo -e "${YELLOW}Note: Ubuntu 24.04 with Panfork uses Mesa drivers (no libmali.so)${NC}"
fi

