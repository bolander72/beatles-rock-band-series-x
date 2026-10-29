"""Assemble the audited 0.6.0.9 install bundle; no game files. BSD-3-Clause."""
import argparse
import hashlib
import importlib.util
import json
import shutil
import zipfile
from pathlib import Path

PACKAGE = 'Beatles-Rock-Band-Launcher-0.6.0.9-x64.appx'
PACKAGE_SHA = 'f953ec7ebb70961c6d8ac10684505ff5706460a094367bb84a8cb4a353df7852'
SOURCE_SHA = 'df0738c85fe063c74ef414998769b1f96db3f383b4acbca98c986657af0ea07c'
ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def assemble(incoming, output):
    incoming, output = incoming.resolve(), output.resolve()
    if output.exists():
        raise ValueError('Choose a new output directory; existing release files are not overwritten.')
    assert sha(incoming / PACKAGE) == PACKAGE_SHA, 'Not the audited candidate APPX'
    assert sha(incoming / 'corresponding-source.tar.gz') == SOURCE_SHA, 'Not the audited corresponding source'
    for line in (incoming / 'SHA256SUMS.txt').read_text(encoding='utf-8-sig').splitlines():
        digest, name = line.split(maxsplit=1)
        assert sha(incoming / name.strip()) == digest.lower(), name
    spec = importlib.util.spec_from_file_location('verify_full', ROOT / 'xenia-patches/verify_full.py')
    verifier = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(verifier)
    checked = verifier.verify(incoming / PACKAGE, incoming / 'Microsoft.VCLibs.x64.14.00.appx', '0.6.0.9')
    output.mkdir(parents=True)
    folder = output / 'The-Beatles-Rock-Band-0.6.0.9'
    (folder / 'tools').mkdir(parents=True)
    for name in (PACKAGE, 'Microsoft.VCLibs.x64.14.00.appx', 'XeniaRiffmasterLab.cer',
                 'BUILD.txt', 'LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(incoming / name, folder / name)
    shutil.copy2(ROOT / 'docs/INSTALL.md', folder / 'INSTALL.md')
    shutil.copy2(ROOT / 'docs/COMPATIBILITY.md', folder / 'COMPATIBILITY.md')
    shutil.copy2(ROOT / 'docs/TROUBLESHOOTING.md', folder / 'TROUBLESHOOTING.md')
    shutil.copy2(ROOT / 'tools/prepare_game.py', folder / 'tools/prepare_game.py')
    (folder / 'START-HERE.txt').write_text(
        'The Beatles: Rock Band - community Developer Mode launcher\n\n'
        '1. Once, in Device Portal > Settings > Preference, enable Treat UWP apps as games by default and leave it on. Restart if prompted.\n'
        '2. Install the launcher APPX through Device Portal with the Microsoft x64 VCLibs dependency if needed. An existing App assignment may need one final Dev Home > View > Details > Game correction.\n'
        '3. Supply your own extracted Xbox 360 game files. See INSTALL.md for the Mac/browser transfer method.\n'
        '4. Connect the Xbox Riffmaster receiver, complete local player setup, and select Play with green/A.\n'
        'After setup, later cold launches open the game directly. See INSTALL.md for calibration and controller setup.\n\n'
        'PREVIEW: automated checks and app registration passed; console direct-launch/gameplay verification is pending.\n'
        'One Xbox Riffmaster is the verified underlying instrument setup. Other instruments/full-band play are unverified.\n'
        'No game executables, music, charts, DLC or firmware are included. Developer Mode is required.\n'
        'The large corresponding-source.tar.gz download is optional for inspecting/rebuilding the code. Players do not need it.\n'
        'Source and updates: https://github.com/bolander72/beatles-rock-band-series-x\n')
    (folder / 'MICROSOFT-RUNTIME-NOTICE.txt').write_text(
        'Microsoft.VCLibs.140.00, version 14.0.33519.0, x64\n'
        'Copyright Microsoft Corporation. Supplied unmodified from the Windows SDK.\n'
        'This Microsoft framework is governed by Microsoft software license terms, not the project BSD license.\n'
        'Windows SDK and terms: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/\n')
    record = {
        'version': '0.6.0.9', 'display_name': 'The Beatles: Rock Band',
        'build_commit': 'eb673d48afa22c4f5f666fc34b6945e9533eadb2',
        'build_run': 'https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/40938459908',
        'appx_sha256': PACKAGE_SHA, 'corresponding_source_sha256': SOURCE_SHA,
        'verified_package_blocks': checked['verified_uncompressed_blocks'],
        'runtime_imports_checked': checked['runtime_check']['verified_imported_functions'],
        'console_registration_verified': True, 'console_direct_launch_verified': False,
        'verification_scope': 'Packaging snapshot; see repository docs/VALIDATION.md for later console results.',
        'console_gameplay_verified_on_version': '0.6.0.7', 'developer_mode_required': True,
        'game_files_included': False, 'release_status': 'preview; awaiting console launcher checks',
    }
    (folder / 'VERIFICATION.json').write_text(json.dumps(record, indent=2) + '\n')
    files = [p for p in sorted(folder.rglob('*')) if p.is_file()]
    (folder / 'SHA256SUMS.txt').write_text(''.join(
        '%s  %s\n' % (sha(p), p.relative_to(folder).as_posix()) for p in files))
    bundle = output / 'The-Beatles-Rock-Band-0.6.0.9-x64.zip'
    with zipfile.ZipFile(bundle, 'x', compression=zipfile.ZIP_STORED) as archive:
        for path in sorted(folder.rglob('*')):
            if path.is_file():
                archive.write(path, path.relative_to(output).as_posix())
    with zipfile.ZipFile(bundle) as archive:
        assert archive.testzip() is None
        assert not any(Path(n).suffix.lower() in ('.iso', '.xex', '.ark', '.pfx', '.key') for n in archive.namelist())
    shutil.copy2(folder / 'VERIFICATION.json', output / 'VERIFICATION.json')
    shutil.copy2(ROOT / 'docs/INSTALL.md', output / 'INSTALL.md')
    # Do not duplicate the large source archive locally; the release workflow
    # uploads the exact audited incoming file alongside the small install ZIP.
    (output / 'SHA256SUMS.txt').write_text(
        '%s  %s\n%s  corresponding-source.tar.gz\n%s  VERIFICATION.json\n%s  INSTALL.md\n' %
        (sha(bundle), bundle.name, SOURCE_SHA, sha(output / 'VERIFICATION.json'), sha(output / 'INSTALL.md')))
    print(json.dumps({'bundle': bundle.name, 'bytes': bundle.stat().st_size,
                      'sha256': sha(bundle), 'package_blocks': checked['verified_uncompressed_blocks']}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--artifacts', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    assemble(args.artifacts, args.output)
