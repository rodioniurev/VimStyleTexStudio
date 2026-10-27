# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
from pathlib import Path
import hashlib
import sys
import zipfile

dist = Path(sys.argv[1]).resolve()
archive = dist.parent / (dist.name + '.zip')
with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as output:
    for file in sorted(dist.rglob('*')):
        if file.is_file():
            output.write(file, file.relative_to(dist.parent))
with zipfile.ZipFile(archive) as output:
    damaged = output.testzip()
    if damaged:
        raise RuntimeError('Corrupt ZIP member: ' + damaged)
digest = hashlib.file_digest(archive.open('rb'), 'sha256').hexdigest()
archive.with_suffix('.zip.sha256').write_text(digest + '  ' + archive.name + '\n')
print(archive)
print(f'{archive.stat().st_size / 1024 / 1024:.1f} MiB')
print('SHA256:', digest)
