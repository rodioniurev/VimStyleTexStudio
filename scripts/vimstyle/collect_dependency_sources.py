# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Collect exact MSYS2 runtime source packages and a build dependency lock file.

Arguments: MSYS2 root, portable package directory, output directory.
Sources contain upstream tarballs, PKGBUILD recipes and distribution patches.
"""
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import sys
import time
import urllib.request

root, package, output = [Path(p).resolve() for p in sys.argv[1:4]]
output.mkdir(parents=True, exist_ok=True)
packages, owners = [], {}
for entry in sorted((root / 'var/lib/pacman/local').iterdir()):
    desc = entry / 'desc'
    if not desc.exists():
        continue
    fields = {}
    for block in desc.read_text(encoding='utf-8').split('\n\n'):
        lines = block.splitlines()
        if lines and lines[0].startswith('%'):
            fields[lines[0].strip('%')] = lines[1:]
    value = {key.lower(): fields.get(key, []) for key in ['NAME', 'VERSION', 'BASE', 'ARCH', 'URL', 'LICENSE']}
    value = {key: data[0] if len(data) == 1 else data for key, data in value.items()}
    value['binary_url'] = 'https://repo.msys2.org/mingw/ucrt64/' + value['name'] + '-' + value['version'].split(':')[-1] + '-any.pkg.tar.zst'
    packages.append(value)
    for path in (entry / 'files').read_text(encoding='utf-8').splitlines():
        if path.endswith(('.dll', '.exe')):
            owners.setdefault(Path(path).name.lower(), []).append(value)

runtime = {}
for owner in packages:
    if owner['name'].endswith(('-qt6-translations', '-poppler-data', '-ca-certificates')):
        key = owner['base'] + '-' + owner['version'].split(':')[-1]
        runtime[key] = owner
for binary in sorted(package.rglob('*')):
    if binary.suffix.lower() not in ['.dll', '.exe'] or binary.name == 'texstudio.exe':
        continue
    matches = owners.get(binary.name.lower(), [])
    if not matches:
        raise RuntimeError('No MSYS2 source owner for ' + binary.name)
    for owner in matches:
        key = owner['base'] + '-' + owner['version'].split(':')[-1]
        runtime[key] = owner

lock = {'repository': 'https://repo.msys2.org/mingw/ucrt64', 'packages': packages}
(output / 'windows-packages.lock.json').write_text(json.dumps(lock, indent=2) + '\n', encoding='utf-8')

def download(item):
    key, owner = item
    name = key + '.src.tar.zst'
    url = 'https://repo.msys2.org/mingw/sources/' + name
    target = output / name
    if not target.exists():
        partial = target.with_suffix('.partial')
        for attempt in range(3):
            try:
                with urllib.request.urlopen(url, timeout=120) as response, partial.open('wb') as stream:
                    while chunk := response.read(1024 * 1024):
                        stream.write(chunk)
                partial.replace(target)
                break
            except Exception:
                if attempt == 2:
                    raise
                time.sleep(2)
    digest = hashlib.file_digest(target.open('rb'), 'sha256').hexdigest()
    print(name + ': ' + str(target.stat().st_size) + ' bytes', flush=True)
    return {'file': name, 'url': url, 'sha256': digest, 'bytes': target.stat().st_size,
            'base': owner['base'], 'version': owner['version']}

with ThreadPoolExecutor(max_workers=4) as pool:
    sources = list(pool.map(download, sorted(runtime.items())))
(output / 'DEPENDENCY_SOURCES.json').write_text(json.dumps(sources, indent=2) + '\n', encoding='utf-8')
print('Collected source packages:', len(sources))
