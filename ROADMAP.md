# DenpaModem Roadmap

## Goal

Build a programmable embedded baseband modem for amateur radio use.

The first target is POCSAG transmit over the ESP32 DAC. The design should stay open for additional protocols, output backends, and control transports without forcing a rewrite of the core architecture.

## Non-Goals

- Do not patch or fork RadioLib.
- Do not couple application control to one transport.
- Do not make POCSAG assumptions part of generic DAC or command infrastructure.
- Do not require a sound card or external modem IC for the primary ESP32 DAC path.

## Architecture

### Module / Plugin Model

DenpaModem should use project-owned modules/plugins above RadioLib.

Plugin types:

- RadioLib-compatible extensions
  - `PhysicalLayer` backends such as `Esp32DacPhy`, PWM, I2S DAC, or external DAC.
  - Optional RadioLib protocol/client additions.
  - Must stay free of DenpaModem web, DAPNET, MQTT, and configuration UI logic.
- Protocol modules
  - POCSAG, APRS, RTTY, CW, and future modulation/protocol modules.
  - Own protocol-specific settings, validation, commands, and transmit API.
  - May use RadioLib, another library, or custom encoder code.
- Transport modules
  - Serial command console.
  - Web/API.
  - TCP command socket.
  - Future UDP where useful.
- Integration modules
  - DAPNET.
  - MQTT.
  - Hamlib network control.
  - Direct CAT control.
  - Metrics.
- Hardware/service modules
  - Network manager.
  - PTT manager.
  - Configuration store.
  - TX scheduler/queue.

Rules:

- Protocol modules expose DenpaModem APIs, not raw RadioLib APIs.
- Transport and integration modules call protocol modules through shared application services.
- Protocol-specific logic should not leak into transports.
- Transport-specific parsing should not leak into protocol modules.
- If a useful protocol is missing in RadioLib, evaluate whether to implement it locally first or contribute it upstream to RadioLib.

### Module Orchestration

Use a central application context that owns shared services and module registration.

Core services:

- Configuration store.
- Event bus.
- Command registry.
- Protocol registry.
- Output registry.
- Network manager.
- Radio/PTT controller.
- TX scheduler.
- Logger/status service.

Module lifecycle:

```text
construct -> register -> loadConfig -> begin -> loop -> stop
```

Responsibilities:

- `register`
  - Register commands, protocol handlers, web routes, status fields, and configuration schema.
- `loadConfig`
  - Load and migrate module configuration.
- `begin`
  - Start hardware or network resources after configuration is loaded.
- `loop`
  - Run non-blocking periodic work.
- `stop`
  - Release resources when needed.

Modules should communicate through services and events, not direct global references.

Example flow:

```text
WebModule -> CommandRegistry -> PocsagModule -> TxScheduler -> Esp32DacPhy
DapnetModule -> PocsagModule -> TxScheduler -> Esp32DacPhy
SerialModule -> CommandRegistry -> Dac settings / PocsagModule / Status
```

### Configuration Model

Use a shared configuration store with module-owned schemas.

Goals:

- Each module owns its own configuration namespace.
- The core owns storage, persistence, versioning, and migration flow.
- Configuration can be changed through Serial, Web, TCP/API, or defaults.
- Runtime state and persistent configuration stay separate.

Example namespaces:

```text
core.network
core.ptt
output.esp32_dac
protocol.pocsag
integration.dapnet
integration.mqtt
radio.hamlib
radio.yaesu_cat
```

Each module should provide:

- Config schema version.
- Default config.
- Validation.
- Migration from older versions.
- Apply method for runtime changes.
- Export/status view without secrets.

Recommended config storage format:

- JSON-like structured model in code.
- Persist to ESP32 NVS initially.
- Keep room for file-based storage later if LittleFS/SPIFFS is added.

Versioning:

- Global config version for whole-device migrations.
- Per-module config version for plugin-specific migrations.
- Unknown module config should be preserved where possible.
- Failed migration should keep old config and report `ERR config-migration`.

Policy:

- Do not let every plugin invent its own persistence.
- Let every plugin own validation and migration of its own schema.

### Physical Output Backends

- `Esp32DacPhy`
  - RadioLib `PhysicalLayer` adapter.
  - Uses ESP32 DAC, default GPIO 25.
  - Uses fixed virtual direct-FSK center frequency of 100.0 MHz.
  - Maps RadioLib direct-FSK values below center to one DAC level and values above center to the other DAC level.
  - Returns DAC to idle level between transmissions.
  - Keeps hex dump only as optional debug.

