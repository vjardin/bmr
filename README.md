# bmr — PMBus CLI for Flex BMR685/BMR456 (Linux I2C)

This tool speaks PMBus/SMBus over `/dev/i2c-*` to read sensors, configure
rails, margin voltages, manage timing and on/off behavior, and access
Flex (former Ericsson) manufacturer features. JSON output is powered by Jansson.

* `FPM-AppNote302-PMBus-Command-Set-RevD.pdf`
* `FPM-TechSpec-BMR685-RevA_2022-03-09-082051_rhel.pdf`
* `p010641-.pdf (BMR456)`

## Build (Meson)

```bash
meson setup build # -Dfully_static=false
meson compile -C build
sudo meson install -C build
```

## Global CLI layout & options

All commands accept the bus and address; most support JSON output.

```bash
bmr --bus /dev/i2c-1 --addr 0x40 <command> [subcommand] [--pretty-off|P]
```

* `--bus` Linux I2C device path (default: `/dev/i2c-1`).
* `--addr` 7-bit device address (default: `0x40`).
* `--pretty-off|P` disable pretty output

## read — Telemetry & sensor data

```bash
bmr ... read all|vin|vout|iout|temp|freq|duty
```

### What it does

Reads common PMBus telemetry using standard `READ_*` commands (e.g.,
`READ_VIN`, `READ_VOUT`, `READ_IOUT`, `READ_TEMPERATURE_x`, `READ_FREQUENCY`,
`READ_DUTY_CYCLE`). Linear data encoding follows PMBus (LIN-11 or LIN-16u);
output voltage scaling uses `VOUT_MODE`.

### Key options

* `all` – dump all supported sensors in one JSON block.
* Specific sensor names – only that measurement.

### Use case

Bring-up sanity check before enabling loads:

```bash
bmr --bus /dev/i2c-1 --addr 0x40 read all
```

Validate VIN is within range, VOUT ≈ expected setpoint, and TEMP stays safe
while idling.

## status — Faults, warnings, and flags

```bash
bmr ... status
```

### What it does

Collects and decodes PMBus `STATUS_*` registers (`STATUS_WORD`, `STATUS_VOUT`,
`STATUS_IOUT`, `STATUS_INPUT`, `STATUS_TEMPERATURE`, `STATUS_CML`,
`STATUS_MFR_SPECIFIC`, etc.) into a single JSON. Helps interpret
present/latched faults and warnings.

### Use case

Root-cause a rail shutdown during board test:

```bash
bmr ... status
```

If `STATUS_INPUT` shows UVLO while `STATUS_VOUT` flags TON_MAX_FAULT, sequence
or upstream supply is suspect. Cross-check timing (below).

## snapshot — Flex/Ericsson snapshot buffer

```bash
bmr ... snapshot [--cycle <n>] [--decode]
```

### What it does

Reads vendor snapshot (e.g., `MFR_GET_SNAPSHOT`), a manufacturer log capturing
telemetry and status at event times. `--cycle` selects which snapshot entry to
read; `--decode` converts linear formats and decodes status bits. Availability
and depth are device-specific (BMR685 documents the feature).

### Use case

After unexpected PG (Power Good) drop, fetch the last event record:

```bash
bmr ... snapshot --cycle 0 --decode
```

Correlate VIN dip + IOUT surge at the fault time to confirm overload rather than
configuration error.

## mfr-multi-pin — Configure Multi-Pin / Power-Good behavior

```bash
bmr ... mfr-multi-pin get
bmr ... mfr-multi-pin set --mode standalone|dls --pg pushpull|highz \
                          --pg-enable 0|1 --sec-rc-pull 0|1
```

### What it does

Interfaces with `MFR_MULTI_PIN_CONFIG` (0xF9). Lets you select standalone/DLS
modes, PG driver type (push-pull vs high-Z/open), enable/disable PG, secondary
RC pull behavior, etc., as documented for BMR685/BMR456. Exact bitfields are
device-specific; this command reads/modifies/writes the byte safely.

### Use case

Make PG compatible with downstream logic requiring high-Z (open-drain) signaling:

