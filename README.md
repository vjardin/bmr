# bmr — PMBus CLI for Flex BMR685/BMR456 (Linux I2C)

This tool speaks PMBus/SMBus over `/dev/i2c-*` to read sensors, decode vendor
**snapshots** (0xD7), configure **MFR_MULTI_PIN_CONFIG** (0xF9), dump **MFR_FIRMWARE_DATA**
(0xFD), trigger **MFR_RESTART** (0xFE), and access **USER_DATA_00** (0xB0). JSON output
is powered by Jansson.

## Build (Meson)
```bash
meson setup build -Dfully_static=false
meson compile -C build
sudo meson install -C build
```

## Usage
```bash
bmr --bus /dev/i2c-1 --addr 0x40 read all --json --pretty
bmr --bus /dev/i2c-1 --addr 0x40 status --json --pretty
bmr --bus /dev/i2c-1 --addr 0x40 snapshot --cycle 0 --decode --json --pretty
bmr --bus /dev/i2c-1 --addr 0x40 mfr-multi-pin get
bmr --bus /dev/i2c-1 --addr 0x40 mfr-multi-pin set --mode standalone --pg pushpull --pg-enable 1 --sec-rc-pull 0
bmr --bus /dev/i2c-1 --addr 0x40 fwdata --json --pretty
bmr --bus /dev/i2c-1 --addr 0x40 restart
bmr --bus /dev/i2c-1 --addr 0x40 id
bmr --bus /dev/i2c-1 --addr 0x40 user-data get
bmr --bus /dev/i2c-1 --addr 0x40 user-data set --ascii "Hello" --store
bmr --bus /dev/i2c-1 --addr 0x40 timing help
bmr --bus /dev/i2c-1 --addr 0x40 timing get
bmr --bus /dev/i2c-1 --addr 0x40 timing set --profile sequenced
bmr --bus /dev/i2c-1 -a 0x40 timing set --ton-delay 250 --ton-rise 100 --toff-fall 20
```

**Notes:**
- Linear conversions follow PMBus (VOUT uses `VOUT_MODE` exponent).
- After `STORE_*` commands, allow ~5–10 ms NVM latency before re‑access.
- based on 1/28701-FGC 101 1823 revD February 2013 Ericsson AB BMR456 Technical specification
- based on 1/28701-BMR685 Rev.A July 2021 Flex BMR685 Technical specification that provides more details
