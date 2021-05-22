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
    ]
    env['CPPFLAGS'] += ['-D_XOPEN_SOURCE=700']

    # disable lto for now, since it doesn't work here ('plugin needed to handle lto object')
    # I don't know why it works for libm3, but not for libc.
    env.remove_flag('CXXFLAGS', '-flto')
    env.remove_flag('CFLAGS', '-flto')

    # files that we only want to have in the full C library
    files  = env.glob('src/ctype/*.c')
    #files += ['src/env/__libc_start_main.c']
    files += ['src/errno/strerror.c']
    files += ['src/exit/assert.c']
    files += env.glob('src/dirent/*.c')
    files += env.glob('src/fcntl/*.c')
    files += [f for f in env.glob('src/internal/*.c') if os.path.basename(f) != 'libc.c']
    files += env.glob('src/locale/*.c')
    files += env.glob('src/math/*.c')
    files += env.glob('src/math/' + isa + '/*')
    files += env.glob('src/misc/*.c')
    files += env.glob('src/mman/*.c')
    files += env.glob('src/multibyte/*.c')
    files += env.glob('src/network/*.c')
    files += env.glob('src/prng/*.c')
    files += env.glob('src/regex/*.c')
    files += env.glob('src/search/*.c')
    files += env.glob('src/setjmp/*.c')
    files += env.glob('src/setjmp/' + isa + '/*')
    files += env.glob('src/stat/*.c')
    files += env.glob('src/stdio/*.c')
    files += env.glob('src/stdlib/*.c')
    files += env.glob('src/temp/*.c')
    files += env.glob('src/time/*.c')
    files += env.glob('src/unistd/*.c')
    files += ['m3/file.cc', 'm3/syscall.cc']

    # files we want to have for bare-metal components
    simple_files  = ['src/env/__environ.c']
    simple_files += ['src/exit/atexit.c', 'src/exit/exit.c']
    simple_files += env.glob('src/exit/' + isa + '/*')
    simple_files += ['src/internal/libc.c']
    simple_files += env.glob('src/string/*.c')
    simple_files += env.glob('src/string/' + isa + '/*')
    simple_files += ['m3/debug.cc', 'm3/errno.c', 'm3/init.c', 'm3/lock.c', 'm3/exit.cc']
    simple_objs = env.objs(gen, simple_files)

    # simple C library without dependencies (but also no stdio, etc.)
    lib = env.static_lib(gen, out = 'libsimplec', ins = simple_objs)
    env.install(gen, env['LIBDIR'], lib)

    # full C library
    lib = env.static_lib(gen, out = 'libc', ins = files + simple_objs)
    env.install(gen, env['LIBDIR'], lib)