```bash
bmr ... mfr-multi-pin set --pg highz --pg-enable 1
```

Now PG can be wire-ORed across rails without contention.

## fwdata — Manufacturer firmware data dump

```bash
bmr ... fwdata
```

### What it does

Dumps `MFR_FIRMWARE_DATA` (0xFD) as a block string and shows device/firmware
information that isn’t covered by standard `PMBUS_REVISION`/ID strings. Format
is vendor-specific; output is provided as text/JSON.

### Use case

Inventory/validation in production:

```bash
bmr ... fwdata
```

Ensure all units report the expected FW build before enabling STORE to NVM.

## restart — Vendor restart trigger

```bash
bmr ... restart
```

### What it does

Triggers a soft restart via `MFR_RESTART` (0xFE). Useful after changing
configuration that the module applies at startup, or to clear certain latched
states without power-cycling the board. Behavior is device-specific; consult
the technical spec.

### Use case

Apply multipin or timing changes that require restart semantics.

```bash
bmr ... restart
```

Plan a brief service window (rail drop) if the device restarts output.

## id — Manufacturer identification strings

```bash
bmr ... id
```

### What it does

Reads block strings such as `MFR_ID`, `MFR_MODEL`, `MFR_REVISION`, and standard
`PMBUS_REVISION`, publishing them as JSON. Strings are in PMBus BLOCK format and
are trimmed of trailing NUL/CR/LF.

### Use case

Quick field identification, test the i2c/PMBus:

```bash
bmr ... id
```

Match model and revision to your qualification matrix before running
margins/timing profiles.

## user-data — Read/write user NVM fields

```bash
bmr ... user-data get
bmr ... user-data set --ascii "Hello" [--store]
```

### What it does

Accesses vendor `USER_DATA_00` (0xB0) area for storing small notes or process
metadata (e.g., calibration token). With `--store`, the tool persists changes
via `STORE_USER_ALL` or device-specific store, observing NVM latency. See App
Note 302 for STORE/RESTORE semantics and timing.

### Use case

Stamp a unit with a calibration tag, manufacturing information:

```bash
bmr ... user-data set --ascii "Cal=OK@2025-11-09" --store
```

Wait ~5–10 ms before re-reading to allow NVM write to complete.

## onoff — ON_OFF_CONFIG policy

```bash
bmr ... onoff get
bmr ... onoff set --powerup controlled|immediate \
                  --source pmbus|pin|both \
                  --en-active low|high \
                  --off soft|immediate \
                  [--raw 0xNN]
```

### What it does

Reads/modifies `ON_OFF_CONFIG` (0x02): control source (PMBus, pin, both),
enable polarity, soft vs immediate off, and power-up behavior. This determines
how `OPERATION` interacts with the pin and how the module treats pre-biased
outputs. App Note 302 defines bit semantics; device docs add notes for pre-bias
handling.

### Use case

Allow either pin or PMBus to enable the rail, with soft-off for load safety:

```bash
bmr ... onoff set --powerup controlled --source both --en-active high --off soft
```

Now `operation set --off` ramps down gracefully per TON/TOFF settings.

## operation — Turn rail on/off and apply margin states

```bash
bmr ... operation get
bmr ... operation set [--on|--off] [--margin normal|high|low] [--raw 0xNN]
```

### What it does

Manipulates `OPERATION` (0x01): on/off state and margin selector (normal, high,
low). The effect depends on `ON_OFF_CONFIG` policy and whether margins are
pre-programmed (see `vout`). App Note 302 documents bit meanings.

### Use case

Automated margin test at +5%:

```bash
bmr ... vout set --set-all 1.00 --margin-pct 5
bmr ... operation set --on --margin high
```

Run functional tests while the rail is at +5%, then return to normal.

## timing — TON/TOFF delays, ramp rates, and fault responses

```bash
bmr ... timing help
bmr ... timing get
bmr ... timing set [--profile safe|sequenced|fast|prebias] \
                   [--ton-delay <ms>] [--ton-rise <ms>] \
                   [--toff-delay <ms>] [--toff-fall <ms>] \
                   [--fault-response <composed>|--fault-byte 0xNN] \
                   [--retries <n>] [--delay-units <u>]
```

