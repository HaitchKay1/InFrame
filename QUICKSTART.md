# SteadyFrame 0.1.0 — first alpha

SteadyFrame is a native OBS effect filter that crops and smoothly follows faces. Balanced is the default. This is an initial implementation, not a validated StreamFX replacement yet.

## If you have the built Windows ZIP

1. Extract the entire ZIP. Close OBS.
2. Double-click `Install.cmd` and approve the Windows administrator prompt. It installs only SteadyFrame, and backs up an older SteadyFrame installation before replacing its files.
3. Start OBS. Right-click your camera source > **Filters**.
4. Under **Effect Filters**, click **+** > **SteadyFrame — Face Tracking**.
5. Start with **Balanced**, **Face size 35%**, and **Maximum zoom 2.5**. The default face position is the center of the picture.
6. Move left and right. Increase Follow smoothing for gentler movement; decrease it for a closer follow. Set Ignore small movements to 0 if exact centering matters more than avoiding small camera movements.

The source ZIP does not contain a Windows DLL. See README.md for the Windows build workflow.

## Useful controls

| Control | What it does |
|---|---|
| Starting preset | Choose Balanced, Responsive, or Cinematic, then click Apply preset; Custom leaves your values untouched |
| Automatically adjust zoom | Maintains an approximate face size as you move closer or farther away |
| Fixed zoom | With auto zoom off, follows your face at a chosen zoom level; face-edge protection may widen the shot |
| Face size | Desired face height as a percentage of the output picture |
| Follow smoothing | Higher values produce gentler pans; lower values react faster |
| Face position | 50 / 50 centers your face; vertical 40 moves it a little above center |
| Freeze framing | Holds the current crop without moving the picture |
| Find me again | Releases the current subject selection and looks for the largest face |
| Who to follow | Stay with my face, largest face, or include all detected faces |
| Ignore small movements | Prevents small head movements from constantly moving the camera |
| Hold shot after face disappears | Waits before widening the shot; disable return-to-wide to hold indefinitely |
| Face checks per second | CPU/performance trade-off; OBS output still renders at its configured frame rate |
| Refresh detector status | Refreshes the detector's face count, inference timing, or error message |

Advanced controls appear together below the main controls. For hotkeys, open **OBS Settings > Hotkeys** and search for SteadyFrame. Freeze hotkey and checkbox are independent; both must be off to resume.

## Camera setup

Leave room around your head in the original camera image. A wide camera angle and a 4K input provide more cropping room. Digital framing cannot reconstruct the room outside the camera view or restore detail lost to enlargement. Exact centering may be impossible at an edge. Keep SteadyFrame ahead of any filter that already removes the surrounding image; check the preview after ordering filters.

Use SDR camera video and an SDR OBS composition for this alpha. HDR processing is deliberately bypassed. OBS transforms, scene positioning, and source resolution remain yours to control.

## Troubleshooting

- **Not in the filter list:** check `C:\ProgramData\obs-studio\plugins\steadyframe\bin\64bit\steadyframe.dll`, restart 64-bit OBS, and inspect Help > Log Files. A 64-bit Visual C++ 2015–2022 runtime is required. Standard OBS installs normally have it. Portable OBS may need a manual plugin installation.
- **No tracking:** refresh detector status. Check that the model exists under `steadyframe/data/models`. Use SDR. Check both freeze controls. Move into brighter light and face the camera.
- **Chases another face:** use Stay with my face. Find me again selects the largest detected face. This is spatial association, not biometric identity recognition; overlapping faces can still swap.
- **Too much movement:** increase Follow smoothing and Ignore small movements.
- **Slow to follow:** decrease smoothing, then try 20 or 30 checks per second if your CPU has spare capacity.
- **Soft image:** reduce Maximum zoom or increase the camera input resolution.
- **Uninstall:** close OBS and delete only `C:\ProgramData\obs-studio\plugins\steadyframe`. Previous installer backups are outside OBS's plugin folder under `C:\ProgramData\SteadyFrame\backups`.