Future backends:

- PWM output.
- I2S DAC output.
- External DAC output.
- GPIO direct output for simple keying.

### Internal Modem Output Layer

`Esp32DacPhy` is currently tied to RadioLib `PhysicalLayer`.

Add a project-owned output abstraction above hardware-specific backends, so future modules are not forced to use RadioLib directly.

Goals:

- Keep RadioLib support.
- Allow custom protocol encoders that do not exist in RadioLib.
- Allow other libraries to feed the same output layer.
- Keep DAC, PWM, I2S, and GPIO outputs behind one project-owned interface where practical.

### Protocol Encoders

Initial:

- POCSAG transmit using RadioLib `PagerClient`.

Future candidates:

- AFSK / Bell 202.
- AX.25 / APRS.
- RTTY.
- Morse/CW keying.
- Other simple FSK/OOK protocols supported by RadioLib direct mode.

Protocol-specific logic should not leak into generic output backend names or transport framing.

### Control Layer

Use one command model shared by multiple transports.

Initial transport:

- Serial console.

Future transports:

- TCP socket.
- Ethernet.
- Wi-Fi TCP.
- UDP where useful.
- Web/API bridge if needed.

The parser should receive complete command lines or framed command messages from any transport and call the same application services.

Network setup should be delegated to a reusable `NetworkManager` service/module where possible.

### UART0 Recovery Console

UART0 is always available as the default recovery console.

Responsibilities:

- Boot logs.
- Runtime logs.
- Configuration commands.
- Settings inspection.
- Recovery/factory reset commands.

Policy:

- UART0 support is core recovery functionality, not an optional transport module.
- Runtime log level can be set to `NONE`.
- Command support remains available on UART0.
- Binary protocols such as KISS must not be mixed with logs.
- KISS on UART0 is not planned for initial builds.

Open point:

- If KISS is ever implemented, decide whether it requires a separate UART.

## Control Protocol

Start with a simple line-based text protocol. It is easy to test manually and does not lock the project into a binary framing format too early.

Example commands:

```text
TX POCSAG 1234 ASCII ahoj
POCSAG SPEED 1200
DAC PIN 25
DAC LEVELS 150 50
DAC ENABLE 1
DEBUG HEX 0
STATUS
HELP
```

Example responses:

```text
OK
ERR invalid-address
ERR busy
TX DONE bits=1234
STATUS dac=1 pin=25 mark=150 space=50 idle=100
```

Rules:

- Commands are ASCII text.
- One command per line.
- Responses are one line unless explicitly documented otherwise.
- All transports use the same command grammar.
- Binary payload support can be added later with explicit framing.

## Web Interface

High priority.

The web interface should support at least:

- Manual POCSAG transmit.
- Current DAC configuration.
- Current protocol/module configuration.
- TX status and last error.
- Basic hardware test controls.

The web UI should call the same command/application services as Serial and TCP, not duplicate transmit logic.

## Radio Control

### Hamlib Network Control

Medium priority.

Support network Hamlib/rigctld-compatible control where possible.

Use cases:

- Retune radio before transmit.
- Key PTT.
- Read current radio status.

### Direct CAT Control

Low priority.

Support direct CAT control for selected radios, starting with Yaesu if needed.

Use cases:

- Retune radio without external Hamlib.
- Key PTT if supported.
- Basic status query.

### GPIO PTT

Low priority, but keep architecture ready.

Use cases:

- Direct radio keying through ESP32 GPIO.
- Simple PTT where CAT/Hamlib is not available.

PTT control should be separate from modulation output so protocols can request TX without knowing how the radio is keyed.

## KISS / TNC Compatibility

Very low priority.

KISS may be useful for AX.25/APRS compatibility, but it should not be the primary DenpaModem control protocol.

Plan:

- Keep KISS only as a future optional endpoint for AX.25/APRS.
- Do not force POCSAG or DAC configuration through KISS commands.
- If implemented, run KISS beside the native command protocol, not instead of it.

## DAPNET

High priority for POCSAG.

DAPNET support is a primary standalone use case.

Goals:

- Allow DenpaModem to operate as a standalone DAPNET transmitter endpoint.
- Receive DAPNET messages over network.
- Feed messages into the POCSAG module.
- Reuse the same POCSAG transmit path as manual web/serial commands.
- Keep local/manual test transmit available even when DAPNET is disabled.

Open points:

- DAPNET authentication and configuration model.
- Message queue and retry behavior.
- TX authorization and rate limits.
- Time synchronization requirements.

## Configuration

