# Unity Test

This directory vendors the minimal Unity C test framework files used by the
manual hardware regression test firmware.

- Upstream: https://github.com/ThrowTheSwitch/Unity
- Version: v2.6.1
- License: MIT, see `LICENSE.txt`

Only the core files from `src/` are included because this repository uses the
SiFli SCons firmware build instead of Ceedling, CMake, or Unity's helper
scripts.
