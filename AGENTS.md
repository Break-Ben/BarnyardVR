# Development guidance

- BarnyardVR is based heavily on OpenBarnyard. Keep inherited `OpenBarnyard/` paths and Premake workspace/project names stable unless a change is necessary; favor small, sync-friendly diffs.
- On Windows, initialize submodules with `git submodule update --init --recursive`, run `GenerateProjects-WIN-DX11-VS2026.bat`, then build the generated solution with Visual Studio 2026. Do not commit generated solutions or build output.
- Follow `.clang-format` and nearby code conventions. Avoid reformatting inherited files outside the requested change.
- Keep platform-specific Premake settings under the relevant filters. When changing a target name, update dependent launch commands and import-library references without renaming unrelated directories or projects.
- Validate changes with `git diff --check`; run a Windows build when a Windows toolchain is available.
