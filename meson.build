project('kvsm', 'c', version: '0.1', default_options: ['warning_level=3'])

subdir('src')

subdir('include')

jpeg_dep = dependency('libjpeg')
udev_dep = dependency('libudev')
# Use SDL3 from git until this is released: https://github.com/libsdl-org/SDL/pull/12257
if get_option('system_sdl')
    sdl3_dep = dependency('sdl3', version: '>=3.2.0')
else
    cc = meson.get_compiler('c')
    m_dep = cc.find_library('m', required: false)

    cmake = import('cmake')
    cmake_opts = cmake.subproject_options()
    cmake_opts.add_cmake_defines(
        {'CMAKE_POSITION_INDEPENDENT_CODE': true, 'SDL_STATIC': true},
    )

    sdl3_proj = cmake.subproject('sdl3', options: cmake_opts)
    sdl3_dep = [sdl3_proj.dependency('SDL3-static'), m_dep]
endif

ch9329_dep = subproject('ch9329').get_variable('ch9329_dep')

executable(
    'kvsm',
    src,
    include_directories: include_dirs,
    dependencies: [sdl3_dep, jpeg_dep, udev_dep, ch9329_dep],
    install: true,
)
