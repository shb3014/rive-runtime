#!/bin/bash
# Build owl_tracker_demo on device

set -e

cd "$(dirname "$0")"

echo "=== Building Owl Tracker Demo ==="

# Generate build files
premake5 gmake2

# Build release version
make config=release owl_tracker_demo -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "Binary: bin/release/owl_tracker_demo"
echo ""
echo "Usage:"
echo "  ./bin/release/owl_tracker_demo ~/dress-up.riv"
echo ""
