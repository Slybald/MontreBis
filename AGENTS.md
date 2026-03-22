# AGENTS.md

## Cursor Cloud specific instructions

### Project Overview

This is a **Connected Watch** embedded firmware project built on the **nRF Connect SDK (NCS) v3.2.1** (which includes a Zephyr RTOS 4.2.99 fork). It targets the **nRF5340 DK** with BLE, LVGL display, environmental/motion sensors, compass, pedometer, and RTC.

### SDK & Toolchain Locations

| Component | Path |
|---|---|
| NCS workspace | `/opt/ncs` |
| Zephyr base | `/opt/ncs/zephyr` |
| Zephyr SDK (ARM toolchain) | `/opt/zephyr-sdk-0.17.0` |

### Required Environment Variables

Before running `west build`, `twister`, or other Zephyr commands, export:

```bash
export PATH="$HOME/.local/bin:$PATH"
export ZEPHYR_BASE=/opt/ncs/zephyr
export ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-0.17.0
export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
```

### Build

```bash
cd /workspace
west build --build-dir build . --pristine --board nrf5340dk/nrf5340/cpuapp/ns
```

**Critical**: The board qualifier is `nrf5340dk/nrf5340/cpuapp/ns` (non-secure, with TF-M). Do **not** use `cpuapp` without `/ns` — the project requires TF-M and the non-secure board variant.

### Lint

```bash
/opt/ncs/zephyr/scripts/checkpatch.pl --no-tree --file src/*.c src/ui/*.c
```

### Test (Twister, build-only)

```bash
python3 $ZEPHYR_BASE/scripts/twister -T . -p nrf5340dk/nrf5340/cpuapp/ns --build-only
```

Hardware is required for runtime testing; `--build-only` is the appropriate mode for cloud environments.

### Key Gotchas

- **NCS, not vanilla Zephyr**: This project depends on nRF Connect SDK v3.2.1 (Nordic's fork), not upstream Zephyr. Vanilla Zephyr will fail with Kconfig and LVGL API mismatches.
- **No runtime testing possible**: This is bare-metal embedded firmware. There are no services to start, no web UI, no emulator that supports the full hardware set (BLE + display + sensors + RTC). All validation in cloud is build-only.
- **LVGL version**: NCS v3.2.1 ships LVGL 9.x. The project uses `lv_point_precise_t` and other LVGL 9 APIs.
- **TF-M secure firmware**: The build produces both a TF-M secure image and the application non-secure image, merged into `build/merged.hex`.
