env = ['UFO_LAUNCH_BINARY=@0@'.format(ufo_launch.full_path())]
message('env @0@'.format(env))
test('147 regression', find_program('test-147.sh'), env: env)
