# Building and packaging

The application source is based on upstream TeXstudio development version 4.9.10.
The initial preview uses the same executable validated by the regression suite.
The fork's release number is separate from the upstream application version.

## Windows x64, MSYS2 UCRT64, Qt 6

Install MSYS2 from its official distribution. In its UCRT64 shell install:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja \
  mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-qt6-base \
  mingw-w64-ucrt-x86_64-qt6-5compat \
  mingw-w64-ucrt-x86_64-qt6-declarative mingw-w64-ucrt-x86_64-qt6-tools \
  mingw-w64-ucrt-x86_64-qt6-svg mingw-w64-ucrt-x86_64-qt6-translations \
  mingw-w64-ucrt-x86_64-hunspell mingw-w64-ucrt-x86_64-quazip-qt6 \
  mingw-w64-ucrt-x86_64-poppler-qt6
```

The release's `windows-packages.lock.json` records exact versions and binary
package URLs used locally. A current MSYS2 installation may use newer versions;
use the lock and the dependency sources when reconstructing the release SDK.
The validated SDK used Qt 6.12.0, GCC 16.2.0 UCRT and Poppler 26.08.0.

From PowerShell in the source root:

```powershell
./scripts/vimstyle/build_windows.ps1 -Sdk C:/msys64/ucrt64
python scripts/vimstyle/test_windows.py C:/msys64/ucrt64 . build-windows
python -m pip install pefile
python scripts/vimstyle/package_windows.py C:/msys64/ucrt64 . build-windows dist/windows-x64
python scripts/vimstyle/smoke_windows.py dist/windows-x64
python scripts/vimstyle/zip_package.py dist/windows-x64
```

Use a native Windows Python interpreter. `build_windows.ps1` retains Ninja
response files for the standalone tests. The test runner compiles QtTest wrappers
against the Release application objects and links QtTest without changing the
distributed executable. It requires Qt 6 Test headers and library in the SDK.
Build logs can be redirected by the caller.

The package contains Qt plugins, recursively resolved runtime DLLs, dictionary
data, templates, translations and dependency licenses. It creates a fresh Vim
profile and never copies personal settings. The optional legacy TexTablet tool
is not bundled. A TeX distribution is a separate user installation.

## Linux, Qt 5

On Ubuntu 22.04 install CMake, Ninja, a C++20 compiler, pkg-config,
`qtbase5-dev`, `qtdeclarative5-dev`, `qttools5-dev`, `libqt5svg5-dev`,
`libhunspell-dev`, `libquazip5-dev`, `libpoppler-qt5-dev`, `zlib1g-dev` and
`libxkbcommon-dev`. Then:

```sh
cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DQT_VERSION_MAJOR=5 -DTEXSTUDIO_ENABLE_TESTS=ON \
  -DTEXSTUDIO_BUILD_ADWAITA=OFF -DCMAKE_DISABLE_FIND_PACKAGE_GitInfo=ON
cmake --build build-linux -- -j4
python3 scripts/vimstyle/test_linux.py build-linux .
```

The Linux standalone runner uses GCC-compatible `objcopy` and Ninja. It reuses
the Debug application's test objects and runs only the Vim suite offscreen.

## Source and dependency delivery

The application source ZIP is a `git archive` of the release tag. It includes
all application sources, original notices and build/package/test scripts.
Separate dependency-source archives contain the exact MSYS2 `.src.tar.zst`
packages for the DLLs shipped in the Windows ZIP: original source tarballs,
distribution patches and PKGBUILD recipes. Their manifest records URLs, versions
and SHA256 digests. Rebuild a dependency with MSYS2's `makepkg-mingw` following
its PKGBUILD, then build TeXstudio against the resulting UCRT64 prefix.

To collect runtime sources from an SDK with its pacman database:

```powershell
python scripts/vimstyle/collect_dependency_sources.py C:/msys64 dist/windows-x64 dist/dependency-sources
```

For network distribution, keep matching application and dependency sources
available alongside the binary release and retain the license notices. Do not
publish only a patch or a binary without its corresponding source access.
