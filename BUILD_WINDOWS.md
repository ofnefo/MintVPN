# Building on Windows x64

These instructions build local Windows x64 artifacts from the source tree. A clean-machine build and bit-for-bit reproducibility have not been verified.

## Prerequisites

- Windows x64 and Visual Studio 2022 C++ Build Tools, including the x64 compiler and Windows SDK.
- CMake 3.20 or newer, Ninja, and a compatible Qt 6 build with Widgets, Network and LinguistTools. The local Windows GUI build used static Qt 6.11.1 and MSVC 14.44.
- A verified OpenSSL 3.5.9 development build, matching the Qt configuration.
- Python 3.11 or newer. The routing-catalogue fetch requires internet access.
- Go 1.26; the existing local core build used Go 1.26.5. The first Go build needs the dependencies specified by `core/server/go.mod` and `go.sum` to be available through your configured Go module proxy or cache.
- For the optional installer, NSIS and a separately obtained, verified Windows x64 Cronet DLL. Dependency notices and source references are under `3rdparty/mintvpn-notices`.

The locally observed build used Visual Studio 2022 Developer Command Prompt 17.14.41, CMake 3.31.6-msvc6 and Ninja 1.11.1. These versions identify that build environment; they are not a guarantee that the recipe below reproduces the shipped binary.

## Qt and OpenSSL configuration

The local GUI build used OpenSSL 3.5.9 configured for `VC-WIN64A` with `no-shared`, and Qt 6.11.1 configured with `-static -static-runtime -openssl-linked -no-schannel` and modules `qtbase,qtsvg,qttools,qttranslations`. The Qt configuration sets `OPENSSL_USE_STATIC_LIBS=ON` and `OPENSSL_MSVC_STATIC_RT=ON`. Match the source and notices to the binaries you distribute.

## Build the GUI and core

Open an **x64 Native Tools Command Prompt for VS 2022** in the repository root. Make `cmake`, `ninja`, `python` and `go` available on `PATH`. Stop and inspect the error if any command fails. Set the following paths to your own installed development packages:

```bat
set "QT_ROOT=C:\path\to\your\qt"
set "OPENSSL_ROOT=C:\path\to\your\openssl"
```

Fetch the pinned routing catalogue and verify its SHA-256, then configure and build the GUI:

```bat
python script\fetch_srslist.py build-public
cmake -S . -B build-public -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="%QT_ROOT%\lib\cmake" -DOPENSSL_ROOT_DIR="%OPENSSL_ROOT%" -DINPUT_VERSION=1.0.0
cmake --build build-public --parallel 2
```

The expected GUI output is `build-public\Quattro.exe`. The `fetch_srslist.py` script checks a pinned digest and refuses a different file already present in the build directory. An offline catalogue workflow is not currently documented.

Build the core and updater using the tags and flags from the local Windows x64 build:

```bat
if not exist build-public\package mkdir build-public\package
set "CGO_ENABLED=0"
pushd core\server
go build -p 1 -mod=readonly -trimpath -ldflags "-w -s" -o "..\..\build-public\package\QuattroUpdater.exe" ./cmd/quattro-updater
go build -p 1 -mod=readonly -trimpath -ldflags "-w -s -X github.com/sagernet/sing-box/constant.Version=v1.13.16 -X internal/godebug.defaultGODEBUG=multipathtcp=0 -checklinkname=0" -tags "with_clash_api,with_gvisor,with_quic,with_wireguard,with_utls,with_dhcp,with_tailscale,with_openvpn,with_openconnect,badlinkname,tfogo_checklinkname0,with_purego,with_naive_outbound" -o "..\..\build-public\package\MintVPNCore.exe" .
popd
```

The checked-in Go protobuf files were generated with `protoc` 6.31.1, `protoc-gen-go` 1.36.11 and `protoc-gen-go-grpc` 1.6.2 according to their headers. The commands above use those checked-in files and do not regenerate them. If changing `libcore.proto`, install compatible generators and verify the resulting changes separately.

## Optional installer staging

The installer script expects files under `deployment\windows-amd64`, which is not included in the source tree. After building, stage the binaries and independently verify the Cronet DLL's provenance and digest. The following placeholders must refer to your own DLL and its independently verified SHA-256:

```bat
if not exist deployment\windows-amd64 mkdir deployment\windows-amd64
copy /Y build-public\Quattro.exe deployment\windows-amd64\MintVPN.exe
copy /Y build-public\package\MintVPNCore.exe deployment\windows-amd64\MintVPNCore.exe
copy /Y build-public\package\QuattroUpdater.exe deployment\windows-amd64\QuattroUpdater.exe
set "CRONET_DLL_PATH=C:\path\to\verified\libcronet.dll"
set "CRONET_DLL_SHA256=replace_with_verified_64_character_sha256"
python script\stage_cronet.py deployment\windows-amd64 && python script\verify_installer_cronet.py x64 && makensis /NOCD /DQUATTRO_LOCAL_X64 script\windows_installer.nsi
```

Run both Python commands successfully before compiling; digest equality does not establish the DLL's origin. `/NOCD` keeps the repository root as the working directory so the installer script can resolve its inputs. In the local build environment NSIS `!system` failed even on a trivial command, so the x64 verification is run explicitly before compilation. The installer is produced in the repository root as `MintVPNSetup.exe`. A clean-machine build has not been verified.
