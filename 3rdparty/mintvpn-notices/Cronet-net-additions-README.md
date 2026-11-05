# Additional Cronet notices and source excerpts

These are supplementary attributions for the Windows amd64 Cronet release asset with SHA-256 257f966119ffca91d7a2ce110a4b668b865d88bf2ed5339fd06a5644b0d02823. They do not constitute a full inventory, complete corresponding sources, or a determination that redistribution obligations are satisfied. The upstream build and release jobs indicate a cache handoff, but the release artifact digest has not independently been compared with the cached build artifact.

The source files in Cronet-uri-template and Cronet-mozilla-win are unmodified copies from the SagerNet Naiveproxy snapshot 510717a833c95a17218efc550fe6cac02414cad5; these are the adapted Chromium versions, not vanilla upstream Mozilla/Google sources. Metadata and LICENSE files come from the same snapshot.

- URI Template: [component source and metadata](https://github.com/SagerNet/naiveproxy/tree/510717a833c95a17218efc550fe6cac02414cad5/src/net/third_party/uri_template). net/BUILD.gn lists //net/third_party/uri_template as a public dependency of net_public_deps. Its README.chromium identifies the license as Apache-2.0, states it is shipped, and describes local adaptations.
- Mozilla Windows certificate utility: [component source and metadata](https://github.com/SagerNet/naiveproxy/tree/510717a833c95a17218efc550fe6cac02414cad5/src/net/third_party/mozilla_win). Its README.chromium identifies MPL-2.0 and local adaptations. net/BUILD.gn includes cert/win_util.cc and .h when is_win and chrome_root_store_supported; net/features.gni defaults chrome_root_store_only to true for Windows and chrome_root_store_supported to chrome_root_store_optional || chrome_root_store_only. This establishes a conditional build-input inference, not a final link map or independently verified exact CI checkout.
- Compiler-rt: the included atomic.c is the modified copy in the pinned Naiveproxy snapshot; see its component provenance for the separate pinned upstream revision and observed build action.
- Perfetto, modp_b64, and NASM are supported by observed upstream build paths. NASM is recorded as a build tool, not as shipped DLL code.

The exact resolved GN graph, final link map, generated artifacts, all transitive notices and complete corresponding source materials remain to be verified. Consult the existing Cronet release evidence before distributing the DLL.
