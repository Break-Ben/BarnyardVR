# Development guidance

## Project context
- BarnyardVR's goal is to port the OpenBarnyard codebase to VR. It is based heavily on OpenBarnyard; preserve upstream attribution and prefer narrow, sync-friendly changes.
- Keep inherited `OpenBarnyard/` source paths and Premake workspace/project names stable. The generated solution and output directory retain the `OpenBarnyard` name; the Windows game binary is `BarnyardVR.exe` and the associated mod import library is `BarnyardVR.lib`.

## Build and style
- On Windows, initialize submodules with `git submodule update --init --recursive`, run `GenerateProjects-WIN-DX11-VS2026.bat`, then build the generated solution with Visual Studio 2026. Do not commit generated solutions or build output.
- Follow `.clang-format` and nearby code conventions. Avoid reformatting inherited files outside the requested change.
- Keep platform-specific Premake settings under the relevant filters. When changing a target name, update dependent launch commands and import-library references without renaming unrelated directories or projects.

## Validation
- There are currently no GitHub Actions workflows, so do not assume automated CI coverage.
- Validate changes with `git diff --check`; run a Windows build when a Windows toolchain is available, and note when it cannot be run.
