"""Archive the prepared source and initialized submodules, without SDK files."""
import argparse
import hashlib
import subprocess
import tarfile
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
source = a.source.resolve()
project = Path(__file__).resolve().parent.parent
# Git enumerates exact pinned sources, including recursively checked-out modules.
tracked = subprocess.check_output(
    ['git', '-C', str(source), 'ls-files', '--recurse-submodules', '-z'])
files = {Path(n.decode()) for n in tracked.split(b'\0') if n}
files.update(Path('xenia-canary-uwp') / n for n in (
    'riffinput.cpp', 'riffinput.def', 'riffmaster_mapping.h',
    'riffmaster_full_mapping.h', 'riffmaster_slots.h', 'riffmaster_sensitivity.h',
    'guitar_ui_release.h', 'party_launch.h', 'party_frontend.inc',
    'console_accounts.h', 'console_accounts.cc', 'console_profiles.h',
    'console_profile_storage.h', 'console_profile_migration.h',
    'console_title_account_guard.inc', 'audio_diagnostics.h',
    'calibration_restore.h', 'calibration_restore_storage.h'))
with tarfile.open(a.output, 'w:gz') as archive:
    for relative in sorted(files):
        if relative.name == 'GameInput.h' or '.git' in relative.parts:
            continue
        path = source / relative
        if path.is_file():
            archive.add(path, arcname=str(Path('source') / relative), recursive=False)
    for path in sorted((project / 'xenia-patches').glob('*')):
        if path.is_file():
            archive.add(path, arcname=str(Path('build-project/xenia-patches') / path.name))
    for name in ('LICENSE', 'THIRD_PARTY_NOTICES.md', 'README.md'):
        archive.add(project / name, arcname='build-project/' + name)
print(f'Corresponding source: {len(files)} files; {hashlib.sha256(a.output.read_bytes()).hexdigest()}')
