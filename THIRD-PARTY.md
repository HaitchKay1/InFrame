# Third-party components

- InFrame source: GPL-3.0-or-later, LICENSE.
- OBS Studio API and runtime: GPL-2.0-or-later. Plugin links libobs, using the GPLv3-or-later distribution option for compatibility with Apache-2.0 components. OBS itself is not redistributed in the Windows plugin package.
- OBS official plugin template CMake support: MIT, LICENSE-OBS-TEMPLATE. Template commit `3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc`; its OBS baseline is 31.1.1 and prebuilt dependency revision is 2025-07-11.
- OpenCV 4.12.0: Apache-2.0. Windows builds link selected static OpenCV libraries; build script copies its LICENSE into the resulting package.
- OpenCV's protobuf, flatbuffers, and zlib dependencies: their individual licenses are copied from the pinned OpenCV source into third-party-licenses during Windows packaging.
- YuNet 2023mar ONNX model: MIT, data/models/LICENSE-YuNet. Included locally; runtime needs no download.

No affiliation with OBS, NVIDIA, OpenCV, or StreamFX is implied. No StreamFX code, proprietary NVIDIA model, or NVIDIA SDK binaries are used.

Bundled model SHA-256: `8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4`.
