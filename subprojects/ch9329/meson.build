project('ch9329', 'c', version: '0.1', default_options: ['warning_level=3'])

subdir('src')

subdir('include')

#executable(
#    'ch9329',
#    src,
#    include_directories: include_dirs,
#    install: false,
#)

ch9329_lib = library(
    'ch9329',
    src,
    include_directories: include_dirs,
    install: true,
)

ch9329_dep = declare_dependency(
    include_directories: include_dirs,
    link_with: ch9329_lib,
)