### What it does

Controls standard PMBus timing registers: `TON_DELAY` (0x60), `TON_RISE` (0x61),
`TOFF_DELAY` (0x64), `TOFF_FALL` (0x65), and device-specific TON_MAX/TOFF fault
response. Typical ranges are 0…~32.7 s in 1 ms units; actual bounds and accuracy
are device-specific (BMR685 details ranges/accuracy and sequencing notes; BMR456
provides analogous timing controls).

* `Profiles` are convenience presets:

  * `safe` – longer rise/fall and delays to minimize inrush.
  * `sequenced` – stagger multiple rails with `TON_DELAY`.
  * `fast` – minimal delays for quick bring-up (within device limits).
  * `prebias` – pair with `onoff` pre-bias-friendly settings to avoid sinking.
    (You can still override any field explicitly.)
  * Edit the code and add yours

* `fault-response` can be provided as a literal vendor byte (`--fault-byte`) or
built from sub-options (`--fault-response` with retries/unit selectors), as
supported by the device. See the BMR685 tech spec for the exact mapping.

### Use case

Sequence 3 rails with 250 ms spacing and gentle ramps:

```bash
# Rail A
bmr ... timing set --profile sequenced --ton-delay 0   --ton-rise 100 --toff-fall 40
# Rail B
bmr ... timing set --profile sequenced --ton-delay 250 --ton-rise 100 --toff-fall 40
# Rail C
bmr ... timing set --profile sequenced --ton-delay 500 --ton-rise 100 --toff-fall 40
```

This reduces inrush and ensures downstream logic sees rails in the correct order.

## vout — Nominal voltage and margin setpoints

```bash
bmr ... vout get
bmr ... vout set --command <V> [--mhigh <V>] [--mlow <V>] \
                 [--set-all <V> --margin-pct <pct>]
```

### What it does

Programs nominal voltage (`VOUT_COMMAND`) and margin setpoints
(`VOUT_MARGIN_HIGH/LOW`). Values are converted using `VOUT_MODE` (LIN-16u) so
you can specify volts directly. For safety, prefer `--set-all` with `--margin-pct`
to keep margins consistent around the new setpoint. PMBus semantics and linear
conversions are per App Note 302; device voltage ranges and resolution are in
the tech specs.

### Use case

Set a 0.90 V rail and margins +/-5% in one shot:

```bash
bmr ... vout set --set-all 0.90 --margin-pct 5
    # => command=0.90, mhigh~0.945, mlow~0.855
```

Then use `operation set --margin high|low` during validation.

## capability — PMBus CAPABILITY (0x19) decode & checks

```bash
bmr ... capability get
bmr ... capability check --need-pec on|off --min-speed 100|400 --need-alert on|off [--strict]
```

### What it does

Decodes `PMBUS_CAPABILITY` into `pec_supported`, `max_bus_speed` (100/400 kHz),
`smbalert_supported`, and reserved bits. `check` compares against requirements
and returns `{checks:{...}, mismatches:[...]}`.

### Use case

Guard-rail in bring-up scripts to assert 400 kHz + PEC + ALERT:

```bash
bmr ... capability check --need-pec on --min-speed 400 --need-alert on --strict
```

## interleave — Phase count & index (INTERLEAVE 0x37)

```bash
bmr ... interleave get
bmr ... interleave set [--set 0xNN] [--phases 1..16 --index 0..15]
```

### What it does

Reads/writes the `INTERLEAVE` byte (upper nibble = phases-1, lower nibble =
phase index). Some devices may ignore or restrict settings.

### Use case

Set a 2-phase configuration on phase index 0:

```bash
bmr ... interleave set --phases 2 --index 0
```

## hrr — MFR_SPECIAL_OPTIONS (0xE0) bit control

```bash
bmr ... hrr get
bmr ... hrr set [--pec on|off] [--hrr on|off] [--dls linear|nonlinear] \
                [--artdlc on|off] [--dbv on|off]
bmr ... hrr set --raw 0xNN
```

