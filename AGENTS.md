# Development guidance

## Project context
- BarnyardVR's goal is to port the OpenBarnyard codebase to VR. It is based heavily on OpenBarnyard; preserve upstream attribution and prefer narrow, sync-friendly changes.
- Keep inherited `OpenBarnyard/` source paths and Premake workspace/project names stable. The generated solution and output directory retain the `OpenBarnyard` name; the Windows game binary is `BarnyardVR.exe` and the associated mod import library is `BarnyardVR.lib`.

## Build and style
- On Windows, initialize submodules with `git submodule update --init --recursive`, run `GenerateProjects-WIN-DX11-VS2026.bat`, then build the generated solution with Visual Studio 2026. Do not commit generated solutions or build output.
- Follow `.clang-format` and nearby code conventions. Avoid reformatting inherited files outside the requested change.
- Keep platform-specific Premake settings under the relevant filters. When changing a target name, update dependent launch commands and import-library references without renaming unrelated directories or projects.

## Documentation
- Keep `README.md` concise and user-facing. Preserve its section order: image and description, Features, Installation, Settings, Content Disclosure, Notes, Building, and Contributing. Preserve the existing Notes text unless explicitly asked to change it.
- When user-facing features or VR settings change, update the README's feature list or settings table, including values, defaults and restart requirements. Check `AVRSettings` and `AVROptionsState` rather than assuming the table is complete. Keep installation and build instructions aligned with the actual release workflow and supported platforms.
- Do not add implementation details, long control-mapping breakdowns or change-by-change history to the README. Keep genuinely useful contributor guidance here or in focused developer documentation instead.
- The VR settings menu displays five settings per page; add setting rows before `ROW_PAGE` so pagination includes them. Verify navigation, mouse focus, save, cancel and restore defaults on every page.
- VR settings are stored separately in `BarnyardVR.cfg`; add typed, validated values to `VR/AVRSettings` and expose user-facing settings through `AVROptionsState`. Keep settings out of the original binary save layout and preserve the menu's save, cancel and restore-default behavior.
- The VR controller layout resolver keeps command dispatch and prompts in sync. OpenXR stick and haptic handling is in `Platform/Windows/Input/TInputDeviceController_OpenXR.cpp`; exercise bikes and minigames as well as on-foot controls when changing it. On devices without an application menu button, the two-stick-click chord supplies pause.
- Fixed UI retains the session/recenter reference; head-relative UI follows eligible first-person views and aligns fixed minigame and bike entries. Head-relative floor height applies only in eligible on-foot views. When changing these behaviors, check free-roam, aiming, pause, honey, fixed-camera minigames and bike transitions independently.

## Validation
- `.github/workflows/draft-release.yml` builds and drafts releases when `VERSION` changes on `main`, or by manual dispatch; it never publishes them. Existing drafts must be published or deleted before another draft can be created. It is not general PR test coverage. Do not assume other automated CI coverage.
- Validate changes with `git diff --check`; run a Windows build when a Windows toolchain is available, and note when it cannot be run.
- Use ad hoc test scripts only for temporary local validation. Do not commit them; remove them before finishing or opening/updating a pull request.
- Feature-specific scripts and mock-backed checks do not establish regression coverage for the wider, largely untested codebase. Report their limited scope explicitly; do not treat passing them as a substitute for a Windows build, code review, or in-game/headset validation.
