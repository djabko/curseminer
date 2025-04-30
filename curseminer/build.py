#!/bin/python

import os, subprocess
from pathlib import Path

CWD = os.getcwd()

CC =            'gcc'
CF_LIBS =       '-lncurses -lm -ldl -lSDL2'
CF_SDL2 =       subprocess.check_output('sdl2-config --cflags', shell=True, text=True).strip()
LIBS_SDL2 =     subprocess.check_output('sdl2-config --libs', shell=True, text=True).strip()

OBJDIR =        './obj'
LOGSDIR =       './logs'
OPT_OBJDIR =    './obj_opt'

INCDIRS =       ['./include', './vendor']
SRCDIR =        './src'

TARGET =        os.path.basename(CWD)

INCPATHS =      ' '.join([f'-I{incdir}' for incdir in INCDIRS])
CF =            f'{LIBS_SDL2} {CF_SDL2} {CF_LIBS} {INCPATHS}'
CF_DEBUG =      f'-Wall -g -DDEBUG'
CF_OPTIM =      f'-O3'


def get_all_files(dirs, ext, max_depth=None):
    if type(dirs) is not list:
        raise Exception('Parameter "dirs" must be type list')

    srcs = []

    for d in dirs:
        for depth, item in enumerate(os.walk(d, topdown=True)):
            if (max_depth is not None and max_depth <= depth):
                break

            path, dirs, files = item

            base = path.split('/')[-1]
            if 0 < len(base) and base[0] == '.':
                continue

            for filename in files:
                if filename[0] == '.':
                    continue

                elif filename[-len(ext):] == ext:
                    srcs.append(f'{path}/{filename}')

    return srcs


def remove_path(path, max_depth=None):
    if not os.path.exists(path):
        return

    elif os.path.isfile(path):
        os.remove(path)

    elif os.path.isdir(path):
        for depth, item in enumerate(os.walk(path, topdown=False)):
            if (max_depth is not None and max_depth <= depth):
                break

            p, d, f = item
            nodes = d + f

            for e in nodes:
                e = f'{p}/{e}'
                if os.path.isfile(e):
                    os.remove(e)
                    print(f'Removing file {e}')

                elif os.path.isdir(e):
                    os.rmdir(e)
                    print(f'Removing dir {e}')

                else:
                    print(f'Skipping {e}')

        os.rmdir(path)

    else:
        raise f'Invalid argument "{path}"'

def remove_files(paths):
    for path in paths:
        os.remove(path)

def ensure_path_exists(path):
    if not (type(path) is str or type(path) is os.PathLike or type(path) is bytes):
        raise Exception('Parameter "path" must be string or os.PathLike, not {type(path)}')

    if path[-1] != '/':
        path = '/'.join(path.split('/')[:-1])

    if not os.path.isdir(path):
        Path(path).mkdir(parents=True, exist_ok=True)
        return False

    return True



def source_modified(src, obj):
    if not (os.path.isfile(src) and os.path.isfile(obj)):
        return True

    srcmt = os.path.getmtime(src)
    objmt = os.path.getmtime(obj)

    return srcmt > objmt


def build_obj(srcfile, extra_cflags, dry=False):
    objfile = f'{OBJDIR}/' + '.'.join(srcfile.split('.')[:-1]) + '.o'
    ensure_path_exists(objfile)

    cflags = f'{INCPATHS} {CF_SDL2} -c -o {objfile}'.split(' ')
    cflags.extend(extra_cflags)

    cmd = [CC, srcfile, *cflags]

    if source_modified(srcfile, objfile):
        print(' '.join(cmd))
        if not dry:
            return subprocess.Popen(cmd)
    else:
        print(f'Skipping {srcfile}')


def build_exe(extra_cflags=None, dry=False):

    extra_cflags = [] if extra_cflags is None else extra_cflags
    cflags = f'{LIBS_SDL2} {CF_LIBS} -o {TARGET}'.split(' ')
    cflags.extend(extra_cflags)

    if type(extra_cflags) is not list:
        raise Exception('Parameter "extra_cflags" must by type list')

    objfiles = get_all_files([OBJDIR], '.o')

    cmd = [CC, *objfiles, *cflags]

    print(' '.join(cmd))
    if not dry:
        return subprocess.Popen(cmd)


if __name__ == '__main__':
    from sys import argv
    
    ensure_path_exists(OBJDIR)

    mode = argv[1] if 1 < len(argv) else 'debug'
    target = os.path.basename(CWD)

    sources = get_all_files([SRCDIR], '.c')
    print(sources)

    dry_run = True
    dry_run = False

    processes = []
    # processes_max = os.cpu_count()

    print('Compiling')

    if mode == 'debug' and bool(sources):
        for src in sources:
            proc = build_obj(src, CF_DEBUG.split(' '), dry=dry_run)

            if proc:
                processes.append(proc)

    elif mode == 'clean':
        remove_path(OBJDIR)
        remove_path(TARGET)
        
        remove_files(get_all_files(['./'], '.log'))
        exit(0)

    if 0 < len(processes):
        for proc in processes:
            print(' '.join(proc.args))
            proc.wait()

    print()
    print('Linking')

    proc = build_exe(extra_cflags=CF_DEBUG.split(' '), dry=dry_run)

    if proc:
        proc.wait()

    print()
    print("Done!")
