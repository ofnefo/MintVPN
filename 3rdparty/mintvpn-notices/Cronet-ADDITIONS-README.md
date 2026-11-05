# Cronet third-party notice additions — partial

This directory contains additional unmodified license texts for components identified from the pinned upstream build descriptions. These notices are a partial supplement, not a complete dependency graph, corresponding-source bundle, or license-clearance conclusion.

The existing evidence identifies the distributed Windows DLL SHA-256 `257f966119ffca91d7a2ce110a4b668b865d88bf2ed5339fd06a5644b0d02823` with the upstream SagerNet cronet-go v150.0.7871.63-1 release asset. At that release, the Windows workflow calls `build-naive --target=windows/amd64 build`; build-naive runs `ninja ... cronet`. Pinned Naiveproxy revision is `510717a833c95a17218efc550fe6cac02414cad5`; its `components/cronet/BUILD.gn` defines the Windows DLL `cronet` and depends on `//net`. The pinned `net/BUILD.gn` connects `net` to `net_deps` and `net_public_deps`, which reference the components below. GN dependencies do not on their own prove the exact contents of the binary after compilation and dead stripping. The actual release workflow execution, cache state, and binary link map were not independently established.

| Addition | Pinned source of license | Evidence |
| --- | --- | --- |
| Cronet-BoringSSL | https://github.com/google/boringssl/blob/3a9254f16eda7a4c5d2260039ff23456a0a34de4/LICENSE | Naiveproxy DEPS lines 350, 2045–2046; net_public_deps -> //third_party/boringssl |
| Cronet-QUICHE | https://github.com/google/quiche/blob/997d654308b6a1a17435e472ef5190aecb12e3eb/LICENSE | DEPS lines 438, 1711–1712; net -> //net/third_party/quiche |
| Cronet-zstd | https://github.com/facebook/zstd/blob/5233c58e6ca0b1c4c6b353ad79649191ed195bdc/LICENSE | DEPS lines 3133–3134; net_deps conditionally adds zstd; build explicitly sets disable_zstd_filter=false |
| Cronet-ICU | https://chromium.googlesource.com/chromium/deps/icu/+/3859e64eed5d34544b27fbcab0ac1685ce83df3c/LICENSE | DEPS lines 2302–2303; net_deps conditionally adds ICU |
| Cronet-zlib | https://github.com/SagerNet/naiveproxy/blob/510717a833c95a17218efc550fe6cac02414cad5/src/third_party/zlib/LICENSE | net_deps -> //third_party/zlib |
| Cronet-brotli | https://github.com/SagerNet/naiveproxy/blob/510717a833c95a17218efc550fe6cac02414cad5/src/third_party/brotli/LICENSE | net_deps conditionally adds decoder or headers |
| Cronet-simdutf | https://github.com/SagerNet/naiveproxy/blob/510717a833c95a17218efc550fe6cac02414cad5/src/third_party/simdutf/LICENSE | net -> //third_party/simdutf |
| Cronet-protobuf | https://github.com/chromium/chromium/blob/150.0.7871.63/third_party/protobuf/LICENSE | net_deps -> protobuf_lite; identical SHA-256 for both `third_party/protobuf/BUILD.gn` and `src/google/protobuf/message_lite.cc` between pinned Naiveproxy and Chromium 150.0.7871.63. This is two-file evidence, not proof of entire tree identity. |

Source metadata: https://github.com/SagerNet/cronet-go/blob/v150.0.7871.63-1/.github/workflows/naive-build.yml ; https://github.com/SagerNet/cronet-go/blob/v150.0.7871.63-1/cmd/build-naive/cmd_build.go ; https://github.com/SagerNet/naiveproxy/blob/510717a833c95a17218efc550fe6cac02414cad5/src/DEPS ; https://github.com/SagerNet/naiveproxy/blob/510717a833c95a17218efc550fe6cac02414cad5/src/net/BUILD.gn ; https://github.com/SagerNet/naiveproxy/blob/510717a833c95a17218efc550fe6cac02414cad5/src/components/cronet/BUILD.gn .

Open: additional transitive components and individual license notices, exact release build graph/link map and any modified sources, Chromium/Naiveproxy and third-party corresponding sources, Qt source and static linking/relink materials, and exact provenance/build recipe for OpenSSL. Review the actual distribution and applicable license requirements before representing the package as complete.
