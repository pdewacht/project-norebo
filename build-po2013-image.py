#!/usr/bin/env python3
import sys, os, os.path, logging, subprocess, re, csv, zipfile, io, requests

NOREBO_ROOT = os.path.dirname(os.path.realpath(__file__))
FILE_LIST = list(csv.DictReader(open(os.path.join(NOREBO_ROOT, 'build-po2013-image.csv'))))


def download_files(target_dir):
    with requests.Session() as session:
        session.headers.update({'User-Agent': 'project-norebo/1.0'})
        for fi in FILE_LIST:
            resp = session.get(fi['url'])
            resp.raise_for_status()
            data = resp.content
            if fi['mode'] in ('text', 'source'):
                data = re.sub(b'\r?\n', b'\r', data)
                with open(os.path.join(target_dir, fi['filename']), 'wb') as f:
                    f.write(data)
            elif fi['mode'] == 'archive':
                fi['members'] = []
                with zipfile.ZipFile(io.BytesIO(data)) as zf:
                    for member in zf.infolist():
                        fn = os.path.basename(member.filename)
                        if not fn.endswith('.txt'):
                            with open(os.path.join(target_dir, fn), 'wb') as f:
                                f.write(zf.read(member))
                            fi['members'].append(fn)


def bulk_delete(dir, ext):
    for fn in os.listdir(dir):
        parts = fn.split('.')
        if parts[-1] == ext:
            os.remove(os.path.join(dir, fn))

def bulk_rename(dir, old_ext, new_ext):
    for fn in os.listdir(dir):
        parts = fn.split('.')
        if parts[-1] == old_ext:
            parts[-1] = new_ext
            os.rename(os.path.join(dir, fn),
                      os.path.join(dir, '.'.join(parts)))

def mksubdir(parent, subdir):
    fn = os.path.join(parent, subdir)
    os.mkdir(fn)
    return fn


def norebo(args, working_directory='.', search_path=()):
    norebo = os.path.join(NOREBO_ROOT, 'norebo')
    norebo_path = os.pathsep.join(search_path)
    os.environ['NOREBO_PATH'] = norebo_path
    logging.debug('Running norebo\n\tCWD = %s\n\tPATH = %s\n\t%s',
                  working_directory, norebo_path, ' '.join(args))
    subprocess.check_call([norebo] + list(args), cwd=working_directory)

def compile(modules, **kwargs):
    norebo(['ORP.Compile'] + [m+'/s' for m in modules], **kwargs)


def build_norebo(target_dir):
    compile(['Norebo.Mod', 'Kernel.Mod', 'FileDir.Mod', 'Files.Mod',
             'Modules.Mod', 'Fonts.Mod', 'Texts.Mod', 'RS232.Mod', 'Oberon.Mod',
             'ORS.Mod', 'ORB.Mod', 'ORG.Mod', 'ORP.Mod', 'CoreLinker.Mod',
             'VDisk.Mod', 'VFileDir.Mod', 'VFiles.Mod', 'VDiskUtil.Mod'],
            working_directory=target_dir,
            search_path=[os.path.join(NOREBO_ROOT, 'Norebo'),
                         os.path.join(NOREBO_ROOT, 'Oberon'),
                         os.path.join(NOREBO_ROOT, 'Bootstrap')])

    bulk_rename(target_dir, 'rsc', 'rsx')
    norebo(['CoreLinker.LinkSerial', 'Modules', 'InnerCore'],
           working_directory=target_dir,
           search_path=[os.path.join(NOREBO_ROOT, 'Norebo'),
                        os.path.join(NOREBO_ROOT, 'Bootstrap')])
    bulk_rename(target_dir, 'rsx', 'rsc')


def build_image(target_dir):
    # sources_dir = mksubdir(target_dir, 'sources')
    target_dir = os.path.realpath(target_dir)
    os.mkdir(target_dir)
    sources_dir = mksubdir(target_dir, 'sources')
    norebo_dir = mksubdir(target_dir, 'norebo')
    compiler_dir = mksubdir(target_dir, 'compiler')
    oberon_dir = mksubdir(target_dir, 'oberon')

    logging.info('Downloading sources')
    download_files(sources_dir)

    logging.info('Building norebo')
    build_norebo(norebo_dir)

    logging.info('Building a cross-compiler')
    compile(['ORS.Mod', 'ORB.Mod', 'ORG.Mod', 'ORP.Mod'],
            working_directory=compiler_dir,
            search_path=[sources_dir, compiler_dir, norebo_dir])

    # Delete all symbol files, so that we don't accidentally link against Norebo.
    bulk_delete(norebo_dir, 'smb')
    bulk_delete(compiler_dir, 'smb')

    logging.info('Compiling the complete Project Oberon 2013')
    compile([fi['filename'] for fi in FILE_LIST if fi['mode'] == 'source'],
            working_directory=oberon_dir,
            search_path=[sources_dir, compiler_dir, norebo_dir])

    logging.info('Linking the Inner Core')
    # Hide the rsc files, Norebo can't use them (CoreLinker knows to expect this extension)
    bulk_rename(oberon_dir, 'rsc', 'rsx')
    norebo(['CoreLinker.LinkDisk', 'Modules', 'Oberon.dsk'],
           working_directory=target_dir,
           search_path=[oberon_dir, norebo_dir])

    logging.info('Installing files')

    def copy(src, dst):
        return '%s=>%s' % (src, dst)

    install_args = ['Oberon.dsk']
    for fi in FILE_LIST:
        if fi['mode'] in ('text', 'source'):
            install_args.append(copy(fi['filename'], fi['filename']))
        if fi['mode'] == 'source':
            smb = fi['filename'].replace('.Mod', '.smb')
            rsx = fi['filename'].replace('.Mod', '.rsx')
            rsc = fi['filename'].replace('.Mod', '.rsc')
            install_args.append(copy(smb, smb))
            install_args.append(copy(rsx, rsc))
        if fi['mode'] == 'archive':
            install_args.extend(copy(fn, fn) for fn in fi['members'])

    norebo(['VDiskUtil.InstallFiles'] + install_args,
           working_directory=target_dir,
           search_path=[oberon_dir, sources_dir, norebo_dir])

    logging.info('All done! Finished disk image is %s', os.path.join(target_dir, 'Oberon.dsk'))


def main():
    logging.basicConfig(format='%(levelname)s: %(message)s', level=logging.INFO)
    if len(sys.argv) != 2 or os.path.exists(sys.argv[1]):
        logging.error("Usage: %s TARGET-DIR", __file__)
        logging.error("  (TARGET_DIR must not already exist.)")
        sys.exit(1)
    build_image(sys.argv[1])


if __name__ == '__main__':
    main()