### What it does

Controls `MFR_SPECIAL_OPTIONS` bits such as **PEC require**, **HRR**, **DLS** slope,
**ART/DLC**, **DBV**. Writes only the requested bits or the raw byte.

### Use case

Enable HRR and non-linear droop:

```bash
bmr ... hrr set --hrr on --dls nonlinear
```

> If you turn **PEC on**, your SMBus stack must send PEC.

## vin — VIN_ON/OFF thresholds

```bash
bmr ... vin get [--exp5 N] [--raw]
bmr ... vin set [--on V] [--off V] [--exp5 N] \
                | [--on-raw 0xNNNN] [--off-raw 0xNNNN]
```

### What it does

Programs `VIN_ON (0x35)` and `VIN_OFF (0x36)` using LIN-16u (`--exp5`) or raw
words. Use raw if exponent is unknown.

### Use case

Require VIN ≥ 7.5 V to start, stop below 6.8 V:

```bash
bmr ... vin set --on 7.5 --off 6.8 --exp5 -13
```

## pgood — POWER_GOOD window

```bash
bmr ... pgood get [--exp5 N] [--raw]
bmr ... pgood set [--on V] [--off V] [--exp5 N] \
                  | [--on-raw 0xNNNN] [--off-raw 0xNNNN]
```

### What it does

Controls `POWER_GOOD_ON (0x5E)` / `POWER_GOOD_OFF (0x5F)` (LIN-16u or raw).

### Use case

Ensure PG asserts when VOUT ≥ 96% and de-asserts ≤ 90%:

```bash
bmr ... pgood set --on 0.96 --off 0.90 --exp5 -13
```

## freq — Switching frequency setpoint

```bash
bmr ... freq get
bmr ... freq set --raw 0xNNNN
```

### What it does

Reads/writes the device’s frequency setpoint register (word). Register code and
range are device-specific; use raw word.

### Use case

Bump frequency by a small step for EMI validation:

```bash
bmr ... freq set --raw 0x012C
```

## salert — SMBALERT# mask (0x1B)

```bash
bmr ... salert get
bmr ... salert set --raw 0xNN
```

### What it does

Reads/writes `SMBALERT_MASK` if implemented by the device. Useful to silence
non-critical categories.

### Use case

Mask temperature warnings from ALERT while keeping faults:

```bash
bmr ... salert set --raw 0xXX  # choose per device map
```

## addr-offset — Address offset helper

```bash
bmr ... addr-offset get
bmr ... addr-offset set --raw 0xNN
```

### What it does

Reads/writes `MFR_OFFSET_ADDRESS` (if present) to apply a board-level offset to
the base address.

### Use case

Shift module to `0x5A + 1`:

```bash
bmr ... addr-offset set --raw 0x01
```

## ramp-data — Vendor ramp capture (0xDB)

```bash
bmr ... ramp-data
```

### What it does

Reads `MFR_GET_RAMP_DATA` and returns a hex blob. Format is vendor-specific.

### Use case

Capture ramp profile for offline analysis:

```bash
bmr ... ramp-data
```

## status-data — Vendor status dump (0xDF)

```bash
bmr ... status-data
```

### What it does

Reads `MFR_GET_STATUS_DATA` (vendor snapshot of status bytes) as hex.

### Use case

Fetch condensed status history:

```bash
bmr ... status-data
```

## write-protect — WRITE_PROTECT (0x10)

```bash
bmr ... write-protect get
bmr ... write-protect set [--none|--ctrl|--nvm|--all] | --raw 0xNN
```

### What it does

Controls write protection policy. Typical values: `0x00` (none), `0x40` (control-only),
`0x80` (NVM), `0xFF` (all). Exact semantics can vary per device.

### Use case

Lock NVM while allowing live control:

```bash
bmr ... write-protect set --ctrl
```

## temp — Temperature limits & live sensors

```bash
bmr ... temp get  [all|ot|ut|warn]
bmr ... temp set  [--ot-fault <C>] [--ut-fault <C>] [--ot-warn <C>] [--ut-warn <C>]
bmr ... temp read [all|t1|t2|t3]
```

