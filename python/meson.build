project('ufo-python', 'c',
        default_options : ['c_std=c99', 'warning_level=3'],
        version : '1.1.0')

pymod = import('python')
py3 = pymod.find_installation(required : true)
py3_dep = py3.dependency()

subdir('src')
subdir('ufo')
