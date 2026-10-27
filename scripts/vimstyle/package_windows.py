# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Package a native Windows build; requires pefile on PYTHONPATH."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import pefile

sdk, source, build, dist = map(lambda p: Path(p).resolve(), sys.argv[1:5])
release = sys.argv[5] if len(sys.argv) > 5 else '0.1.0-preview.2'
dist.mkdir(parents=True, exist_ok=True)
shutil.copy2(build / 'texstudio.exe', dist)
env = dict(os.environ, PATH=str(sdk / 'bin') + os.pathsep + os.environ['PATH'])
deploy = sdk / 'bin/windeployqt6.exe'
subprocess.run([str(deploy), '--release', '--no-qml', '--qmlcore',
                '--no-translations', '--dir', str(dist), str(dist / 'texstudio.exe')],
               env=env, check=True)

# Resolve imports recursively, including Qt plugins and Poppler's dependencies.
pending = list(dist.rglob('*.dll')) + [dist / 'texstudio.exe']
visited, missing = set(), set()
system = Path(os.environ['SystemRoot']) / 'System32'
while pending:
    path = pending.pop()
    if path in visited:
        continue
    visited.add(path)
    pe = pefile.PE(str(path), fast_load=True)
    pe.parse_data_directories(directories=[1, 13])
    imports = list(getattr(pe, 'DIRECTORY_ENTRY_IMPORT', []))
    imports += list(getattr(pe, 'DIRECTORY_ENTRY_DELAY_IMPORT', []))
    for entry in imports:
        name = entry.dll.decode('ascii')
        if name.lower().startswith(('api-ms-', 'ext-ms-')):
            continue
        local = dist / name
        library = sdk / 'bin' / name
        if local.exists():
            pending.append(local)
        elif library.exists():
            shutil.copy2(library, local)
            pending.append(local)
        elif not (system / name).exists() and not (path.parent / name).exists():
            missing.add(name)
    pe.close()
if missing:
    raise RuntimeError('Unresolved DLLs: ' + ', '.join(sorted(missing)))

for origin, target in [
    (source / 'templates', dist / 'templates'),
    (source / 'utilities/dictionaries', dist / 'dictionaries'),
    (source / 'utilities/fonts', dist / 'share/fonts'),
    (source / 'utilities/manual/build', dist / 'help/build'),
    (sdk / 'share/poppler', dist / 'share/poppler'),
    (sdk / 'share/licenses', dist / 'licenses'),
]:
    if origin.exists():
        shutil.copytree(origin, target, dirs_exist_ok=True)
translations = dist / 'translations'
translations.mkdir(exist_ok=True)
for directory, pattern in [(source / 'translation', 'texstudio*.qm'),
                           (sdk / 'share/qt6/translations', 'qt_*.qm'),
                           (sdk / 'share/qt6/translations', 'qtbase_*.qm')]:
    for path in directory.glob(pattern):
        shutil.copy2(path, translations)
for name in ['latex2e.html', 'latex2e.css', 'list.png']:
    shutil.copy2(source / 'utilities' / name, dist / 'help' / name)
shutil.copy2(source / 'COPYING', dist)
shutil.copy2(source / 'VIM_MODE.md', dist)
for name in ['README.md', 'CHANGELOG.md', 'UPSTREAM_README.md']:
    shutil.copy2(source / name, dist)
shutil.copytree(source / 'docs/vimstyle', dist / 'docs', dirs_exist_ok=True)
(dist / 'SOURCE_CODE.txt').write_text(
    f'VimStyleTexStudio {release} (unofficial TeXstudio build)\n'
    'Matching application source and dependency sources:\n'
    f'https://github.com/rodioniurev/VimStyleTexStudio/releases/tag/v{release}\n'
    'Build and packaging instructions are included in the source archive.\n'
    'See COPYING and licenses/ for license notices.\n', encoding='utf-8')
(dist / 'config').mkdir(exist_ok=True)
(dist / 'config/texstudio.ini').write_text('[texmaker]\nEditor\\VimMode=true\n', encoding='utf-8')
(dist / 'qt.conf').write_text('[Paths]\nPrefix=.\nPlugins=.\nTranslations=translations\n', encoding='utf-8')
print(f'Packaged {len(visited)} executable/DLL files; no unresolved imports.')
