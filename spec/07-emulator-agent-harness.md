# Topic: Emulator Agent Harness

## Verdict

Autonomous emulator testing is feasible enough to make part of the engineering plan.

Best path:

1. Build `glyph` to `EBOOT.PBP`.
2. Launch PPSSPP from the agent harness with an isolated memstick/config.
3. Enable PPSSPP's remote debugger/WebSocket API.
4. Drive input through WebSocket.
5. Capture screenshots through WebSocket or host display capture.
6. Collect PPSSPP logs and app-written logs.
7. Fail tests on timeout, crash, emulator log errors, missing expected screen state, or visual regressions.

This does not replace PSP Go hardware testing. It gives fast autonomous smoke/regression coverage.

## Local Availability Check

At research time in this workspace, no `ppsspp`, `PPSSPPSDL`, `PPSSPPQt`, or `flatpak` binary was found on `PATH`.

That means the first implementation task needs to install or build PPSSPP in the agent/runtime image before emulator tests can run.

## PPSSPP Capabilities Relevant To Harness

### Command-Line Launch

PPSSPP supports command-line arguments:

- `-d`: debug log level.
- `-v`: verbose log level.
- `-j`, `-i`, `-r`, `-J`: choose CPU backend.
- `--pause-menu-exit`: pause menu exits emulator.
- `--escape-exit`: Escape exits emulator.
- `--fullscreen`
- `--windowed`
- `--appendconfig`: merge another config file.

For harness use:

```sh
PPSSPPSDL --windowed --escape-exit --appendconfig test-ppsspp.ini path/to/EBOOT.PBP
```

Exact binary name depends on installation/build.

### WebSocket Debugger API

PPSSPP has a built-in WebSocket API. It must be enabled with Developer Tools -> Allow remote debugger, or equivalent config. The API uses JSON messages over WebSocket with subprotocol `debugger.ppsspp.org`.

Useful events from PPSSPP source/samples:

- `version`: client/server handshake.
- `game.status`: current game/app status.
- `input.buttons.send`: alter button state.
- `input.buttons.press`: press/release a button for N frames.
- `input.analog.send`: set analog stick.
- `gpu.buffer.screenshot`: capture screenshot.
- `gpu.stats.get`: FPS/frame timing stats.
- `gpu.stats.feed`: frame timing stream.
- `memory.read`, `memory.readString`: inspect emulated memory.
- `cpu.getReg`: read CPU register.
- `cpu.breakpoint.add/remove/list`: manage breakpoints.
- `cpu.stepping`: breakpoint/step event.
- `hle.func.list`: list detected HLE/function symbols.
- `hle.backtrace`: stack frame list.
- log events are broadcast through logger channel.

PPSSPP API samples include Node.js client code for auto-connect, `game.status`, memory reads, and monitoring `sceIoOpen` by setting a breakpoint on the HLE stub.

### Screenshots

Preferred:

- Use `gpu.buffer.screenshot` through WebSocket.
- Save PNG/JPEG artifact locally.
- Use local image inspection to verify UI state.

Fallback:

- Run SDL/Qt PPSSPP inside Xvfb.
- Capture the emulator window with host screenshot tooling.

### Logs

Collect three log classes:

1. PPSSPP process stdout/stderr.
2. PPSSPP logger WebSocket events.
3. `glyph` app log written to emulated storage.

Recommended app log path:

```text
ef0:/PSP/GAME/glyph/logs/glyph.log
```

In PPSSPP on Linux/macOS, the memstick is usually under:

```text
~/.config/ppsspp/PSP/
```

For isolated tests, prefer a portable/configured memstick directory if the PPSSPP build supports it, or run with a temporary `HOME`/config root.

## Harness Design

### Directory Layout

Future repo structure:

```text
tools/harness/
  run_ppsspp_smoke.py
  ppsspp_ws.py
  configs/
    ppsspp-test.ini
  scenarios/
    open_fixture_book.json
test-artifacts/
  ppsspp/
    logs/
    screenshots/
```

### Isolated Memstick

Create:

```text
<tmp>/ppsspp/PSP/GAME/glyph/EBOOT.PBP
<tmp>/ppsspp/PSP/GAME/glyph/books/*.epub
<tmp>/ppsspp/PSP/GAME/glyph/saves/
<tmp>/ppsspp/PSP/GAME/glyph/cache/
<tmp>/ppsspp/PSP/GAME/glyph/logs/
<tmp>/ppsspp/PSP/SYSTEM/ppsspp.ini
```

For PSP Go path simulation, `glyph` should support both:

- real hardware `ef0:/`
- PPSSPP test path via `ms0:/` or an app-level test root override

Important: PPSSPP emulates normal memstick well. It may not faithfully expose PSP Go `ef0:/` for homebrew. Therefore `glyph` should have a storage abstraction and allow a test mode or fallback to `ms0:/` in PPSSPP.

