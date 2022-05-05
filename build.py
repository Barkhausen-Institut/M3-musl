import src.tools.ninjagen as ninjagen
import os

def build(gen, env):
    if env['PLATF'] != 'kachel':
        return

    env = env.clone()

    isa = 'riscv64' if env['ISA'] == 'riscv' else env['ISA']

    env['CPPPATH'] = [
        'src/libs/musl/m3/include',
        'src/libs/musl/m3/include/' + env['ISA'],
        'src/libs/musl/arch/' + isa,
        'src/libs/musl/arch/generic',
        'src/libs/musl/src/internal',
        'src/libs/musl/src/include',
        'src/libs/musl/src/internal',
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
    ]
    env['CPPFLAGS'] += ['-D_XOPEN_SOURCE=700', '-U_GNU_SOURCE']

    # disable lto for now, since it doesn't work here ('plugin needed to handle lto object')
    # I don't know why it works for libm3, but not for libc.
    env.remove_flag('CXXFLAGS', '-flto')
    env.remove_flag('CFLAGS', '-flto')

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
        files += env.glob('src/' + d + '/' + isa + '/*')
        files += env.glob('src/' + d + '/*.c')

    # directories where we can't use all files
    files += [
        'src/env/clearenv.c', 'src/env/getenv.c', 'src/env/putenv.c',
        'src/env/secure_getenv.c', 'src/env/setenv.c', 'src/env/unsetenv.c'
    ]
    files += [
        'src/exit/abort_lock.c', 'src/exit/assert.c',
        'src/exit/at_quick_exit.c', 'src/exit/quick_exit.c'
    ]
    files += [f for f in env.glob('src/errno/*.c') if os.path.basename(f) != '__errno_location.c']
    files += [f for f in env.glob('src/internal/*.c') if os.path.basename(f) != 'libc.c']

    # m3-specific files
    files += [
        'm3/dir.cc', 'm3/file.cc', 'm3/process.cc', 'm3/socket.cc', 'm3/syscall.cc',
        'm3/time.cc', 'm3/misc.cc',
    ]

    # files we want to have for bare-metal components
    simple_files  = ['src/env/__environ.c']
    simple_files += ['src/errno/__errno_location.c']
    simple_files += ['src/exit/atexit.c', 'src/exit/exit.c']
    simple_files += env.glob('src/exit/' + isa + '/*')
    simple_files += ['src/internal/libc.c']
    simple_files += env.glob('src/string/*.c')
    simple_files += env.glob('src/string/' + isa + '/*')
    for f in env.glob('src/malloc/*.c'):
        filename = os.path.basename(f)
        if filename != 'lite_malloc.c' and filename != 'free.c' and filename != 'realloc.c':
            simple_files += [f]
    simple_files += env.glob('src/malloc/mallocng/*.c')
    simple_files += [
        'm3/debug.cc', 'm3/exit.cc', 'm3/heap.cc', 'm3/init.c', 'm3/lock.c', 'm3/malloc.cc',
        'm3/pthread.c'
    ]

    # disallow FPU instructions, because we use the library for e.g. TileMux as well
    simple_env = env.clone()
    if simple_env['ISA'] == 'x86_64':
        simple_env['CFLAGS'] += ['-msoft-float', '-mno-sse']
    elif simple_env['ISA'] == 'riscv':
        simple_env['CFLAGS'] += ['-march=rv64imac']

    simple_objs = simple_env.objs(gen, simple_files)

    # simple C library without dependencies (but also no stdio, etc.)
    lib = env.static_lib(gen, out = 'libsimplec', ins = simple_objs)
    env.install(gen, env['LIBDIR'], lib)

    # full C library
    lib = env.static_lib(gen, out = 'libc', ins = files + simple_objs)
    env.install(gen, env['LIBDIR'], lib)
