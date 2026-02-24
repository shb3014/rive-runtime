#!/bin/bash
# Quick test script for owl tracker demo

TARGET_IP="${1:-192.168.1.45}"
TARGET_USER="${2:-ubuntu}"
PASSWORD="shb084ww"

echo "=== Owl Tracker Demo Test ==="
echo "Target: $TARGET_USER@$TARGET_IP"
echo ""

# Check if binary exists
echo "1. Checking if binary exists..."
sshpass -p "$PASSWORD" ssh $TARGET_USER@$TARGET_IP "test -f ~/rive-runtime/demos/rk3566_player/bin/release/owl_tracker_demo && echo 'Binary found' || echo 'Binary NOT found - run build_owl_demo_simple.sh'"

# Check if dress-up.riv exists
echo ""
echo "2. Checking if dress-up.riv exists..."
sshpass -p "$PASSWORD" ssh $TARGET_USER@$TARGET_IP "test -f ~/dress-up.riv && echo 'Rive file found' || echo 'Rive file NOT found - copy to ~/dress-up.riv'"

# Check if SoulCam is built
echo ""
echo "3. Checking if SoulCam is built..."
sshpass -p "$PASSWORD" ssh $TARGET_USER@$TARGET_IP "test -f ~/SoulCam/build/soulcam && echo 'SoulCam found' || echo 'SoulCam NOT found - build SoulCam first'"

# Check if YOLO model exists
echo ""
echo "4. Checking if YOLO model exists..."
sshpass -p "$PASSWORD" ssh $TARGET_USER@$TARGET_IP "test -f ~/SoulCam/rk3566/yolov8n.rknn && echo 'YOLO model found' || echo 'YOLO model NOT found'"

# Check Mesa PLS
echo ""
echo "5. Checking Mesa PLS installation..."
sshpass -p "$PASSWORD" ssh $TARGET_USER@$TARGET_IP "test -d /opt/mesa-pls && echo 'Mesa PLS found' || echo 'Mesa PLS NOT found'"

echo ""
echo "=== Test Complete ==="
echo ""
echo "If all checks passed, you can run the demo with:"
echo "  ssh $TARGET_USER@$TARGET_IP"
echo "  ~/run_owl_demo.sh"
echo ""