### Test Config

Candidate PPSSPP config settings to set with `--appendconfig` or generated `PSP/SYSTEM/ppsspp.ini`:

```ini
[General]
Enable Logging = True
FileLogging = True
AutoRun = True
RemoteDebuggerOnStartup = True
RemoteDebuggerLocal = True
AskForExitConfirmationAfterSeconds = 0
PauseOnLostFocus = False
ScreenshotsAsPNG = True

[CPU]
CPUCore = 1
CPUSpeed = 0
FastMemoryAccess = True
IOTimingMethod = 0

[Graphics]
GraphicsBackend = OPENGL
InternalResolution = 1

[SystemParam]
PSPModel = 1
MemStickSize = 16
```

Notes:

- `PSPModel = 1` is PSP slim/64 MB class in PPSSPP source constants, not specifically PSP Go.
- PPSSPP docs say model choice generally does not change memory available to games.
- Use OpenGL first in CI/headless environments; Vulkan may be faster but less portable.

### Scenario Flow

Smoke test:

1. Build `EBOOT.PBP`.
2. Prepare temp memstick with `glyph` and fixture EPUB.
3. Launch PPSSPP with timeout.
4. Wait for WebSocket connection.
5. Send `version`.
6. Send `game.status`; verify `glyph` running.
7. Wait for app-ready marker:
   - screenshot contains file browser, or
   - app log line says `ready`, or
   - test-mode memory/status marker.
8. Send button sequence:
   - open fixture book.
   - page forward.
   - open TOC.
   - return.
   - open settings.
9. Capture screenshots at each checkpoint.
10. Collect logs.
11. Exit emulator.

### Input Driving

Use `input.buttons.press` for high-level actions:

```json
{"event":"input.buttons.press","button":"Cross","duration":2}
{"event":"input.buttons.press","button":"Down","duration":2}
{"event":"input.buttons.press","button":"R","duration":2}
```

Button names need confirmation against PPSSPP source before implementation. If names differ, use `input.buttons.send` with explicit flags from source.

### Failure Detection

Fail if:

- PPSSPP process exits non-zero.
- WebSocket never connects.
- `game.status` never reports app running.
- no frame/screenshot after timeout.
- screenshot is blank/black after startup timeout.
- PPSSPP logs contain error/assert/crash patterns.
- `glyph.log` contains fatal error.
- expected visual checkpoint missing.
- frame time/FPS collapses under threshold for static reader screen.

### Visual Quality Evaluation

Use screenshots for:

- blank screen detection.
- text overlap.
- clipped UI.
- unreadably small text.
- incorrect screen after input sequence.
- regression comparison against baseline screenshots.

The agent can inspect generated screenshot artifacts directly. For deterministic checks, add simple computer-vision rules first:

- nonblank pixel count.
- text area bounding boxes not overlapping chrome.
- minimum contrast between foreground/background.
- screenshot dimensions.

Then add human/agent visual review for polish.

## Limits

- PPSSPP is not a PSP Go.
- It may not expose `ef0:/` exactly like hardware.
- GPU timing, storage timing, cache behavior, and suspend/resume can differ.
- WebSocket API is sparsely documented; source comments are canonical.
- Headless visual capture may require building `PPSSPPHeadless` or running SDL under Xvfb.
- Real hardware remains required before release.

## Implementation Spike

M0 scope:

- Detect whether PPSSPP is installed.
- Run smoke command only if available.
- Document install/build hooks.
- Do not build PPSSPP from source in M0.

Recommended later engineering spike:

- Install/build PPSSPP in local environment.
- Confirm CLI launch of any known homebrew `EBOOT.PBP`.
- Enable WebSocket debugger from generated config.
- Write 50-line Python or Node client:
  - connect
  - `version`
  - `game.status`
  - `gpu.buffer.screenshot`
  - one button press
- Save screenshot artifact.
- Document exact working command.

## Sources

- PPSSPP command-line docs: https://www.ppsspp.org/docs/reference/command-line/
- PPSSPP WebSocket API docs: https://www.ppsspp.org/docs/reference/websocket-api/
- PPSSPP developer tools: https://www.ppsspp.org/docs/development/developer-tools/
- PPSSPP development tips, WebSocket test script and headless CI note: https://www.ppsspp.org/docs/development/tips-and-tricks/
- PPSSPP system settings and PSP model note: https://www.ppsspp.org/docs/settings/system/
- PPSSPP memory stick location FAQ: https://www.ppsspp.org/docs/faq/
- PPSSPP WebSocket source: https://github.com/hrydgard/ppsspp/tree/master/Core/Debugger/WebSocket
- PPSSPP API samples: https://github.com/unknownbrackets/ppsspp-api-samples/tree/master/js
