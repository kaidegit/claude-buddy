# Development

This repository is a firmware project plus a pinned SiFli SDK submodule. The
same commands are used locally and in GitHub Actions.

## Repository Layout

```text
.
|-- .github/workflows/firmware.yml
|-- SiFli-SDK/                 # submodule
|-- app/
|   |-- project/               # SCons project, board config, linker and ptab
|   `-- src/                   # Buddy firmware source
|-- docs/                      # public project documentation
|-- third_party/               # external simulator/template submodules
|-- tools/lvgl_pc_sim/         # PC LVGL/SDL simulator overlay
`-- tests/host/                # portable protocol/core tests
```

The local `prd/` directory is intentionally ignored. Public documentation should
be edited under `docs/` or the root `README.md`.

## Setup

Initialize submodules before building:

```bash
git submodule update --init --recursive
```

Install the SiFli SDK toolchain profile:

```bash
./SiFli-SDK/install.sh
```

## Host Tests

Host tests exercise the portable protocol and application code without flashing
the board:

```bash
./tests/host/run_host_tests.sh
```

Run these before changing `app/src/core`, `app/src/bridge`, or protocol-facing
storage hooks.

## LVGL PC Simulator

The PC simulator previews the current LVGL v8 UI at `390x450` without flashing
the board. It links the production UI source with mock `buddy_ui_model_t` data,
the built-in ASCII character renderer, and the `lv_port_pc_vscode` SDL driver.
Runtime GIFs, BLE, board storage, and the SiFli LCD path are intentionally not
simulated in the first version.

Install SDL2 and CMake first:

```bash
brew install sdl2 cmake
```

On Debian/Ubuntu:

```bash
sudo apt install build-essential cmake libsdl2-dev
```

Make sure nested simulator submodules are present:

```bash
git submodule update --init --recursive third_party/lv_port_pc_vscode
```

Build and run from the repository root:

```bash
cmake -S tools/lvgl_pc_sim -B build/lvgl_pc_sim
cmake --build build/lvgl_pc_sim -j2
build/lvgl_pc_sim/buddy_lvgl_pc_sim
```

Keyboard controls:

- `1`, `Enter`, or Right Arrow: primary action.
- `2`, `Space`, or Left Arrow: secondary action.
- `M` or `Tab`: jump to settings.
- `N` / `B`: next / previous mock scene.
- `Q` or `Esc`: quit.

## Firmware Build

Build the HCPU firmware using the same flow as CI:

```bash
cd app/project
source ../../SiFli-SDK/export.sh
scons --board=sf32lb52-lchspi-ulp -j2
```

From the repository root, inspect the generated firmware size with:

```bash
source SiFli-SDK/export.sh
arm-none-eabi-size app/project/build_sf32lb52-lchspi-ulp_hcpu/main.elf
```

Build output is ignored under `app/project/build_*`.

## Documentation Checks

For documentation-only changes, at minimum run:

```bash
git diff --check
```

For code changes, run the host tests and the firmware build unless the change is
strictly isolated to documentation, comments, or repository metadata.

## Contribution Guidelines

- Keep changes focused on one coherent intent.
- Use Conventional Commits, such as `docs: add architecture overview` or
  `fix: reject oversized character chunks`.
- Avoid committing generated build output.
- Keep public docs user-facing; do not copy internal planning notes directly
  into tracked documentation.
- Preserve the dependency boundary: core logic should stay portable, while
  SiFli SDK calls belong in transport, UI, storage, or platform modules.
