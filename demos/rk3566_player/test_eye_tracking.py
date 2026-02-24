#!/usr/bin/env python3
"""
Test script to send fake detection data to owl tracker demo.
Simulates a person moving across the camera frame to test look_dir transitions.

The owl_tracker_demo binds to /tmp/soulcam_scene.sock, same as SoulCam sends to.
"""

import socket
import json
import time
import math
import sys

SOCKET_PATH = "/tmp/soulcam_scene.sock"


def send_detection(x, y, conf=0.9):
    box_w = 100
    box_h = 200

    detection = {
        "source": "test",
        "type": "detections",
        "count": 1,
        "objects": [{
            "cls_id": 0,
            "label": "person",
            "conf": conf,
            "box": {
                "left": max(0, int(x - box_w / 2)),
                "top": max(0, int(y - box_h / 2)),
                "right": min(640, int(x + box_w / 2)),
                "bottom": min(640, int(y + box_h / 2))
            }
        }]
    }

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        sock.sendto(json.dumps(detection).encode(), SOCKET_PATH)
        normX = x / 640.0
        if normX < 0.33:
            look = "1 (left)"
        elif normX > 0.66:
            look = "2 (right)"
        else:
            look = "3 (center)"
        print(f"  person at ({x:3.0f}, {y:3.0f}) -> look_dir {look}")
    except Exception as e:
        print(f"  Error: {e}")
    finally:
        sock.close()


def send_no_detection():
    detection = {
        "source": "test",
        "type": "detections",
        "count": 0,
        "objects": []
    }
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
    try:
        sock.sendto(json.dumps(detection).encode(), SOCKET_PATH)
        print("  no person -> look_dir 0 (center)")
    except Exception as e:
        print(f"  Error: {e}")
    finally:
        sock.close()


def test_directions():
    """Test each look direction."""
    print("=== Testing look directions ===\n")

    print("1. No person (center):")
    send_no_detection()
    time.sleep(2)

    print("\n2. Person on LEFT (x=100):")
    for i in range(20):
        send_detection(100, 320)
        time.sleep(0.1)
    time.sleep(1)

    print("\n3. Person on RIGHT (x=540):")
    for i in range(20):
        send_detection(540, 320)
        time.sleep(0.1)
    time.sleep(1)

    print("\n4. Person in CENTER (x=320):")
    for i in range(20):
        send_detection(320, 320)
        time.sleep(0.1)
    time.sleep(1)

    print("\n5. No person (center):")
    send_no_detection()
    time.sleep(2)


def test_sweep():
    """Sweep person from left to right."""
    print("=== Sweep left to right ===\n")
    for x in range(50, 600, 10):
        send_detection(x, 320)
        time.sleep(0.15)
    print("\nDone.")


def test_circle():
    """Move person in a circle."""
    print("=== Circular movement ===\n")
    for i in range(180):
        angle = math.radians(i * 4)
        x = 320 + 250 * math.cos(angle)
        y = 320 + 250 * math.sin(angle)
        send_detection(x, y)
        time.sleep(0.05)
    print("\nDone.")


if __name__ == "__main__":
    print("=== Owl Eye Tracker Test ===")
    print(f"Sending to: {SOCKET_PATH}\n")

    mode = sys.argv[1] if len(sys.argv) > 1 else "directions"

    if mode == "directions":
        test_directions()
    elif mode == "sweep":
        test_sweep()
    elif mode == "circle":
        test_circle()
    else:
        print(f"Unknown mode: {mode}")
        print("Usage: test_eye_tracking.py [directions|sweep|circle]")
