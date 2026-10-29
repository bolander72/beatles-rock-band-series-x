#!/usr/bin/env python3
"""Package a user's extracted game for local Device Portal transfer. BSD-3-Clause."""
import argparse
import hashlib
import json
import struct
import sys
import zipfile
from pathlib import Path


def is_beatles(path):
    try:
        with path.open('rb') as stream:
            header = stream.read(24)
            if len(header) != 24:
                return False
            magic, flags, size, _, _, count = struct.unpack('>6I', header)
            if magic != 0x58455832 or not flags & 1 or flags & (8 | 16 | 32 | 64):
                return False
            if count > 4096 or 24 + count * 8 > size or size > path.stat().st_size:
                return False
            for _ in range(count):
                key, offset = struct.unpack('>2I', stream.read(8))
                if key == 0x00040006:
                    if offset < 24 + count * 8 or offset + 24 > size:
                        return False
                    stream.seek(offset + 12)
                    return struct.unpack('>I', stream.read(4))[0] == 0x454108B1
    except (OSError, struct.error):
        pass
    return False


def package(game, output, target_mib=1024):
    game, output = game.resolve(), output.resolve()
    if output == game or game in output.parents:
        raise ValueError('Choose an output folder outside the game folder.')
    if not is_beatles(game / 'default.xex'):
        raise ValueError('Select an extracted Beatles Xbox 360 folder containing a valid default.xex.')
    if not (game / 'gen/main_xbox.hdr').is_file() or not list((game / 'gen').glob('*.ark')):
        raise ValueError('The accompanying gen data files are missing; default.xex alone is not the game.')
    files = []
    for path in sorted(game.rglob('*')):
        relative = path.relative_to(game)
        if any(n == '.DS_Store' or n == '.git' or n.startswith('._') for n in relative.parts):
            continue
        if path.is_symlink():
            raise ValueError('Symbolic links are not supported; supply ordinary extracted game files.')
        if path.is_file():
            files.append(path)
    if list(output.glob('beatles-game-part-*.zip')) or (output / 'transfer-manifest.json').exists():
        raise ValueError('Output already contains a transfer set. Choose a new output folder.')
    output.mkdir(parents=True, exist_ok=True)
    groups, group, size = [], [], 0
    limit = target_mib * 1024 * 1024
    for path in files:
        length = path.stat().st_size
        if group and size + length > limit:
            groups.append(group)
            group, size = [], 0
        group.append(path)
        size += length
    if group:
        groups.append(group)
    manifest = {'destination': 'DevelopmentFiles', 'game_folder': 'Games/xbox360/Beatles-Rock-Band',
                'file_count': len(files), 'total_bytes': sum(f.stat().st_size for f in files), 'archives': []}
    for number, group in enumerate(groups, 1):
        path = output / ('beatles-game-part-%02d.zip' % number)
        print('Creating %s (%d files)...' % (path.name, len(group)), flush=True)
        with zipfile.ZipFile(path, 'x', compression=zipfile.ZIP_STORED) as archive:
            for file in group:
                name = Path('Games/xbox360/Beatles-Rock-Band') / file.relative_to(game)
                archive.write(file, name.as_posix())
        digest = hashlib.sha256()
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
                digest.update(chunk)
        manifest['archives'].append({'name': path.name, 'bytes': path.stat().st_size, 'sha256': digest.hexdigest()})
    (output / 'transfer-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('Ready: %d archives, %d game files. In Device Portal > File explorer > DevelopmentFiles,' % (len(groups), len(files)))
    print('upload each ZIP in order and wait for extraction to finish. Then open the game tile.')
    print('These locally generated ZIPs contain your game files. Keep them out of the public repository.')
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('game_folder', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--target-mib', type=int, default=1024,
                        help='Target group size; a larger individual file gets its own ZIP without splitting.')
    args = parser.parse_args()
    if args.target_mib <= 0:
        parser.error('--target-mib must be positive')
    try:
        package(args.game_folder, args.output, args.target_mib)
    except (ValueError, OSError, zipfile.BadZipFile) as error:
        print('Error: ' + str(error), file=sys.stderr)
        sys.exit(1)