### What it does

Reads and programs the **OT/UT FAULT/WARN** temperature limits and reads live
temperatures. All limits and readings use **PMBus Linear11** (5-bit signed
exponent, 11-bit signed mantissa) and are converted to °C in the JSON output.
Inputs accept **C (default), K, or F** (e.g., `110`, `358K`, `185F`).

### Key options

* `temp get all|ot|ut|warn` — dump FAULT/WARN limits (decoded °C + raw LIN11).
* `temp set ...` — write one or more limits; values converted from C/K/F to
   LIN11 with readback verification.
* `temp read all|t1|t2|t3` — read `READ_TEMPERATURE_1/2/(3 if present)` and
   decode to °C.

### Use case

Set typical limits and verify live sensors:

```bash
# Program limits
bmr --bus /dev/i2c-1 --addr 0x40 temp set --ot-fault 110 --ot-warn 100 --ut-warn -20 --ut-fault -40

# Persist if desired (device NVM):
bmr --bus /dev/i2c-1 --addr 0x40 user-data set --store

# Read back limits and live temps
bmr --bus /dev/i2c-1 --addr 0x40 temp get all
bmr --bus /dev/i2c-1 --addr 0x40 temp read all
```

## fault — Fault-response policy (OT/UT/VIN/VOUT/TON_MAX/IOUT)

```bash
bmr ... fault get [all|temp|vin|vout|tonmax|iout]
bmr ... fault temp set \
  [--ot-delay 16s|32s|2^n] [--ot-mode ignore|delay-retry|disable-retry|disable-until-clear] [--ot-retries 0..6|cont] \
  [--ut-delay 16s|32s|2^n] [--ut-mode ignore|delay-retry|disable-retry|disable-until-clear] [--ut-retries 0..6|cont]
```

### What it does

Programs and reads the **PMBus FAULT RESPONSE** bytes (PMBus Part II, Table 4).
Each response byte packs three fields:

* **Mode** (bits 7:6):
  * `00` **ignore** — report status only.
  * `01` **delay-then-retry** — wait delay, then apply retry policy.
  * `10` **disable-and-retry** — disable output immediately, then retry after delay.
  * `11` **disable-until-fault-clears** (latch-off).

* **Retries** (bits 5:3): `0..6`, `7=continuous`.
* **Delay** (bits 2:0): time base depends on command family:
  * **Temperature (OT/UT)**: seconds = `2^n` (n in 0..7) → `n=4`=16 s, `n=5`=32 s.
  * **VIN/VOUT/TON_MAX/IOUT**: typically **10 ms/LSB** on BMR45x (see device spec).

`fault get` decodes mode/retries/delay with proper units per family.
`fault temp set` programs the **OT/UT** response bytes with friendly arguments.

### Use case — 1s off, single retry on OT/UT (with temperature thresholds)

The fault response defines what to do when a fault happens; you still need
temperature limits to create the fault. Below sets both:

**Program the OT/UT fault-response policy** (disable, wait 16s, retry once):
```bash
bmr --bus /dev/i2c-1 --addr 0x40 fault temp set \
  --ot-mode disable-retry --ot-retries 1 --ot-delay 16s \
  --ut-mode disable-retry --ut-retries 1 --ut-delay 16s
```

**Set temperature thresholds** (example production-style values; adjust to your design):

```bash
bmr --bus /dev/i2c-1 --addr 0x40 temp set \
  --ot-fault 110 --ot-warn 100 \
  --ut-warn -20  --ut-fault -40
```

**Persist to NVM** (optional, if you want the policy/limits after power cycle):

```bash
bmr --bus /dev/i2c-1 --addr 0x40 user-data set --store
```

**Verify**:

```bash
# Check fault temperature policy
bmr --bus /dev/i2c-1 --addr 0x40 fault get temp

# Check limits
bmr --bus /dev/i2c-1 --addr 0x40 temp get all
```

#### Use case - restart trigger (enforce the 16s OFF + single retry)

