# Validation — 2026-10-30

## Completed

- Full CMake configure and native Linux plugin build with GCC 13.3, OBS development/runtime 30.2.3, and OpenCV 4.12.0 compiled from source.
- Independent plugin source compile with `-Wall -Wextra -Werror`.
- CTest: 2/2 tests passed (tracking controller and detector worker).
- Controller tests cover centering, desired zoom, temporary face loss, return-to-wide, subject continuity with a larger bystander, stale/invalid/low-confidence input, freeze, indefinite hold, group framing, 30 vs 60 fps motion, and 20,000 randomized bounded crops.
- Controller run under AddressSanitizer and UndefinedBehaviorSanitizer passed. LeakSanitizer was disabled because it cannot inspect processes in this execution environment; this is not a leak-check result.
- Detector tests load the actual bundled ONNX file, detect no faces in a blank image, publish a result from the background worker, shut down cleanly, and contain a missing-model error.
- Separate real-image smoke test detected one face in OpenCV's sample image. One observed inference took about 28 ms here; this is not a benchmark for the user's CPU or RTX 5060 Ti.
- An actual headless OBS session loaded the pre-rename plugin, created a synthetic camera source containing the sample image, added the filter, compiled its OpenGL shader, rendered 149 output frames over approximately five seconds, removed the filter, and shut down cleanly. Rendering used Mesa llvmpipe under Xvfb, not a physical GPU.
- Re-detection of the rendered output found face center at approximately **49.9% / 49.5%**, versus **54.8% / 55.9%** before cropping. The requested test face height was 55%; re-detected output height was approximately 56.8%. This is one stationary fixture, not a tracking-accuracy study. The test deliberately used zero dead zone and a closer shot than the default Balanced preset.
- Visually inspected the rendered result for framing, normal color order, and absence of black crop borders. Test fixture images are not distributed in this package.

## Not completed

- Windows/MSVC build, Windows installer execution, or Windows Direct3D shader compilation.
- Live camera testing, RTX 5060 Ti performance, gaming/streaming contention, multiple plugin instances, or camera resolution changes during a stream.
- OBS UI visual inspection, source hotkey persistence, multi-filter ordering, every supported OBS release, or exhaustive failure/recovery tests.
- Comparison against StreamFX, NVIDIA Maxine, or other auto-framing products.
- Real-world multi-person identity continuity or measured quality under occlusion, motion blur, profiles, and poor lighting.
- HDR processing: intentionally bypassed in this alpha.

The Windows GitHub Actions workflow is supplied but has not been run. Its build and packaging scripts are an intended path to a Windows artifact, not evidence that a Windows artifact already exists.

## InFrame rename — 2026-10-31

Renamed the UI, native module, CMake option, namespace, installer, build artifacts, and documentation. Legacy serialized source/hotkey IDs remain only for saved-scene compatibility. The Windows installer moves the old module outside OBS plugin discovery and retains a backup. A successful Windows build of the previous version was reported by the user; the renamed build is checked separately through CI.
