def options(opt):
    opt.load('compiler_c compiler_cxx')

def add_compiler_flags(conf, flags):
    for v in ('CFLAGS', 'CXXFLAGS'):
        conf.env.append_value(v, flags)

def configure(conf):
    conf.load('compiler_c compiler_cxx')

    if conf.env.CC_NAME == 'gcc':
        add_compiler_flags(conf, '-g')

    conf.check_cfg(package='wayland-client', args=['--cflags', '--libs'], uselib_store='WAYLAND_CLIENT')
    conf.check_cfg(package='wayland-egl', args=['--cflags', '--libs'], uselib_store='WAYLAND_EGL')
    conf.check_cfg(package='wayland-cursor', args=['--cflags', '--libs'], uselib_store='WAYLAND_CURSOR')
    conf.check_cfg(package='glesv2', args=['--cflags', '--libs'], uselib_store='GLESV2')
    conf.check_cfg(package='egl', args=['--cflags', '--libs'], uselib_store='EGL')

    conf.env.INCLUDES_GLM = conf.path.make_node('glm-repo').abspath()

def build(bld):
    bld.objects(target='base', source='base.cc', use='WAYLAND_EGL WAYLAND_CLIENT GLESV2 EGL')

    bld.program(target='icosahedron', source='icosahedron.cc',
                use='base GLESV2 EGL',
                lib='m')

    bld.program(target='cube', source='cube.cc',
                use='base GLESV2 EGL GLM',
                lib='m')

    bld.program(target='spinny-triangle', source='spinny-triangle.cc', use='base GLESV2 EGL')