Read the live temperature to know your baseline:

```bash
bmr --bus /dev/i2c-1 --addr 0x40 temp read t1
```

Force a fault immediately (pick one):

```bash
# Force OT now (set OT below current temp, e.g., if T1 ~ 25 °C):
bmr --bus /dev/i2c-1 --addr 0x40 temp set --ot-fault 20

# or, Force UT now (set UT above current temp):
bmr --bus /dev/i2c-1 --addr 0x40 temp set --ut-fault 30

# Persist to NVM if you want the policy to survive power cycles
#bmr --bus /dev/i2c-1 --addr 0x40 user-data set --store
```

The rail shall shutdown, wait 16s, retry once, then:

* If the condition cleared (e.g., OT cooled), it recovers.
* If the condition persists (e.g., UT still above ambient), it stays off.

Restore your real thresholds after testing:

```bash
bmr --bus /dev/i2c-1 --addr 0x40 temp set \
  --ot-fault 110 --ot-warn 100 --ut-warn -20 --ut-fault -40

# Persist to NVM if you want the policy to survive power cycles
bmr --bus /dev/i2c-1 --addr 0x40 user-data set --store
```

## Notes & best practices

* **Linear formats**: The tool reads `VOUT_MODE` to scale VOUT and uses
  LIN-11/LIN-16 for other readings.

* **STORE/RESTORE**: When persisting configuration (`STORE_*`), allow ~5–10 ms
  for NVM writes before re-accessing. Avoid frequent stores to extend NVM life.

* **Pre-bias outputs**: If rails may be pre-biased (e.g., shared loads), choose
  `onoff` settings and timing that avoid sinking current on start. Device manuals
  include explicit guidance.

* **PG & system logic**: With `mfr-multi-pin`, set PG to high-Z/open-drain when
  multiple rails feed a common PG net.

## Examples (quick reference)

```bash
# Status & sensors
bmr --bus /dev/i2c-1 --addr 0x40 read all
bmr --bus /dev/i2c-1 --addr 0x40 status

# ID & firmware
bmr --bus /dev/i2c-1 --addr 0x40 id
bmr --bus /dev/i2c-1 --addr 0x40 fwdata

# Capability & checks
bmr --bus /dev/i2c-1 --addr 0x40 capability get
bmr --bus /dev/i2c-1 --addr 0x40 capability check --need-pec on --min-speed 400 --need-alert on --strict

# Snapshot debug
bmr --bus /dev/i2c-1 --addr 0x40 snapshot --cycle 0 --decode
bmr --bus /dev/i2c-1 --addr 0x40 status-data

# PG/Multi-Pin config
bmr --bus /dev/i2c-1 --addr 0x40 mfr-multi-pin set --pg highz --pg-enable 1

# On/Off policy + Operation
bmr --bus /dev/i2c-1 --addr 0x40 onoff set --powerup controlled --source both --en-active high --off soft
bmr --bus /dev/i2c-1 --addr 0x40 operation set --on --margin normal

# Timing and sequencing
bmr --bus /dev/i2c-1 --addr 0x40 timing set --profile sequenced --ton-delay 250 --ton-rise 100 --toff-fall 40

# Voltage, margins, thresholds
bmr --bus /dev/i2c-1 --addr 0x40 vout set --set-all 1.00 --margin-pct 5
bmr --bus /dev/i2c-1 --addr 0x40 vin set --on 7.5 --off 6.8 --exp5 -13
bmr --bus /dev/i2c-1 --addr 0x40 pgood set --on 0.96 --off 0.90 --exp5 -13

# HRR & options
bmr --bus /dev/i2c-1 --addr 0x40 hrr set --hrr on --dls nonlinear

# Frequency & interleave
bmr --bus /dev/i2c-1 --addr 0x40 freq set --raw 0x012C
bmr --bus /dev/i2c-1 --addr 0x40 interleave set --phases 2 --index 0

# Alerts & protection
bmr --bus /dev/i2c-1 --addr 0x40 salert get
bmr --bus /dev/i2c-1 --addr 0x40 write-protect set --ctrl
```
