"""Assemble the audited 0.6.0.13 install bundle; no game files. BSD-3-Clause."""
import argparse
import hashlib
import importlib.util
import json
import shutil
import zipfile
from pathlib import Path

VERSION = '0.6.0.13'
PACKAGE = 'Beatles-Rock-Band-Launcher-0.6.0.13-x64.appx'
PACKAGE_SHA = 'b0a44932dec6218cee18608f0b05b6c93854e3d32fb36d1aa1d08b3d21fb9639'
SOURCE_SHA = '06162d73456d2224ce028f161738413207bfc982c50422a2f7dfd643cd624f01'
BUILD_COMMIT = 'fb634e558fd115c9657b3dcc13e2d30f57565028'
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
    assert BUILD_COMMIT in (incoming / 'BUILD.txt').read_text(encoding='utf-8-sig')
    checked = verifier.verify(incoming / PACKAGE, incoming / 'Microsoft.VCLibs.x64.14.00.appx', VERSION)
    output.mkdir(parents=True)
    folder = output / ('The-Beatles-Rock-Band-' + VERSION)
    (folder / 'tools').mkdir(parents=True)
    for name in (PACKAGE, 'Microsoft.VCLibs.x64.14.00.appx', 'XeniaRiffmasterLab.cer',
                 'BUILD.txt', 'LICENSE', 'THIRD_PARTY_NOTICES.md'):
        shutil.copy2(incoming / name, folder / name)
    shutil.copy2(ROOT / 'docs/INSTALL.md', folder / 'INSTALL.md')
    shutil.copy2(ROOT / 'docs/COMPATIBILITY.md', folder / 'COMPATIBILITY.md')
    shutil.copy2(ROOT / 'docs/TROUBLESHOOTING.md', folder / 'TROUBLESHOOTING.md')
    shutil.copy2(ROOT / 'docs/NATIVE-ACCOUNT-TESTING.md', folder / 'NATIVE-ACCOUNT-TESTING.md')
    for name in ('VALIDATION.md', 'AUDIO-DIAGNOSTICS.md'):
        shutil.copy2(ROOT / 'docs' / name, folder / name)
    shutil.copy2(ROOT / 'tools/prepare_game.py', folder / 'tools/prepare_game.py')
    (folder / 'START-HERE.txt').write_text(
        'The Beatles: Rock Band - community Developer Mode launcher\n\n'
        '1. Once, in Device Portal > Settings > Preference, enable Treat UWP apps as games by default and leave it on. Restart if prompted.\n'
        '2. Install the launcher APPX through Device Portal with the Microsoft x64 VCLibs dependency if needed. An existing App assignment may need one final Dev Home > View > Details > Game correction.\n'
        '3. Supply your own extracted Xbox 360 game files. See INSTALL.md for the Mac/browser transfer method.\n'
        '4. Connect the Xbox Riffmaster receiver, sign in to your console account and open The Beatles: Rock Band. If needed, select your account in the native Xbox picker.\n'
        'With a single valid game copy, startup opens the game directly. See INSTALL.md for calibration and controller setup.\n\n'
        'DEVELOPMENT PREVIEW: This exact build reached the game directly and completed a player-tested song with audio/video timing reported correct on one Xbox Riffmaster setup. Validation used restored personal calibration. Physical tile cold startup, story-save/reload and separate-account ownership still need verification. See VALIDATION.md.\n'
        'This update adds userAccountInformation permission to associate console accounts with persistent local saves. Saves remain offline; Xbox Live authentication and cloud saves are not provided. Back up existing app data before updating.\n'
        'One Xbox Riffmaster is the tested instrument setup. A second independent guitar and microphone capture are not implemented yet.\n'
        'No game executables, music, charts, DLC or firmware are included. Developer Mode is required.\n'
        'The large corresponding-source.tar.gz download is optional for inspecting/rebuilding the code. Players do not need it.\n'
        'Source and updates: https://github.com/bolander72/beatles-rock-band-series-x\n')
    (folder / 'MICROSOFT-RUNTIME-NOTICE.txt').write_text(
        'Microsoft.VCLibs.140.00, version 14.0.33519.0, x64\n'
        'Copyright Microsoft Corporation. Supplied unmodified from the Windows SDK.\n'
        'This Microsoft framework is governed by Microsoft software license terms, not the project BSD license.\n'
        'Windows SDK and terms: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/\n')
    record = {
        'version': VERSION, 'display_name': 'The Beatles: Rock Band',
        'build_commit': BUILD_COMMIT,
        'build_run': 'https://github.com/bolander72/beatles-rock-band-series-x/actions/runs/41090860641',
        'appx_sha256': PACKAGE_SHA, 'corresponding_source_sha256': SOURCE_SHA,
        'verified_package_blocks': checked['verified_uncompressed_blocks'],
        'runtime_imports_checked': checked['runtime_check']['verified_imported_functions'],
        'console_registration_verified': True, 'console_direct_launch_verified': True,
        'console_full_stop_launches_reached_intro': 0,
        'console_activation_method': 'Device Portal deployment/activation; physical song play confirmed by player',
        'physical_dev_home_tile_launch_verified': False,
        'local_profile_persists_across_app_restarts': False,
        'story_save_reload_verified': False, 'native_account_binding_implemented': True,
        'native_account_binding_console_verified': True,
        'native_account_binding_verified_scope': 'one existing signed-in console account associated with local slot 0; separate-account save ownership unverified',
        'legacy_save_migration_console_verified': False,
        'supported_users': 'multiple', 'account_information_permission_declared': True,
        'capabilities_unchanged_from_0_6_0_11': True,
        'xbox_live_authentication_implemented': False, 'cloud_saves_implemented': False,
        'independent_second_guitar_implemented': False, 'microphone_capture_implemented': False,
        'verification_scope': 'Packaging snapshot; see repository docs/VALIDATION.md for later console results.',
        'console_gameplay_verified_on_version': VERSION,
        'console_audio_video_sync_player_verified': True,
        'console_sync_verification_scope': "one completed song after restoring this player's earlier calibration; other TVs require calibration",
        'console_guest_clock_tracks_host_over_60_seconds': True,
        'personal_calibration_or_diagnostic_logs_included': False, 'developer_mode_required': True,
        'previous_direct_launch_evidence': {'version': '0.6.0.10', 'full_stop_portal_launches_reached_intro': 3},
        'game_files_included': False, 'release_status': 'draft preview; song timing verified on one setup; physical tile cold startup, story-save/reload and separate-account ownership remain unverified',
    }
    (folder / 'VERIFICATION.json').write_text(json.dumps(record, indent=2) + '\n')
    files = [p for p in sorted(folder.rglob('*')) if p.is_file()]
    (folder / 'SHA256SUMS.txt').write_text(''.join(
        '%s  %s\n' % (sha(p), p.relative_to(folder).as_posix()) for p in files))
    bundle = output / ('The-Beatles-Rock-Band-' + VERSION + '-x64.zip')
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
