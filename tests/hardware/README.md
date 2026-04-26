# Buddy Hardware Tests

This directory contains a standalone Unity-based firmware project for manual
pre-release regression on the Huangshan Pi `sf32lb52-lchspi-ulp` HCPU target.

The test firmware boots RT-Thread, runs the registered Unity tests once, prints
a stable result marker, and then stays alive so the serial console remains
available. The same tests can be rerun from FinSH with `buddy_hwtest`.

## Build

From the repository root:

```bash
./SiFli-SDK/install.sh
cd tests/hardware/project
source ../../../SiFli-SDK/export.sh
scons --board=sf32lb52-lchspi-ulp -j2
```

## Flash

Use the generated script for the available transport:

```bash
./build_sf32lb52-lchspi-ulp_hcpu/download.sh
```

or:

```bash
./build_sf32lb52-lchspi-ulp_hcpu/uart_download.sh -p /dev/cu.usbserial-XXXX
```

## Pass Criteria

Watch the board serial console. A passing run ends with:

```text
BUDDY_HW_TEST_RESULT:PASS failures=0
```

A failing run prints Unity failure details and:

```text
BUDDY_HW_TEST_RESULT:FAIL failures=N
```

These tests are intentionally not wired into GitHub Actions. They are for
manual hardware regression before release.
