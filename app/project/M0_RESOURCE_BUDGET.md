# M0 Resource Budget

Baseline command:

```bash
source ../../SiFli-SDK/export.sh
scons --board=sf32lb52-lchspi-ulp -j8
arm-none-eabi-size build_sf32lb52-lchspi-ulp_hcpu/main.elf
```

## Baseline

- SDK: `056ac78384aad5b2600deb684af643a1beeed8a5`
- Bluetooth submodule: `18be0ad344659a6f2154123e0dda4c2f03083f83`
- Board: `sf32lb52-lchspi-ulp_hcpu`
- Toolchain: SiFli SDK profile `default`, GCC `arm-none-eabi-gcc 14.2.1`

## Board Partitions

From `SiFli-SDK/customer/boards/sf32lb52-lchspi-ulp/ptab.json`:

| Region | Size | Purpose |
|---|---:|---|
| `HCPU_FLASH_CODE` | 8 MB | HCPU firmware |
| `FS_REGION` | 4 MB | runtime filesystem |
| `PSRAM_DATA` | 8 MB | external RAM |
| `HCPU_RAM_DATA` | 511 KB | HCPU RAM |
| `LPSYS_RAM` | 24 KB | LCPU RAM |

## Current Firmware Size

`arm-none-eabi-size build_sf32lb52-lchspi-ulp_hcpu/main.elf`:

| text | data | bss | dec | hex |
|---:|---:|---:|---:|---:|
| 296660 | 3112 | 27540 | 327312 | 4fe90 |

Generated artifacts:

- `main.bin`: about 293 KB
- `uart_download.sh`: generated
- `sftool_param.json`: generated

## SCons Memory Summary

From the M0 local build:

| Region | Total | Used | Free |
|---|---:|---:|---:|
| ROM2 | 4.0 MB | 11.0 KB | about 4.0 MB |
| ROM | 8.0 MB | 318.8 KB | about 7.7 MB |
| RAM | 511.0 KB | 159.5 KB | 351.5 KB |

Notable RAM sections:

| Section | Used |
|---|---:|
| `.bss` | 45.9 KB |
| `.text` | 42.3 KB |
| `.retm_data` | 26.2 KB |
| `.stack` | 16.0 KB |
| `.data` | 10.4 KB |
| `.heap` | 6.0 KB |

## Open Measurements

- Runtime `heap_free_min` still requires a flashed board and serial/FinSH output.
- Task stack high-water marks still require runtime instrumentation.
- FS free bytes still require filesystem mount/runtime check.
- M0 static link has enough RAM headroom for the next milestone, but LVGL buffers and GIF decode buffers must be placed deliberately when they are introduced.
