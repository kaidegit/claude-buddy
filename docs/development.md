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
