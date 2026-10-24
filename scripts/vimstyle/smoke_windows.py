# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 rodioniurev
"""Verify a portable build without exposing the SDK on PATH."""
from pathlib import Path
import os
import subprocess
import sys

dist = Path(sys.argv[1]).resolve()
env = dict(os.environ)
for key in ['QT_PLUGIN_PATH', 'QT_QPA_PLATFORM_PLUGIN_PATH', 'QT_QPA_PLATFORM',
            'QML2_IMPORT_PATH', 'QML_IMPORT_PATH']:
    env.pop(key, None)
windows = os.environ['SystemRoot']
env['PATH'] = os.pathsep.join([str(Path(windows) / 'System32'), windows])
for option in ['--version', '--help']:
    result = subprocess.run([str(dist / 'texstudio.exe'), option], cwd=dist, env=env,
                            capture_output=True, text=True, timeout=30,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    print(option, 'exit:', result.returncode)
    print(result.stdout)
    if result.stderr:
        print(result.stderr)
    if result.returncode:
        raise SystemExit(result.returncode)
print('Portable startup passed with system-only PATH and the native Windows Qt platform.')
