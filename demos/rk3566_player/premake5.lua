-- RK3566 Demo Player Build Configuration

RIVE_RUNTIME_DIR = path.getabsolute('../..')
RIVE_PLS_DIR = path.getabsolute('../../renderer')

dofile(RIVE_RUNTIME_DIR .. '/build/rive_build_config.lua')

workspace('rk3566_player')
configurations({ 'debug', 'release' })

project('rk3566_player')
kind('ConsoleApp')
language('C++')
cppdialect('C++17')
targetdir('bin/%{cfg.buildcfg}')
objdir('obj/%{cfg.buildcfg}')

files({
    'rk3566_drm_player.cpp',
    'drm_egl_context.cpp',
    'drm_egl_context.h',
})

includedirs({
    RIVE_RUNTIME_DIR .. '/include',
    RIVE_PLS_DIR .. '/include',
    RIVE_PLS_DIR .. '/src',
})

-- System include directories (for cross-compilation)
if os.getenv('SYSROOT') then
    local sysroot = os.getenv('SYSROOT')
    externalincludedirs({
        sysroot .. '/usr/include',
        sysroot .. '/usr/include/aarch64-linux-gnu',
        sysroot .. '/usr/include/libdrm',
    })
    libdirs({
        sysroot .. '/usr/lib/aarch64-linux-gnu',
        sysroot .. '/lib/aarch64-linux-gnu',
    })
else
    -- Native build: add system include paths
    externalincludedirs({
        '/usr/include/libdrm',
    })
end

-- Link Rive libraries
-- Native build uses 'out/release', cross-compiled uses 'out/arm64_release'
if os.getenv('SYSROOT') then
    libdirs({
        RIVE_RUNTIME_DIR .. '/out/arm64_%{cfg.buildcfg}',
    })
else
    libdirs({
        RIVE_RUNTIME_DIR .. '/out/%{cfg.buildcfg}',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}',
    })
end

-- Use linkoptions to specify link group for circular dependencies
if os.getenv('SYSROOT') then
    -- Cross-compilation: use -l: syntax
    linkoptions({
        '-Wl,--start-group',
        '-l:librive_pls_renderer.a',
        '-l:librive.a',
        '-l:librive_sheenbidi.a',
        '-l:librive_yoga.a',
        '-Wl,--end-group',
    })
else
    -- Native build: use full paths for LLVM LTO linking
    linkoptions({
        '-fuse-ld=lld',  -- Use LLVM linker for LTO objects
        '-Wl,--start-group',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}/librive_pls_renderer.a',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}/librive_decoders.a',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}/liblibpng.a',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}/libzlib.a',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}/liblibjpeg.a',
        RIVE_PLS_DIR .. '/out/%{cfg.buildcfg}/liblibwebp.a',
        RIVE_RUNTIME_DIR .. '/out/%{cfg.buildcfg}/librive.a',
        RIVE_RUNTIME_DIR .. '/out/%{cfg.buildcfg}/librive_harfbuzz.a',
        RIVE_RUNTIME_DIR .. '/out/%{cfg.buildcfg}/librive_sheenbidi.a',
        RIVE_RUNTIME_DIR .. '/out/%{cfg.buildcfg}/librive_yoga.a',
        '-Wl,--end-group',
    })
end

-- System libraries for DRM/EGL
links({
    'EGL',
    'GLESv2',  -- Changed from GLESv3
    'drm',
    'gbm',
    'pthread',
    'dl',
    'm',
})

defines({
    'RIVE_ANDROID', -- Use Android/GLES code paths
})

filter('configurations:debug')
do
    defines({ 'DEBUG' })
    symbols('On')
    optimize('Off')
end

filter('configurations:release')
do
    defines({ 'NDEBUG' })
    optimize('Speed')
end

filter({})

-- Cross-compilation settings
if os.getenv('CC') then
    toolset('gcc')
    local sysroot = os.getenv('SYSROOT') or ''
    buildoptions({ '--sysroot=' .. sysroot })
    linkoptions({ 
        '--sysroot=' .. sysroot,
        '-Wl,-rpath-link,' .. sysroot .. '/lib/aarch64-linux-gnu',
        '-Wl,-rpath-link,' .. sysroot .. '/usr/lib/aarch64-linux-gnu',
        '-Wl,--dynamic-linker=/lib/ld-linux-aarch64.so.1'
    })
end

