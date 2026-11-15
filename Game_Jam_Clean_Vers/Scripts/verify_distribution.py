"""Read-only checkout audit against a Git commit, including hydrated LFS bytes.

python Game_Jam_Clean_Vers/Scripts/verify_distribution.py --report <outside-repo.json>
Run git fetch origin first to refresh the remote reference. Exit 1 means differences
exist, not necessarily corruption. No files, Git flags or configuration are repaired.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ref', default='HEAD')
    parser.add_argument('--report', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]

    def git(*arguments):
        return subprocess.check_output(['git', '-C', str(root), *arguments])

    commit = git('rev-parse', '--verify', args.ref + '^{commit}').decode().strip()
    entries = []
    for record in git('ls-tree', '-rlz', commit).split(b'\0'):
        if not record:
            continue
        metadata, path = record.split(b'\t', 1)
        mode, kind, oid, size = metadata.split()
        if kind != b'blob':
            raise RuntimeError('Unsupported tree entry: ' + repr(record))
        entries.append((os.fsdecode(path), oid.decode(), int(size)))
    # Only small Git blobs can be LFS pointers; batch-read these without filters.
    small = {oid for _, oid, size in entries if size <= 1024}
    output = subprocess.check_output(['git', '-C', str(root), 'cat-file', '--batch'],
                                     input=('\n'.join(sorted(small)) + '\n').encode())
    pointers = {}
    offset = 0
    while offset < len(output):
        end = output.index(b'\n', offset)
        oid, _, size = output[offset:end].split()
        size = int(size)
        data = output[end + 1:end + 1 + size]
        offset = end + 2 + size
        if data.startswith(b'version https://git-lfs.github.com/spec/v1\n'):
            digest = re.search(rb'^oid sha256:([0-9a-f]{64})$', data, re.M)
            length = re.search(rb'^size ([0-9]+)$', data, re.M)
            if not digest or not length:
                raise RuntimeError('Invalid LFS pointer in commit: ' + oid.decode())
            pointers[oid.decode()] = (digest[1].decode(), int(length[1]))

    differences, line_endings, lfs_count = [], [], 0
    file_stamps = {}
    text_extensions = {'.ini', '.cs', '.cpp', '.h', '.inl', '.py', '.ps1', '.cmd',
                       '.bat', '.json', '.uproject', '.uplugin', '.md', '.txt', '.vsconfig'}
    for name, oid, size in entries:
        path = root / name
        if not path.is_file():
            differences.append({'path': name, 'reason': 'missing'})
            continue
        is_lfs = oid in pointers
        lfs_count += is_lfs
        expected, expected_size = pointers[oid] if is_lfs else (oid, size)
        actual_size = path.stat().st_size
        file_stamps[name] = (actual_size, path.stat().st_mtime_ns)
        digest = hashlib.sha256() if is_lfs else hashlib.sha1()
        if not is_lfs:
            digest.update(f'blob {actual_size}\0'.encode())
        with path.open('rb') as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b''):
                digest.update(chunk)
        if digest.hexdigest() == expected and actual_size == expected_size:
            continue
        # Record CRLF checkout conversion separately; do not hide content edits.
        if not is_lfs and actual_size < 4 * 1024 * 1024 and (
                path.suffix.lower() in text_extensions or path.name in {'.gitignore', '.gitattributes', '.vsconfig'}):
            normalized = path.read_bytes().replace(b'\r\n', b'\n')
            normalized_oid = hashlib.sha1(f'blob {len(normalized)}\0'.encode() + normalized).hexdigest()
            if normalized_oid == oid:
                line_endings.append(name)
                continue
        differences.append({'path': name, 'reason': 'LFS bytes differ' if is_lfs else 'Git blob differs',
                            'expected': expected, 'actual': digest.hexdigest(),
                            'expected_size': expected_size, 'actual_size': actual_size})
    flags = [os.fsdecode(x) for x in git('ls-files', '-v', '-z').split(b'\0')
             if x and (x[:1].islower() or x.startswith(b'S '))]
    untracked = [os.fsdecode(x) for x in git('ls-files', '--others', '--exclude-standard', '-z').split(b'\0') if x]
    ignored = [os.fsdecode(x) for x in git('ls-files', '--others', '--ignored', '--exclude-standard', '-z').split(b'\0') if x]
    # Generated caches are expected; ignored distributable content is not.
    generated = ('Game_Jam_Clean_Vers/Binaries/', 'Game_Jam_Clean_Vers/Intermediate/',
                 'Game_Jam_Clean_Vers/Saved/', 'Game_Jam_Clean_Vers/DerivedDataCache/')
    ignored_project = [x for x in ignored if not x.startswith(generated)
                       and '/.vs/' not in '/' + x and Path(x).suffix.lower() not in {'.sln', '.slnx', '.suo'}]
    head = git('rev-parse', 'HEAD').decode().strip()
    branch = git('branch', '--show-current').decode().strip()
    changed_during_audit = []
    for name, stamp in file_stamps.items():
        path = root / name
        if not path.is_file() or (path.stat().st_size, path.stat().st_mtime_ns) != stamp:
            changed_during_audit.append(name)
    report = {'commit': commit, 'local_head': head, 'branch': branch,
              'origin': git('remote', 'get-url', 'origin').decode().strip(),
              'tracked_files': len(entries), 'lfs_files_verified': lfs_count,
              'differences': differences, 'line_endings_only': line_endings,
              'hidden_git_flags': flags, 'untracked': untracked,
              'changed_during_audit': changed_during_audit,
              'ignored_project_files': ignored_project, 'ignored_file_count': len(ignored),
              'scope': 'Working bytes versus selected commit; not a remote LFS availability, dependency, build or gameplay test.'}
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k not in {'line_endings_only', 'untracked', 'ignored_project_files'}}, ensure_ascii=False, indent=2))
    print(f'CRLF-only: {len(line_endings)}; untracked: {len(untracked)}; ignored project files: {len(ignored_project)}')
    return 1 if differences or untracked or ignored_project or changed_during_audit or head != commit or branch != 'main' else 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f'Audit failed: {exc}', file=sys.stderr)
        sys.exit(2)
