import os
from ninjapie import BuildPath


def prepare(env):
    env['CPPPATH'] = [
        'src/libs/musl/m3/include',
        'src/libs/musl/m3/include/' + env['ISA'],
        'src/libs/musl/arch/' + env['ISA'],
        'src/libs/musl/arch/generic',
        'src/libs/musl/src/internal',
        'src/libs/musl/src/include',
        'src/libs/musl/include',
        'src/include',
        env['CROSSDIR'] + '/include/c++/' + env['CROSSVER'],
        env['CROSSDIR'] + '/include/c++/' + env['CROSSVER'] + '/' + env['CROSS'][:-1],
    ]

    # disable the warnings that musl produces
    env['CFLAGS'] += [
        '-Wno-parentheses',
        '-Wno-unused-parameter',
        '-Wno-sign-conversion',
        '-Wno-sign-compare',
        '-Wno-unused-value',
        '-Wno-unused-function',
        '-Wno-implicit-fallthrough',
        '-Wno-missing-attributes',
        '-Wno-missing-braces',
        '-Wno-format-contains-nul',
        '-Wno-unused-but-set-variable',
        '-Wno-unknown-pragmas',
        '-Wno-maybe-uninitialized',
        '-Wno-return-local-addr',
        '-Wno-missing-field-initializers',
        '-Wno-stringop-truncation',
        '-Wno-old-style-declaration',
        '-Wno-type-limits',
        '-Wno-cast-function-type',
        '-Wno-array-parameter',
        '-Wno-dangling-pointer',
    ]
    env['CPPFLAGS'] += ['-D_XOPEN_SOURCE=700', '-U_GNU_SOURCE']

    # disable lto for now, since it doesn't work here ('plugin needed to handle lto object')
    # I don't know why it works for libm3, but not for libc.
    env.remove_flag('CXXFLAGS', '-flto=auto')
    env.remove_flag('CFLAGS', '-flto=auto')


def build(gen, env):
    env = env.clone()
    prepare(env)

    # files that we only want to have in the full C library
    files = []

    # directories with all files
    dirs = [
        'aio', 'complex', 'conf', 'crypt', 'ctype', 'dirent', 'fcntl', 'fenv', 'ipc',
        'ldso', 'legacy', 'linux', 'locale', 'math', 'misc', 'mman', 'mq', 'multibyte', 'network',
        'passwd', 'prng', 'process', 'regex', 'sched', 'search', 'select', 'setjmp', 'signal',
        'stat', 'stdio', 'stdlib', 'temp', 'termios', 'thread', 'time', 'unistd',
    ]
    for d in dirs:
        files += env.glob(gen, 'src/' + d + '/' + env['ISA'] + '/*')
        files += env.glob(gen, 'src/' + d + '/*.c')

    # directories where we can't use all files
    files += [
        'src/env/clearenv.c', 'src/env/getenv.c', 'src/env/putenv.c',
        'src/env/secure_getenv.c', 'src/env/setenv.c', 'src/env/unsetenv.c'
    ]
    files += [
        'src/exit/abort_lock.c', 'src/exit/assert.c',
        'src/exit/at_quick_exit.c', 'src/exit/quick_exit.c'
    ]
    files += [f for f in env.glob(gen, 'src/errno/*.c')
              if os.path.basename(f) != '__errno_location.c']
    files += [f for f in env.glob(gen, 'src/internal/*.c') if os.path.basename(f) != 'libc.c']

    # m3-specific files
    files += [
        'm3/dir.cc', 'm3/file.cc', 'm3/process.cc', 'm3/socket.cc', 'm3/syscall.cc',
        'm3/time.cc', 'm3/misc.cc', 'm3/epoll.cc'
    ]

    # files we want to have for bare-metal components
    simple_files = ['src/env/__environ.c']
    simple_files += ['src/errno/__errno_location.c']
    simple_files += ['src/exit/atexit.c', 'src/exit/exit.c']
    simple_files += ['src/internal/libc.c']
    simple_files += env.glob(gen, 'src/string/*.c')
    for f in env.glob(gen, 'src/malloc/*.c'):
        filename = os.path.basename(f)
        if filename != 'lite_malloc.c' and filename != 'free.c' and filename != 'realloc.c':
            simple_files += [f]
    simple_files += env.glob(gen, 'src/malloc/mallocng/*.c')
    simple_files += [
        'm3/debug.cc', 'm3/exit.cc', 'm3/heap.cc', 'm3/init.c', 'm3/lock.c', 'm3/malloc.cc',
        'm3/pthread.c', 'm3/dl.cc', 'm3/tls.cc'
    ]

    for isa in env['ALL_ISAS']:
        for sf in [True, False]:
            tenv = env.new(isa, sf)
            prepare(tenv)

            sfiles = simple_files.copy()
            sfiles += tenv.glob(gen, 'src/exit/' + isa + '/*')
            sfiles += tenv.glob(gen, 'src/string/' + isa + '/*')

            objs = tenv.objs(gen, ins=sfiles)
            lib = tenv.static_lib(gen, out='simplec-' + isa + '-' + str(sf),
                                  ins=objs + ['m3/simple.cc'])
            out = BuildPath(tenv['LIBDIR'] + '/libsimplec.a')
            tenv.install_as(gen, out, lib)
            if isa == env['ISA'] and not sf:
                our_simple_objs = objs

    # full C library
    lib = env.static_lib(gen, out='c', ins=files + our_simple_objs)
    env.install(gen, env['LIBDIR'], lib)
