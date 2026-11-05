# MintVPN dependency notices

This directory contains identified upstream licence texts, notices and source references. The exact components and applicable notices depend on the build.

MintVPN is based on [Throne](https://github.com/throneproj/Throne). Retain the upstream attribution, copyright notices and GPL licence, and provide the source and build materials corresponding to distributed binaries.

## Sources and version references

- **Cronet DLL:** [SagerNet cronet-go v150.0.7871.63-1 release](https://github.com/SagerNet/cronet-go/releases/tag/v150.0.7871.63-1), [source](https://github.com/SagerNet/cronet-go/tree/v150.0.7871.63-1), [build workflow](https://github.com/SagerNet/cronet-go/blob/v150.0.7871.63-1/.github/workflows/naive-build.yml). The official `libcronet-windows-amd64.dll` referenced here has SHA-256 `257f966119ffca91d7a2ce110a4b668b865d88bf2ed5339fd06a5644b0d02823`. Check the DLL actually shipped against that hash.
- **Naiveproxy:** [pinned source commit 510717a833c95a17218efc550fe6cac02414cad5](https://github.com/SagerNet/naiveproxy/tree/510717a833c95a17218efc550fe6cac02414cad5).
- **Chromium:** [150.0.7871.63 source tag](https://chromium.googlesource.com/chromium/src/+/refs/tags/150.0.7871.63). The included Chromium and Naiveproxy root licences do not enumerate every third-party component compiled into the DLL.
- **OpenSSL 3.5.9:** [source release](https://github.com/openssl/openssl/releases/tag/openssl-3.5.9); `openssl-3.5.9.tar.gz` SHA-256 `603f5602e2eef00d77fbd429d34dcd5822bb301757a1bc9cdb24c670f1eb859a`. The OpenSSL licence is in `OpenSSL/`.
- **Qt 6.11.1:** [official source archive](https://download.qt.io/archive/qt/6.11/6.11.1/single/qt-everywhere-src-6.11.1.tar.xz), SHA-256 `252acef8c5ae68074d91cadba2ee4a83465051bbb970dd26e8f0daa0f3904e03` ([official metadata](https://download.qt.io/archive/qt/6.11/6.11.1/single/qt-everywhere-src-6.11.1.tar.xz.mirrorlist)). Licence texts under `Qt/` come from upstream v6.11.1 module repositories; SPDX files are installed toolkit metadata. The local Windows GUI uses a static Qt build; the configuration is described in [BUILD_WINDOWS.md](../../BUILD_WINDOWS.md). See [Qt licensing](https://doc.qt.io/qt-6/licensing.html).
- **Go components:** `Go-modules/manifest.json` maps collected root notices to module versions. The 138 module versions and replacements reported by `go version -m` for the local core were matched to the collected root notices; that comparison does not establish all embedded subcomponent notices. `Go-toolchain/` holds Go's root licence and patent notice.
- **Other GUI components:** `GUI-third-party/` contains the available QHotkey, simple-protobuf and SQLiteCpp root licence texts. Determine the complete set of shipped components and their attribution and notice requirements from the release source and build.

For Cronet, the complete final link graph and an independently matched build checkout have not been established. The presence of a licence text in this directory does not establish that the corresponding component is shipped or that the notices cover all bundled code.