Runtime configurable:

- DAC enabled.
- DAC pin.
- DAC mark level.
- DAC space level.
- Hex dump debug.
- POCSAG speed.
- Repeated transmit interval for test mode.
- Radio control backend.
- PTT backend.
- DAPNET endpoint and credentials.
- Network settings.
- Enabled modules/plugins.

Later persistent configuration:

- Store selected runtime settings in NVS.
- Add command to save and load defaults.

## Timing

POCSAG DAC transmit currently depends on RadioLib calling `transmitDirect()` and delaying per bit.

Risks:

- Serial debug during TX can disturb timing.
- `dacWrite()` latency should be measured on target hardware.
- Future audio waveform protocols need timer/I2S/DMA driven output, not per-bit `dacWrite()`.

Plan:

- Keep hex dump disabled for real transmit.
- Measure bit timing on GPIO/DAC with logic analyzer or scope.
- Add a timing test command.

## Milestones

### M1: POCSAG DAC Proof of Concept

- `Esp32DacPhy` builds on ESP32.
- POCSAG test message transmits through DAC GPIO 25.
- DAC idle level is centered between mark and space.
- DAC levels are runtime configurable in code.
- Hex dump debug can verify preamble and sync word.

Known FT-991A DATA input calibration:

- `DAC LEVELS 235 20` is the current working default.
- `DAC LEVELS 225 30` works but needs longer reliability testing.
- `DAC LEVELS 210 45` is borderline.
- `DAC LEVELS 255 0` works but uses DAC rail values.

### M2: Serial Control

- Add command parser.
- Add Serial transport.
- Support POCSAG TX command.
- Support DAC level/pin/debug commands.
- Return deterministic `OK`/`ERR` responses.

### M3: Runtime State Model

- Centralize modem settings.
- Avoid direct global manipulation from command handlers.
- Add `STATUS`.
- Prevent overlapping transmissions.
- Add module registry.
- Add command registry.
- Add TX scheduler skeleton.

### M4: Web Control

- Add web interface.
- Support manual POCSAG transmit from browser.
- Support DAC configuration from browser.
- Show TX status and last error.
- Reuse the same application services as Serial control.

### M5: DAPNET

- Add DAPNET configuration.
- Add DAPNET message receiver/client.
- Feed received messages into POCSAG module.
- Keep manual TX independent for hardware testing.

### M6: Persistent Settings

- Add shared configuration store.
- Store selected settings in ESP32 NVS.
- Add `SAVE`, `LOAD`, `DEFAULTS`.
- Add per-module config schema versions.
- Add basic migration hooks.

### M7: Network Control

- Add TCP transport using the same command parser.
- Keep Serial available for recovery/debug.
- Evaluate Ethernet and Wi-Fi variants.
- Integrate or adapt reusable `NetworkManager`.
- Add Hamlib network control evaluation.

### M8: Additional Protocols

- Add AFSK/Bell 202 only after choosing waveform generation method.
- Add AX.25/APRS.
- Keep KISS out of scope unless there is a real user need.

### M9: Observability and Integrations

- Add MQTT integration.
- Add optional metrics endpoint.
- Evaluate Prometheus format.
- Evaluate Zabbix integration.

## Open Questions

- Should DAC pin be limited to ESP32 DAC-capable pins only, or should invalid pins return `ERR`?
- Should mark/space naming stay generic, or become low/high level?
- Should POCSAG transmit be blocking initially, or should commands enqueue jobs?
- Which Ethernet/Wi-Fi stack will be used for TCP transport?
- Do we want a binary framed protocol later, and if yes, SLIP or KISS-like framing?
- How will TX authorization and safety limits be handled?
- What is the right project-owned output abstraction above `Esp32DacPhy`?
- Which parts should be contributed upstream to RadioLib?
- Should Hamlib control be client-only, or should DenpaModem expose a rigctld-like endpoint too?
- What is the minimum useful DAPNET standalone feature set?

## Priority Summary

Highest priority:

- Manual POCSAG transmit over web.
- Manual POCSAG transmit over Serial.
- Stable ESP32 DAC output for hardware testing.

High priority:

- DAPNET standalone POCSAG support.
- Web interface.
- Plugin/module architecture.

Medium priority:

- Shared command/application services.
- Runtime state model.
- Persistent configuration.
- Hamlib network control.

Low priority:

- GPIO PTT.
- Direct Yaesu CAT.
- MQTT.

Nice to have:

- KISS endpoint for AX.25/APRS.
- Prometheus metrics.
- Zabbix integration.
