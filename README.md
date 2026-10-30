# SteadyFrame

A free, open-source face-following effect filter for OBS Studio. Native C++ filter, YuNet face detection on a worker thread, GPU cropping through OBS, and plain-language filter controls.

**Status: 0.1.0 development alpha.** The source package is not an installable Windows plugin. A Windows build workflow and installer scripts are provided, but a Windows build and real-camera test have not been executed for this delivery. See VALIDATION.md for exactly what was tested. Do not interpret the included feature set as a performance or quality victory over StreamFX.

## Implemented

- OBS camera-source Effect Filter registration, no virtual webcam or separate camera capture.
- Balanced / Responsive / Cinematic starting presets; custom tuning.
- Adjustable target face size, automatic or fixed zoom, maximum zoom, and horizontal/vertical composition.
- Time-based pan/zoom smoothing, small-movement dead zone, and pan-speed cap.
- Spatial subject continuity, largest-face mode, and group bounding-box framing.
- Tracking-loss grace period, then gentle return to wide or indefinite hold.
- Freeze and reacquire controls, with source-scoped OBS hotkeys.
- Original-view detection while zoomed in, aspect-preserving bounded crops, stable output dimensions.
- Background inference with one replaceable pending frame; stale detections rejected.
- Downsampled GPU readback staged across frames, configurable analysis rate, readable status.
- Local processing only. No telemetry, network calls, frame recording, or face database.

## Why not NVIDIA first?

Current Maxine supports Blackwell, but requires its SDK runtime and separately distributed face-tracking components. This implementation instead uses OpenCV 4.12 + the small YuNet model on the CPU. NVIDIA, AMD, and Intel GPUs can all handle the crop through OBS; **the RTX tensor cores are not used**. Actual CPU cost needs measuring on the user's machine.

This reduces proprietary dependencies and makes the first build easier to distribute and maintain. It does not prove greater detector accuracy or lower CPU use than Maxine. `Detector` publishes normalized face boxes to a separate framing controller, making a future NVIDIA or DirectML detector feasible without replacing the camera-motion logic. Those backends are not implemented here.

## Build a Windows package using GitHub

1. Create a GitHub repository and upload the **contents** of this folder, including `.github/workflows/build.yml`, at the repository root. A git push preserves the dot-directory reliably; uploading only visible files may omit it.
2. Open the repository's **Actions** tab and run **Build SteadyFrame**, or let the push trigger it.
3. After a successful Windows job, download the **SteadyFrame-Windows-x64** artifact.
4. Extract the artifact wrapper, then extract `SteadyFrame-0.1.0-Windows-x64.zip` inside it.
5. Follow QUICKSTART.md. That built ZIP contains the DLL, model, effect, licenses, and installer.

The workflow builds dependencies from pinned versions and tests the detector and framing controller. The initial build can take a while; subsequent runs cache dependencies. It uploads an artifact, not a public release. GitHub availability, quotas, and build-machine updates can affect execution.

## Build locally on Windows

Install Visual Studio 2022's **Desktop development with C++** workload and Windows SDK 10.0.22621, CMake 3.28+, and Git for Windows. Use 64-bit Windows. From this folder in PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

The script builds OpenCV 4.12.0 as static libraries with the dynamic Microsoft runtime, uses the official OBS plugin template's dependency bootstrap, builds Release, runs tests, and produces `SteadyFrame-0.1.0-Windows-x64.zip`. The template currently pins the OBS 31.1.1 development baseline. Newer OBS releases need compatibility testing; no plugin can promise immunity to every future OBS change.

## Linux development

With CMake 3.28+, libobs development files, and OpenCV >=4.8 (4.12 tested):

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

No Qt or OBS frontend API is used. The Windows installer is the intended packaging target. Linux packaging and macOS distribution are not polished in this alpha.

To test only camera-motion math without OBS or OpenCV:

```sh
cmake -S . -B build-core -DSTEADYFRAME_CORE_ONLY=ON
cmake --build build-core
ctest --test-dir build-core --output-on-failure
```

## Design and limits

The render thread captures the un-cropped input, downsamples it to at most 512 pixels on its longest side, and maps a staging surface on a subsequent frame. The detector thread consumes a one-frame mailbox. The renderer reads the newest result, rejects old observations, and updates the crop continuously at OBS's render cadence. Analysis never opens the camera independently.

Spatial face association uses proximity and size continuity, not identity recognition. It may swap people when faces overlap or move abruptly, and the group mode can include background faces. YuNet is a face detector, not a full head/body tracker. Profiles, occlusion, darkness, small faces, and motion blur can cause dropouts. There is no optical flow, predictive model, identity lock, automatic speaker detection, or preview-only overlay yet.

The crop preserves input aspect ratio and output dimensions. It cannot invent content past image edges. Maximum zoom and face-edge protection can override the desired framing size. Smoothing and the dead zone intentionally permit temporary off-center positioning.

This alpha supports SDR and bypasses processing in HDR paths. There is one detector worker per filter instance; adding many instances increases CPU and memory use. GPU readback and extra render passes still cost time. Inference timing in the settings is not the full end-to-end render latency.

## Next validation targets

1. Build on Windows, verify Direct3D shader compilation, filter chaining, hotkeys, unload, and camera resolution changes.
2. Measure CPU/GPU time and OBS render lag with the RTX 5060 Ti while gaming and NVENC streaming.
3. Record consented test footage with leaning, profiles, standing, leaving/re-entering, and a second person crossing behind.
4. Compare framing error, unwanted subject switches, recovery time, and perceived motion against StreamFX using the same footage.
5. Tune the presets from evidence; consider an optional Maxine backend only after measuring detector shortcomings.

## Sources and licenses

- OBS source/filter API: https://docs.obsproject.com/reference-sources
- Official OBS plugin template: https://github.com/obsproject/obs-plugintemplate
- OpenCV FaceDetectorYN: https://docs.opencv.org/4.12.0/df/d20/classcv_1_1FaceDetectorYN.html
- YuNet model and license: https://github.com/opencv/opencv_zoo/tree/main/models/face_detection_yunet
- NVIDIA Maxine Windows requirements: https://docs.nvidia.com/maxine/ar/latest/WindowsARSDK/Windows.html

New SteadyFrame code is GPL-3.0-or-later. See LICENSE and THIRD-PARTY.md. No NVIDIA runtime or StreamFX source is included.
