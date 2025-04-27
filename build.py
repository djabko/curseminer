#!/bin/python

import os
import subprocess

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


def get_all_files(dirs, ext):
    if type(dirs) is not list:
        raise Exception('Parameter "dirs" must be type list or str')

    srcs = []

    for d in dirs:
        for item in os.walk(d, topdown=True):
            path, dirs, files = item

            for filename in files:
                if filename[-len(ext):] == ext:
                    srcs.append(f'{path}/{filename}')

    return srcs


def remove_path(path):
    if not os.path.exists(path):
        return

    elif os.path.isfile(path):
        os.remove(path)

    elif os.path.isdir(path):
        for item in os.walk(path, topdown=False):
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

def ensure_path_exists(path):
    if not (type(path) is str or type(path) is os.PathLike or type(path) is bytes):
        raise Exception('Parameter "path" must be string or os.PathLike, not {type(path)}')

    if path[-1] != '/':
        path = '/'.join(path.split('/')[:-1])

    if not os.path.isdir(path):
        os.mkdir(path)
        return False

    return True



def source_modified(src, obj):
    if not (os.path.isfile(src) and os.path.isfile(obj)):
        return True

    srcmt = os.path.getmtime(src)
    objmt = os.path.getmtime(obj)

    return srcmt > objmt


def build_obj(srcfile, extra_cflags, dry=False):
    objfile = srcfile.replace(SRCDIR, OBJDIR)[:-1] + 'o'
    ensure_path_exists(objfile)

    cflags = f'{INCPATHS} {CF_SDL2} -c -o {objfile}'.split(' ')
    cflags.extend(extra_cflags)

    cmd = [CC, srcfile, *cflags]

    if dry:
        print(' '.join(cmd))

    elif source_modified(srcfile, objfile):
        return subprocess.Popen(cmd)


def build_exe(extra_cflags=None, dry=False):

    extra_cflags = [] if extra_cflags is None else extra_cflags
    cflags = f'{LIBS_SDL2} {CF_LIBS} {CF_SDL2} {INCPATHS} -o {TARGET}'.split(' ')
    cflags.extend(extra_cflags)

    if type(extra_cflags) is not list:
        raise Exception('Parameter "extra_cflags" must by type list')

    objfiles = get_all_files([OBJDIR], '.o')

    cmd = [CC, *objfiles, *cflags]

    if dry:
        print(' '.join(cmd))
    else: 
        return subprocess.Popen(cmd)


if __name__ == '__main__':
    from sys import argv
    
    ensure_path_exists(OBJDIR)

    mode = argv[1] if 1 < len(argv) else 'debug'
    target = os.path.basename(CWD)

    sources = get_all_files([SRCDIR], '.c')

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
