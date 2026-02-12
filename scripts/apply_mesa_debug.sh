#!/bin/bash
# Apply debugging patch to Mesa and rebuild

DEVICE_IP="192.168.1.45"
DEVICE_USER="ubuntu"
#
# IMPORTANT: Do NOT hardcode passwords in this repo.
# Use SSH keys, or provide credentials via your SSH agent/config.

echo "=== Applying Mesa Debug Patch ===" 
echo ""

# Copy patch to device
scp panfrost_debug.patch ${DEVICE_USER}@${DEVICE_IP}:~/

# Apply patch, rebuild, and reinstall
ssh ${DEVICE_USER}@${DEVICE_IP} << 'ENDSSH'
cd ~/mesa

echo "Current Mesa commit:"
git log --oneline -1

echo ""
echo "=== Adding debug logging to pan_context.c ==="

# Add logging directly without patch file
cat > /tmp/debug_additions.txt << 'EOF'

Add after line ~1000 in src/gallium/drivers/panfrost/pan_context.c
(in panfrost_draw_vbo function):

   /* DEBUG: Log draw calls for tessellation debugging */
   static int draw_count = 0;
   if (draw_count < 100) {
      fprintf(stderr, "[PAN_DEBUG] Draw #%d: mode=%d count=%d instances=%d indexed=%d\n",
              draw_count++, info->mode, info->count, info->instance_count, 
              info->index_size > 0 ? 1 : 0);
   }

EOF

# Manual insertion into pan_context.c
sed -i '/struct panfrost_batch \*batch = panfrost_get_batch_for_fbo(ctx);/a\
\
   /* DEBUG: Log draw calls */\
   static int draw_count = 0;\
   if (draw_count < 100) {\
      fprintf(stderr, "[PAN_DEBUG] Draw \\#%d: mode=%d count=%d inst=%d idx=%d\\n",\
              draw_count++, info->mode, info->count, info->instance_count,\
              info->index_size > 0 ? 1 : 0);\
   }' src/gallium/drivers/panfrost/pan_context.c

echo "Logging added to pan_context.c"

# Add GPU detection logging
sed -i '/case PIPE_CAP_DRAW_PARAMETERS:/a\
      /* DEBUG: Log GPU info */\
      {\
         static bool gpu_logged = false;\
         if (!gpu_logged) {\
            fprintf(stderr, "[PAN_DEBUG] GPU: 0x%x\\n", panfrost_device_gpu_id(dev));\
            gpu_logged = true;\
         }\
      }' src/gallium/drivers/panfrost/pan_screen.c

echo "Logging added to pan_screen.c"

echo ""
echo "=== Rebuilding Mesa (this takes 5-10 minutes) ==="
cd build
time ninja

if [ $? -ne 0 ]; then
    echo "ERROR: Build failed!"
    exit 1
fi

echo ""
echo "=== Reinstalling Mesa ==="
DESTDIR=/tmp/mesa-install ninja install
sudo rm -rf /opt/mesa-pls
sudo mv /tmp/mesa-install/opt/mesa-pls /opt/

echo ""
echo "=== Mesa Debug Build Complete ==="
ls -lh /opt/mesa-pls/lib/aarch64-linux-gnu/libEGL.so*

ENDSSH

echo ""
echo "=== Debug Build Complete ==="
echo ""
echo "To test:"
echo "  ssh ubuntu@${DEVICE_IP}"
echo "  cd ~/rive-runtime/demos/rk3566_player/bin/release"
echo "  export LD_LIBRARY_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu:\$LD_LIBRARY_PATH"
echo "  export LIBGL_DRIVERS_PATH=/opt/mesa-pls/lib/aarch64-linux-gnu/dri"
echo "  ./rk3566_player ~/dress-up.riv 2>&1 | grep PAN_DEBUG"

