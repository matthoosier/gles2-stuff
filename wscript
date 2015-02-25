def options(opt):
	opt.load('compiler_c compiler_cxx')

def configure(conf):
	conf.load('compiler_c compiler_cxx')
	conf.check_cfg(package='wayland-client', args=['--cflags', '--libs'], uselib_store='WAYLAND_CLIENT')
	conf.check_cfg(package='wayland-egl', args=['--cflags', '--libs'], uselib_store='WAYLAND_EGL')
	conf.check_cfg(package='wayland-cursor', args=['--cflags', '--libs'], uselib_store='WAYLAND_CURSOR')
	conf.check_cfg(package='glesv2', args=['--cflags', '--libs'], uselib_store='GLESV2')
	conf.check_cfg(package='egl', args=['--cflags', '--libs'], uselib_store='EGL')

def build(bld):
	bld.program(target='simple-egl', source='simple-egl.cc',
	            use='WAYLAND_EGL WAYLAND_CURSOR WAYLAND_CLIENT GLESV2 EGL',
	            lib='m')
	bld.program(target='icosahedron', source='icosahedron.cc',
	            use='WAYLAND_EGL WAYLAND_CURSOR WAYLAND_CLIENT GLESV2 EGL',
	            lib='m')
