# RK3566 Rive Player - Quick Start

Get running in 5 steps!

## Prerequisites

- RK3566 device (Orange Pi) with IP address
- Development machine with Linux or WSL2
- SSH access to RK3566

## Step 1: Install Tools (5 minutes)

```bash
sudo apt update
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
    build-essential git
```

## Step 2: Setup Sysroot (5 minutes)

```bash
cd rive-runtime/build
chmod +x setup_rk3566_sysroot.sh
./setup_rk3566_sysroot.sh <YOUR_RK3566_IP>
```

Enter root password when prompted.

## Step 3: Build Rive (15-30 minutes)

```bash
cd rive-runtime
chmod +x build/cross_compile_rk3566.sh
./build/cross_compile_rk3566.sh release
```

Go get coffee ☕ while it builds...

## Step 4: Build Demo Player (2 minutes)

```bash
cd demos/rk3566_player
chmod +x build.sh
export SYSROOT="$(pwd)/../../build/rk3566_sysroot"
./build.sh release
```

## Step 5: Deploy & Run (2 minutes)

```bash
# Transfer to RK3566
scp rk3566_player orangepi@<RK3566_IP>:~/
scp your_animation.riv orangepi@<RK3566_IP>:~/

# SSH to RK3566 and run
ssh orangepi@<RK3566_IP>
sudo ./rk3566_player your_animation.riv
```

## Expected Output

```
=== RK3566 Rive Player ===
Initializing DRM/EGL...
Display: 1920x1080@60Hz
=== Starting Render Loop ===
FPS: 60.0 (16.7 ms/frame)
```

Press `Ctrl+C` to quit.

## Troubleshooting

| Problem | Solution |
|---------|----------|
| Can't open DRM device | Run with `sudo` or add user to video group |
| Failed to initialize EGL | Install: `sudo apt install libegl1 libgles2` |
| File not found | Check .riv file path is correct |

## Get .riv Files

Download free animations: https://rive.app/community/

## Full Documentation

See [`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md) for detailed instructions and troubleshooting.

## Need Help?

1. Check [`README.md`](README.md) for architecture details
2. Read [`DEPLOYMENT_GUIDE.md`](DEPLOYMENT_GUIDE.md) for comprehensive guide
3. Visit https://rive.app/community/ for support

