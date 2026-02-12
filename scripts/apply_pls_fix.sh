#!/bin/bash
# Script to apply PLS implementation fix and rebuild on Orange Pi

set -e

echo "=== Applying PLS Implementation Fix for Panfrost ==="
echo ""

cd ~/rive-runtime

echo "1. Backing up original files..."
cp renderer/src/gl/render_context_gl_impl.cpp renderer/src/gl/render_context_gl_impl.cpp.backup
cp demos/rk3566_player/rk3566_drm_player.cpp demos/rk3566_player/rk3566_drm_player.cpp.backup

echo ""
echo "2. Files backed up successfully!"
echo ""
echo "3. Please manually apply the following changes:"
echo ""
echo "=== CHANGE 1: renderer/src/gl/render_context_gl_impl.cpp ==="
echo "Around line 2989, ADD these lines after the ANGLE_shader_pixel_local_storage_coherent block:"
echo ""
cat << 'EOF'
        // On non-Android GLES platforms (e.g. Linux with Panfrost/Mali),
        // use EXT_shader_pixel_local_storage if available. Mesa 26.0+ enables
        // this on Panfrost (Mesa commit 298ad17b81e).
        if (capabilities.EXT_shader_pixel_local_storage &&
            (capabilities.ARM_shader_framebuffer_fetch ||
             capabilities.EXT_shader_framebuffer_fetch))
        {
            return MakeContext(rendererString,
                               capabilities,
                               MakePLSImplEXTNative(capabilities),
                               contextOptions.shaderCompilationMode);
        }
EOF

echo ""
echo "=== CHANGE 2: demos/rk3566_player/rk3566_drm_player.cpp ==="
echo "Around line 150, REPLACE the existing logging with:"
echo ""
cat << 'EOF'
        std::cout << "Rive renderer initialized" << std::endl;
        
        // Print renderer capabilities and PLS implementation details
        auto renderContextGL = m_renderContext->static_impl_cast<RenderContextGLImpl>();
        const auto& caps = renderContextGL->capabilities();
        std::cout << "\n=== Renderer Capabilities ===" << std::endl;
        std::cout << "EXT_shader_pixel_local_storage: " << (caps.EXT_shader_pixel_local_storage ? "YES" : "NO") << std::endl;
        std::cout << "EXT_shader_pixel_local_storage2: " << (caps.EXT_shader_pixel_local_storage2 ? "YES" : "NO") << std::endl;
        std::cout << "ANGLE_shader_pixel_local_storage: " << (caps.ANGLE_shader_pixel_local_storage ? "YES" : "NO") << std::endl;
        std::cout << "ARB_shader_image_load_store: " << (caps.ARB_shader_image_load_store ? "YES" : "NO") << std::endl;
        std::cout << "ARM_shader_framebuffer_fetch: " << (caps.ARM_shader_framebuffer_fetch ? "YES" : "NO") << std::endl;
        std::cout << "EXT_shader_framebuffer_fetch: " << (caps.EXT_shader_framebuffer_fetch ? "YES" : "NO") << std::endl;
        std::cout << "EXT_color_buffer_integer: " << (caps.EXT_color_buffer_integer ? "YES" : "NO") << std::endl;
        std::cout << "EXT_color_buffer_float: " << (caps.EXT_color_buffer_float ? "YES" : "NO") << std::endl;
        std::cout << "needsFloatingPointTessellationTexture: " << (caps.needsFloatingPointTessellationTexture ? "YES" : "NO") << std::endl;
        std::cout << "PLS Implementation: " << (renderContextGL->plsImpl() ? "ACTIVE" : "NONE (MSAA fallback)") << std::endl;
        
        // Check for GL errors after renderer creation
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) {
            std::cerr << "GL Error after renderer init: 0x" << std::hex << err << std::dec << std::endl;
        }
EOF

echo ""
echo "=== Changes described above ==="
echo ""
echo "To restore backups if needed:"
echo "  cp renderer/src/gl/render_context_gl_impl.cpp.backup renderer/src/gl/render_context_gl_impl.cpp"
echo "  cp demos/rk3566_player/rk3566_drm_player.cpp.backup demos/rk3566_player/rk3566_drm_player.cpp"
echo ""

