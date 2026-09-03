# Changelog

Custom variant of the **Prusa ESP32 PrusaConnect Camera** firmware.

- **Upstream base:** `SW_VERSION 1.1.2` (from the Prusa3D GitHub repository).
  This value is left unchanged because it is reported to Prusa Connect and
  identifies the base firmware.
- **Variant identifier:** `FW_VARIANT_VERSION` in `mcu_cfg.h`. Bump the trailing
  number every time you make a change, and add an entry below.
- Target hardware: **Seeed XIAO ESP32-S3 Sense** with an **OV5640** camera module.

The variant string is shown at boot on the serial console and on the web
interface (System section, "Variant" row).

Version format: `1.1.2-xiao-ov5640.N`, where `1.1.2` is the upstream base and
`N` is the local revision.

---

## 1.1.2-xiao-ov5640.40 — 2026-08-15

### Changed — "Update from File" button disabled until a valid .bin is chosen
- The firmware-update button now starts disabled and only enables once a file
  ending in `.bin` is selected (`fwFileChanged()` on the file input's onchange).
  `uploadFile()` also guards up front, so it can no longer be run with no file
  (which previously threw on `file.size`).

## 1.1.2-xiao-ov5640.39 — 2026-08-15

### Fixed — SD files page still blank (v38 fix was insufficient)
- `AsyncResponseStream` still accumulates the whole body in RAM before sending,
  so a large/failed build produced a blank page (no buttons) just like before.
- Split the page: `/sd_files` is now a tiny STATIC shell that always renders
  (buttons incl. Delete ALL visible immediately), and the file list loads
  separately from a new capped `/sd_list` endpoint (plain text, max 500 entries,
  builds a bounded ~20 KB String). Even if the SD scan is slow or fails, the
  buttons are usable - so Delete ALL can clear a backlog that won't list.

## 1.1.2-xiao-ov5640.38 — 2026-08-15

### Fixed — SD files page hung with a large backlog
- `/sd_files` built the whole listing into one String; a folder with thousands
  of old stills exhausted the internal heap and the page never loaded. Now it
  streams the response (`beginResponseStream`) and caps the list at 1000 rows.

### Added — "Delete ALL" for the timelapse folder
- New button + `/sd_delete_all` route (POST + confirm) to clear the whole
  /timelapse backlog. Deletes in bounded batches of 200 per request and the page
  calls it repeatedly until "done", so a huge backlog can't block the server.

## 1.1.2-xiao-ov5640.37 — 2026-08-15

### Added — digital pan/tilt for the zoom (virtual PTZ)
- When zoomed in, the software crop window can now be panned/tilted within the
  frame instead of always being centred. `panPos`/`tiltPos` (0..100, 50 =
  centred) offset the crop in `ApplySoftwareZoom()`; no extra processing cost.
  At 1.0x there is no travel so it stays centred.
- `Camera::SetPan/SetTilt/GetPan/GetTilt`, route `/pantilt?pan=&tilt=`.
- Main page shows a draggable green box inside a frame rectangle: the box size
  tracks the zoom level, and dragging it pans/tilts the view (mouse + touch).
  Inert at 1.0x.
- Rotation-aware: pan/tilt are in DISPLAY space, mapped back through the EXIF
  image rotation (0/90/180/270) in `ApplySoftwareZoom()` so a rotated image pans
  the correct way (e.g. at 180deg the axes are flipped). H-mirror/V-flip need no
  compensation (baked into the frame the crop uses).

### Added — zoom/pan/tilt persist across reboots and in the preset
- Zoom, pan and tilt are now persisted to NVS (namespace "camptz") and restored
  at boot (`LoadPtzFromNvs()` in `LoadCameraCfgFromEeprom`, zoom re-clamped to the
  framesize max). Zoom persists on change; pan/tilt persist on drag-release only
  (`/pantilt?...&save=1`) to avoid flash wear during a drag. Preferences skips
  unchanged writes.
- Zoom/pan/tilt are also included in the Save/Load camera-settings preset, so a
  saved baseline restores the framing too. The page loads the persisted pan/tilt
  on open so the box shows the real position.

## 1.1.2-xiao-ov5640.36 — 2026-08-15

### Added — save/restore camera settings preset
- "Save current" / "Load last saved" buttons by the exposure controls, so after
  locking exposure and tuning brightness/contrast/saturation you can bank the
  setup and roll back to it. Stored in NVS flash (`Preferences`, namespace
  "campreset"), separate from the EEPROM config.
- `Camera::SaveSettingsPreset()` / `LoadSettingsPreset()` / `HasSettingsPreset()`;
  routes `/cam_save` and `/cam_load`. Preset covers brightness, contrast,
  saturation, h-mirror, v-flip, exposure ctrl, AWB + AWB gain + WB mode, BPC,
  WPC, raw gamma, AEC2, AE level, AEC value, gain ctrl, AGC gain. Load also
  writes them to the normal config so the restored preset survives a reboot.
- Note: brightness/contrast/saturation already tune correctly on top of a locked
  exposure (adjusting them re-applies without releasing the lock).

### Added — fine brightness/contrast/saturation (direct ISP registers)
- The driver only exposes -2..2 for these (very coarse). Now the sliders write
  the OV5640 ISP magnitude registers directly for fine control - hardware, so no
  per-frame CPU cost: brightness 0x5587/0x5588, contrast 0x5585/0x5586,
  saturation 0x5583/0x5584. A coarse driver call is still made first to keep the
  SDE block enabled, then the fine value overwrites it in `ApplyCameraCfg()`.
- Slider ranges widened: brightness/saturation -64..64 (step 2), contrast
  -24..24 (step 1); neutral = 0. Old -2..2 stored values now read as near-neutral,
  so re-tune once. Values still fit the existing int8 config/preset storage.

## 1.1.2-xiao-ov5640.35 — 2026-08-15

### Fixed — web UI extremely slow (full-FAT scan on every SD log write)
- `isCardCorrupted()` ran `SD_MMC.usedBytes()` (a full FAT walk) on every SD log
  append. With verbose logging on, every web request writes to the SD log, so
  the async web server stalled on repeated FAT scans (seconds each on a real
  card, worse at 10 MHz) while Prusa Connect's capture task stayed fast.
- Replaced with a cheap `cardType()` presence check (no bus scan). Free space is
  still tracked by `CheckCardUsedStatus()`; a full card / write error is still
  caught by `file.getWriteError()` in `AppendFile()`.
- Tip: keep the log level at Info for normal use - Verbose writes every web
  request to the SD card (both slow and extra card wear).

## 1.1.2-xiao-ov5640.34 — 2026-08-15

### Changed — SD clock lowered to 10 MHz (write reliability)
- `SD_INIT_FREQ_KHZ` 20000 -> 10000. The XIAO Sense runs the SD in 1-bit mode
  over the camera FPC connector, where 20 MHz is marginal and can corrupt
  writes. 10 MHz is slower but plenty for timelapse-sized writes and should
  improve reliability. The 400 kHz mount fallback is unchanged. (Card quality
  and power supply remain the other two suspects for repeated corruption.)

## 1.1.2-xiao-ov5640.33 — 2026-08-15

### Added — SD file manager: multi-select download & delete
- The SD files page (`/sd_files`) now has a checkbox per file plus a header
  "select all", and "Download selected" / "Delete selected" buttons.
- Download selected triggers a sequential download of each ticked file.
- Delete selected posts to the new `/sd_delete` route (POST + `confirm=yes`,
  auth-protected), which deletes the ticked files from the timelapse folder
  (basename-only, rejects path separators / `..`). Page reloads after.
- Main-page button relabelled "SD card photos" -> "SD card files" (it lists
  both photos and `.avi` videos, which share the /timelapse folder).

## 1.1.2-xiao-ov5640.32 — 2026-08-15

### Added — "Lock exposure to current" (stops AEC hunting)
- On a fixed scene the OV5640's auto-exposure/auto-gain hunt, so the picture
  swings between washed-out-bright and dark-high-contrast. New one-click lock
  reads the exposure the AEC has currently settled on (registers 0x3500..0x3502)
  and the current gain, pins them, and turns OFF auto-exposure and auto-gain so
  nothing keeps re-metering - it freezes at a *good* exposure with no jump.
- `Camera::LockExposureToCurrent()`, `/lock_exposure` route, and a "Lock exposure
  to current" button beside the exposure controls on the config page. White
  balance is left on auto (it wasn't the cause). Re-enable the "Automatic
  Exposure/Gain Control" switches to resume auto metering.

## 1.1.2-xiao-ov5640.31 — 2026-08-15

### Added — SD card corruption recovery (no Windows needed)
- FAT on a cheap card can be left corrupt if power is cut mid-write, after which
  the card is unreadable until reformatted. The device now detects this and lets
  you recover it from the browser instead of pulling the card and using Windows.
- `MicroSd::FormatCard()` reformats the card to FAT32 by mounting with
  `format_if_mount_failed` (a card that already mounts cleanly is left untouched,
  so this can't wipe a healthy card). New `/sd_format` route (POST + `confirm=yes`,
  auth-protected) and a "Recover / Reformat SD" button on the System page.
- Boot now distinguishes "card present but filesystem corrupt" (recoverable)
  from "no card / not responding", logs it clearly, and the System page card
  status shows "Corrupt - use Recover/Reformat below" via `GetCardMountFailed()`.
- Note: write durability was already sound (AVI flushes per frame, photos
  open/write/close, log file flushes per line); this adds recovery, which is the
  practical fix for power-loss FAT damage.

## 1.1.2-xiao-ov5640.30 — 2026-08-15

### Added — zoom limited by framesize (safety)
- Max digital zoom is now capped per framesize so the centre crop never falls
  below `ZOOM_MIN_OUTPUT_WIDTH` x `ZOOM_MIN_OUTPUT_HEIGHT` (640x480). Bigger
  framesize -> more zoom (2592->4x, 1600->2.5x, 1920x1080->2.25x, 800->1.25x,
  640 and below -> no zoom). New `Camera::GetMaxZoom()`; `SetZoom()` clamps to
  it and `SetFrameSize()` re-clamps the stored zoom.
- `/zoom` now returns `current,max` (percent). The web slider sets its `max`
  from this and shows the ceiling (e.g. "2.0x (max 2.5x)"), so you can't drag
  the zoom past the point where the crop would be too small.

## 1.1.2-xiao-ov5640.29 — 2026-08-15

### Fixed — software zoom too dark
- The RGB565 re-encode gave correct hue but a very dark image (the library's
  RGB565 path under-scales the 5/6-bit channels). Switched the re-encode to
  RGB888: expand each decoded RGB565 channel to full 8-bit with bit replication
  (0x1F -> 0xFF) and encode as RGB888 (B,G,R order, the esp32-camera memory
  layout). Correct brightness and colour, no library RGB565 ambiguity.

## 1.1.2-xiao-ov5640.28 — 2026-08-15

### Fixed — software zoom colours scrambled (psychedelic false colour)
- `jpg2rgb565` writes native little-endian RGB565, but `fmt2jpg` defaults to
  big-endian (camera-hardware order), so re-encoding scrambled the R/G/B bits.
  Wrap the re-encode in `jpgSetRgb565BE(false)` / `jpgSetRgb565BE(true)` so the
  encoder reads the decoded buffer in the correct byte order.

## 1.1.2-xiao-ov5640.27 — 2026-08-15

### Changed — digital zoom is now done in software
- Replaced the OV5640 ISP-windowing zoom (`set_res_raw`) with a pixel-domain
  centre crop. The sensor always outputs its full framesize (normal, correct
  white balance), and `ApplySoftwareZoom()` decodes each captured JPEG, copies
  out the centred `100/zoom` crop and re-encodes it. This eliminates the magenta
  colour cast completely — the crop uses the sensor's normal full-frame colour.
- Trade-off: a zoomed frame is a lower-resolution 1:1 centre crop (no upscaling),
  so the JPEG dimensions shrink with zoom. For large framesizes the decode is
  done at a reduced JPEG scale so the RGB buffer fits in PSRAM.
- Applied to both the still-photo path and the live stream. Re-encoded frame is
  written back within the original buffer length (quality is reduced if needed),
  so the driver frame buffer is never overrun.

## 1.1.2-xiao-ov5640.26 — 2026-08-15

### Reverted — zoom binning
- `.25`'s `binning=true` produced a garbled dark image, so reverted to
  `binning=false` (zoom works but with the magenta cast). Quick fixes for the
  cast are exhausted (AWB re-apply, ISP reorder, binning); a clean fix needs
  replicating the OV5640 driver's per-mode `set_res_raw` register math for a
  cropped window - deferred pending a decision on whether it's worth it.

## 1.1.2-xiao-ov5640.25 — 2026-08-15

### Changed — zoom colour (attempt 3: binning)
- The reorder in `.24` didn't clear the magenta cast, so it's the scaler, not
  white balance. Switched the zoom `set_res_raw` to `binning=true` (2x2, as the
  driver uses for downscaled modes) - reading the window without binning
  mishandles the Bayer green pixels and gives the magenta tint. The zoom window
  is now clamped to >= 2x the output so the binned readout still covers it
  (slightly reduces max zoom at a given resolution). Testing.

## 1.1.2-xiao-ov5640.24 — 2026-08-15

### Fixed — zoom colour cast (attempt 2)
- The `.22` fix (re-applying AWB after `set_res_raw`) wasn't enough - the
  windowing resets more ISP state than just white balance. Reordered so the
  zoom window is set FIRST (`ApplyZoomWindow()` at the start of
  `ApplyCameraCfg()`), then the full sensor/ISP config is applied on top. Zoom
  now re-runs the whole `ApplyCameraCfg()`. If a cast still remains it's the
  OV5640 ISP scaler itself (fixed offset/total values), which is the next thing
  to tune.

## 1.1.2-xiao-ov5640.23 — 2026-08-14

### Changed — local-file OTA only (GitHub UI removed)
- The **Update from File** OTA (upload a compiled `.bin` via the web) was always
  functional and independent of the disabled GitHub cloud update. Cleaned up the
  System page: removed the GitHub-only controls ("Available from cloud",
  "Check for Updates", "Update from Cloud"), leaving the file picker +
  **Update from File**, with a hint to pick `ESP32_PrusaConnectCam.ino.bin`
  (Arduino: Sketch -> Export Compiled Binary). GitHub cloud update stays off
  (`ENABLE_OTA_CLOUD_UPDATE false`). After a file update the UI says "Please
  reboot MCU" - reboot to run the new firmware.

## 1.1.2-xiao-ov5640.22 — 2026-08-14

### Fixed — zoom colour cast
- Digital zoom gave a magenta/purple tinge: changing the OV5640 readout window
  (`set_res_raw`) disturbs the ISP auto white balance and the gains don't
  re-converge. `Camera::ApplyZoom()` now re-applies the colour/AWB config
  (whitebal, awb_gain, wb_mode, brightness, contrast, saturation, dcw) after
  the window change, so AWB restarts and the cast clears.

### Fixed — stale web pages after firmware update
- HTML pages are now served `no-cache` (they change with the firmware), instead
  of the 24h `max-age` that made updated pages/buttons not appear until the
  browser cache expired or a manual hard-refresh. Static assets (JS/CSS/icons)
  keep the long cache. (One last cache-clear/hard-refresh is needed to pick up
  this build; after that, updates show immediately.)

## 1.1.2-xiao-ov5640.21 — 2026-08-14

### Added — digital zoom (OV5640) - FIRST CUT
- Digital zoom slider (1.0x-4.0x) on the main page. Uses the OV5640 ISP
  windowing (`sensor->set_res_raw`) to read a centered sub-window of the array
  and scale it to the output resolution - done in the sensor, full frame rate,
  no crop/re-encode. `Camera::SetZoom()`/`ApplyZoom()`; web route
  `/zoom?level=100..400`. Re-applied automatically after resolution changes.
- **First cut / needs on-hardware tuning:** the 4:3 base window + timing values
  are the OV5640 driver's, and to avoid ISP upscaling the crop window is
  clamped to >= the output size, so **higher zoom requires a lower resolution**
  (e.g. big zoom works at VGA, little/none at UXGA). If the image comes out
  shifted or distorted, the window/offset/total numbers in `Camera::ApplyZoom()`
  are what we tune. Best on 4:3 resolutions (FHD 16:9 may distort).

## 1.1.2-xiao-ov5640.20 — 2026-08-14

### Added — PrusaLink printer IP + API key in the web UI (EEPROM-backed)
- **Printer IP** and **API key** fields on the main page (`/pl_config` get/set).
  Stored in EEPROM (2 new fields) so the device polls autonomously without a
  browser. IP defaults to `PRUSALINK_DEFAULT_IP`; key has no
  default and is **masked** in the UI (password field; only overwritten when a
  new value is typed). Removed the compile-time IP/key defines.
- Status shows `no api key` until a key is entered.

## 1.1.2-xiao-ov5640.19 — 2026-08-14

### Added
- **"Auto timelapse on print" toggle** on the main web page (switch under the
  printer status). Runtime enable/disable of the PrusaLink auto start/stop,
  no recompile needed. Default from `PRUSALINK_AUTO_TIMELAPSE`. Web route
  `/pl_auto?on|off`; state shown in `/prusalink_status` (`[auto-TL ON/OFF]`).
  Disabling mid-print still finalizes an already auto-started recording.

## 1.1.2-xiao-ov5640.18 — 2026-08-14

### Added — PrusaLink print-state polling + auto timelapse
- New `prusalink.cpp`/`prusalink.h`: polls the printer's local PrusaLink v1 API
  (`GET /api/v1/status` with `X-Api-Key`) every `PRUSALINK_POLL_INTERVAL_S` and
  interprets `printer.state`. Targets Prusa Core One / Buddy-firmware printers.
- When the printer starts a print (`PRINTING`/`PAUSED`), the MJPEG-AVI timelapse
  **auto-starts**; when the print ends it **auto-stops/finalizes** the file.
  Only auto-stops recordings it auto-started (manual recordings are left alone).
- Printer state is shown on the main web page ("Printer (PrusaLink)") and via
  `/prusalink_status`.
- Config in `mcu_cfg.h`: `ENABLE_PRUSALINK`, `PRUSALINK_PRINTER_IP`,
  `PRUSALINK_API_KEY`, `PRUSALINK_POLL_INTERVAL_S`, `PRUSALINK_AUTO_TIMELAPSE`.
  **Set the printer IP + API key** (printer: Settings -> Network -> PrusaLink).
- Poll runs in `System_Main()` (rate-limited); plain HTTP, no TLS.

## 1.1.2-xiao-ov5640.17 — 2026-08-14

### Added — on-device MJPEG-AVI timelapse video
- New `avi.cpp`/`avi.h` `AviRecorder`: wraps the camera's JPEG frames into a
  standard MJPEG-AVI on the SD card (plays in VLC/most players). The header is
  patched after every frame, so the file stays playable even if interrupted.
- **Start/Stop TL video** buttons on the main page (with live status). One frame
  is appended per photo trigger; played back at `TIMELAPS_VIDEO_FPS` (default
  10) so a slow trigger interval becomes a fast timelapse.
- File is saved to the timelapse folder as `tl_<timestamp>.avi` and appears in
  the **SD card photos** list, downloadable via the browser (`/sd_get` serves
  `.avi` as `video/x-msvideo`).
- Web route `/tl_video?start|stop|status`. Config: `ENABLE_TIMELAPSE_VIDEO`,
  `TIMELAPS_VIDEO_FPS`, `TIMELAPS_VIDEO_PREFIX/SUFFIX` in `mcu_cfg.h`.
- Delivery is SD-download only (no SMB/FTP) - per the design choice; ESP32
  can't practically write to a Windows share, and this keeps it server-free.

## 1.1.2-xiao-ov5640.16 — 2026-08-14

### Added — browse/download SD photos from the web UI
- New **"SD card photos"** button (next to Download snapshot) opens `/sd_files`,
  a listing of the timelapse folder on the SD card with a download link and
  size for each file.
- New `/sd_get?path=...` route downloads a file as an attachment. Restricted to
  the timelapse folder with a path-traversal guard.

## 1.1.2-xiao-ov5640.15 — 2026-08-14

### Changed
- Minimum trigger (photo refresh) interval lowered from 10 s to **1 s**
  (`REFRESH_INTERVAL_MIN` in `mcu_cfg.h`).
- The per-photo timelapse SD-save messages ("Save TimeLaps photo to SD card",
  "Photo saved to SD card[. EXIF]", "Create folder for TimeLaps photos") are
  now routine messages (`AddEventRoutine`), so they're suppressed by default
  like the other periodic operational logs. SD-save **errors** still log
  normally.

### Note
- The SD-card init problem also occurred with OV3660 and OV2640 modules, i.e.
  it was never OV5640-specific - consistent with the root cause being the
  task-stack / GPIO21 issues rather than the camera. Filed upstream as
  https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam/issues/145

## 1.1.2-xiao-ov5640.14 — 2026-08-14

### Fixed — task stack overflow (crash)
- The `CheckMicroSdCard` task crashed with a stack-canary/stack-overflow panic
  ("Stack canary watchpoint triggered (CheckMicroSdCar)"). Several FreeRTOS
  tasks had very small stacks (2000-3000 bytes); `AddEvent` (RTC clock read +
  String building + boot/error-log capture) needs more, so they overflowed and
  corrupted the heap - which can also wedge the SDMMC driver.
- Increased task stacks: CheckMicroSdCard 3000->5120, CaptureAndSendPhoto
  4400->6144, SystemNtpOtaUpdate 5200->6144, WiFiManagement 2700->4096,
  SerialCfg 2300->4096, Telemetry 2200->4096, WiFiWatchdog 2200->4096,
  SysLed 2000->3072 (~13 KB more RAM, plenty free).
- Reported upstream: https://github.com/prusa3d/Prusa-Firmware-ESP32-Cam/issues/145
  (task-stack overflow + XIAO GPIO21 LED/SD-CS pin overlap).

## 1.1.2-xiao-ov5640.13 — 2026-08-14

### Fixed — SD card (real root cause: early-boot timing)
- An isolated test sketch proved the card, pins and 1-bit SD_MMC all work
  perfectly (mounts at 20 MHz / 10 MHz / 400 kHz and in SPI mode, cardType=3,
  7680 MB) — but only when accessed a few seconds after boot. The firmware was
  mounting ~100 ms into boot with only 3 quick retries (all done by ~600 ms),
  and the card is not ready that early in this path.
- Fix: added an 800 ms settle delay before the first mount, and retry up to 10
  times over a several-second window (matching the known-good test), tearing
  down (`SD_MMC.end()`) and re-asserting pins before every attempt. New knobs
  in `mcu_cfg.h`: `SD_INIT_SETTLE_MS`, `SD_INIT_RETRY_DELAY_MS`,
  `SD_INIT_RETRY_COUNT`, `SD_INIT_HIGHFREQ_ATTEMPTS`.
- Note: the earlier GPIO21/LED fix (`.12`) is still correct and retained (that
  pin really is shared with the SD CS), it just wasn't the blocker on its own.

## 1.1.2-xiao-ov5640.12 — 2026-08-14

### Fixed — SD card (root cause found)
- **The status LED and the SD card share GPIO21 on the XIAO ESP32-S3 Sense.**
  GPIO21 is both the on-board user LED and the microSD chip-select (DAT3). The
  firmware blinked it as the status LED, holding the SD CS line and preventing
  the card from initializing (`cardType=0`) - on any card, on any board. The
  camera was unaffected because it never uses GPIO21.
- Fix: `sys_led` now honors `STATUS_LED_ENABLE`; when disabled it releases the
  pin as `INPUT_PULLUP` (held high) and never drives it. Set
  `STATUS_LED_ENABLE false` for the XIAO board, so GPIO21 stays high and the SD
  card works. Trade-off: no blinking status LED on the XIAO (it's physically
  the SD CS pin). Set it back to true only if you don't need the SD card.
- This is a documented XIAO ESP32-S3 Sense hardware conflict, not a card fault.

## 1.1.2-xiao-ov5640.11 — 2026-08-14

### Changed
- SD-init diagnostics now go through the logging system (`SystemLog.AddEvent`)
  instead of raw `Serial`, so the per-attempt mount messages, clock, and
  `cardType` are captured in the **boot log** and retrievable after boot via
  `bootlog;` / `/get_bootlog` (they used to scroll past before the serial
  monitor could catch them). Also logs the active SD pin map.

## 1.1.2-xiao-ov5640.10 — 2026-08-14

### Changed
- **Disabled the GitHub OTA cloud update** (this is bespoke firmware; checking
  the Prusa repo and flashing stock firmware over it is unwanted). New flag
  `ENABLE_OTA_CLOUD_UPDATE` (default false) guards `System_CheckNewVersion()`
  and `System_OtaCloudUpdate()`, so no GitHub contact happens on boot or via
  the web "Check for Updates" button. Local "Update from File" is unaffected.
- **SD init diagnostics**: confirmed the XIAO ESP32-S3 SD pins are correct
  (board variant: SCK=7/MOSI=9/MISO=8 -> SD_MMC CLK=7/CMD=9/D0=8). Refined the
  mount: 100 ms settle delay, no redundant `end()` before the first attempt,
  pins re-asserted between retries, and `cardType()` logged on failure to
  distinguish a bus/clock problem (CARD_NONE) from a FAT mount problem.

## 1.1.2-xiao-ov5640.9 — 2026-08-14

### Changed — more robust micro-SD init
- `MicroSd::InitSdCard()` now retries the mount and falls back to a slower,
  more reliable SDMMC clock (the XIAO ESP32-S3 Sense SD slot is frequently
  unreliable at the 20 MHz default). Configurable via `SD_INIT_RETRY_COUNT`,
  `SD_INIT_FREQ_KHZ`, `SD_INIT_FREQ_FALLBACK_KHZ` in `mcu_cfg.h`.
- Each attempt (and the clock used) is logged; on total failure the message
  now points at the two most common causes: card not seated, or not formatted
  FAT32 (cards >32 GB usually ship exFAT and must be reformatted).
- Optional `SD_INIT_FORMAT_IF_FAILED` (default false, **destructive**) to
  auto-format the card to FAT32 on mount failure.

## 1.1.2-xiao-ov5640.8 — 2026-08-14

### Added — autofocus verification / defocus test
- The **Release AF** button is now **"Relax lens (defocus)"** and sends the AF
  firmware's real release command (`0x3022 = 0x08`) via the ACK handshake,
  relaxing the VCM so a close subject visibly blurs. Pressing **Autofocus**
  then snaps it back sharp — a clear way to confirm AF is working. A short
  hint describing this test was added under the AF buttons.
- **Pause AF** now sends the real pause command (`0x3022 = 0x06`) instead of
  bare manual mode.
- After any AF button press the web UI polls `/af_status` ~8×/3 s so the
  `focusing -> focused` transition is visible in the status readout.
- Note: arbitrary manual VCM positioning is intentionally not implemented —
  neither Espressif's driver nor the popular ESP32 OV5640 libraries expose it
  safely, so only the firmware's documented release/pause commands are used.

## 1.1.2-xiao-ov5640.7 — 2026-08-14

### Fixed — autofocus now uses the correct firmware
- **Root cause of the AF failure: the firmware blob was corrupted.** The blob
  carried over from the reference project was 4332 bytes with a stray `0x80`
  byte inserted roughly every 16 bytes; the real OV5640 AF firmware is 4077
  bytes. The first bytes matched (so the SRAM read-back looked correct) but the
  8051 loaded garbage and hung at status `0x7F`.
- Replaced the blob with Espressif's authoritative firmware
  (`espressif/esp32-camera` `sensors/private_include/ov5640_af_firmware.h`,
  4077 bytes) and re-ported the load / command sequence faithfully from their
  `sensors/ov5640_af.c`:
  - Firmware download uses no clock-register manipulation (the earlier
    `0x3004` clock-enable guess was unnecessary and has been removed).
  - Single / continuous focus use Espressif's exact command handshake
    (`0x3022`/`0x3023` ACK protocol); pause/release set manual mode.
- The library's `esp_camera_af_*` API was confirmed to be compiled out as
  7-8 byte stubs (`CONFIG_CAMERA_AF_SUPPORT` off), so driving the sensor
  directly is required.

## 1.1.2-xiao-ov5640.6 — 2026-08-14

### Added — nuanced log routing
- **Boot log**: all messages are captured to an in-RAM buffer until boot
  finishes (`MarkBootComplete()` at the end of `setup()`), so the startup /
  autofocus messages can be read without the runtime flood. View via serial
  `bootlog;` or web `/get_bootlog` (button "Boot log" in the System section).
- **Error log**: errors and warnings are kept in a size-capped ring buffer.
  View via serial `errorlog;` / web `/get_errorlog` (button "Error log");
  clear via serial `clearerrorlog;`.
- **Routine message switch**: the high-frequency operational messages (photo
  capture, upload cycle, telemetry, periodic WiFi status) are now logged via
  `AddEventRoutine()` and can be turned on/off at runtime — serial
  `routinelog:0/1;`, web `/routine_log?on|off`, or the "Routine log messages"
  toggle in the System section. Default is set by `LOG_ROUTINE_MESSAGES_DEFAULT`
  in `mcu_cfg.h` (**default: off**, so boot/error logs stay readable).
  Buffer sizes: `LOG_BOOT_BUFFER_MAX`, `LOG_ERROR_BUFFER_MAX`.

## 1.1.2-xiao-ov5640.5 — 2026-08-14

### Fixed / Changed
- The bundled esp32-camera library (core 3.3.11) is built with
  `CONFIG_CAMERA_AF_SUPPORT` **disabled**, so `esp_camera_af_is_supported()`
  always returns false ("Sensor does not support autofocus") even though the
  OV5640 is detected. The native driver AF path is therefore unusable here.
- Reverted to a manual AF firmware upload (via `sensor->set_reg`/`get_reg`), now
  with the missing pieces the driver would otherwise handle:
  - Explicitly enable the AF MCU clock + program-memory clock
    (reg 0x3004 bits 6:5) — the earlier `0x7F` "stuck in startup" was the MCU
    never being clocked.
  - Read back the first SRAM bytes after upload and log them vs expected, to
    verify the firmware actually lands in the sensor (diagnostic).
  - Extended the idle-wait to 5 s; AF status now also reports "startup".

## 1.1.2-xiao-ov5640.4 — 2026-08-14

### Fixed
- **Autofocus firmware load timed out (`0x3029 = 0x7F`, stuck in startup).**
  The hand-rolled firmware upload never reached the idle state. Discovered the
  bundled esp32-camera driver (core 3.3.11) already ships a native, tested
  OV5640 autofocus implementation (`esp_camera_af.h`: `esp_camera_af_init`,
  `esp_camera_af_trigger`, `esp_camera_af_get_status`, ...).

### Changed
- Rewrote `ov5640_af.cpp` as a thin wrapper over the driver's `esp_camera_af_*`
  API. Removed the embedded ~4.3 KB AF firmware blob and all manual SCCB
  register/clock handling (the driver does the firmware download and command
  sequence itself). `OV5640_LoadAfFirmware()` now just calls
  `esp_camera_af_init()`; support is detected via `esp_camera_af_is_supported()`.
  - Single = `esp_camera_af_trigger`; Continuous = AUTO mode;
    Pause = MANUAL mode; Release = manual position / AF off.

## 1.1.2-xiao-ov5640.3 — 2026-08-14

### Fixed
- **Autofocus reported "unavailable".** The AF module talked to the OV5640 over
  a separate Arduino `Wire` (I2C) bus, which conflicts with the SCCB bus the
  esp_camera driver owns on the ESP32-S3 — the firmware upload silently failed
  and the status poll timed out. Rewrote `ov5640_af.cpp` to access AF registers
  through the camera driver (`sensor->set_reg` / `get_reg`), the same channel
  used for streaming. Removed `OV5640_InitBus()` / `Wire` usage;
  `OV5640_LoadAfFirmware()` now takes the `sensor_t*` handle.
- Added diagnostics: the detected sensor PID and the AF status register value on
  a load timeout are now logged.

## 1.1.2-xiao-ov5640.2 — 2026-08-14

### Added
- Higher OV5640 capture resolutions in the web UI resolution dropdown:
  - `7` = FHD 1920x1080
  - `8` = QXGA 2048x1536
  - `9` = QSXGA 2592x1944 (5 MP, full sensor)
  - Mapped in `Camera::TransformFrameSizeDataType()`, `GetFrameSizeWidth()`,
    `GetFrameSizeHeight()` (`camera.cpp`) and the dropdown in `WebPage.h`.
- Firmware variant identification (`FW_VARIANT`, `FW_VARIANT_VERSION` in
  `mcu_cfg.h`); surfaced at boot (serial) and in the web System info.
- This CHANGELOG.md.

## 1.1.2-xiao-ov5640.1 — 2026-08-14

Initial custom variant, based on upstream 1.1.2.

### Added
- **OV5640 autofocus support** (`ov5640_af.h` / `ov5640_af.cpp`): uploads the
  on-sensor 8051 AF firmware over SCCB after camera init and exposes
  single / continuous / pause / release / status commands.
  - Enabled via `ENABLE_OV5640_AUTOFOCUS` in `mcu_cfg.h`.
  - Firmware (re)loaded on boot and after each camera re-init
    (`Camera::InitAutofocus()` in `camera.cpp`).
  - Web routes `/af_single`, `/af_continuous`, `/af_pause`, `/af_release`,
    `/af_status` (`WebServer.cpp`).
  - Autofocus buttons and live status readout on the main web page
    (`WebPage.h`).
  - XCLK raised to 16 MHz when autofocus is enabled (OV5640 recommendation).
- **Boot directly as a WiFi client (STA) with DHCP — no configuration AP.**
  - `WIFI_FORCE_STA_CREDENTIALS` / `WIFI_FORCE_STA_SSID` / `WIFI_FORCE_STA_PASS`
    in `mcu_cfg.h` (set to your own network before building).
  - Credentials are provisioned into EEPROM on boot if they differ, the WiFi
    config flag is set, and the service AP is disabled
    (`WiFiMngt::LoadCfgFromEeprom()` in `wifi_mngt.cpp`).
  - Factory default `FACTORY_CFG_ENABLE_SERVICE_AP` changed to `0`.

### Notes
- The OV5640 is auto-detected by the esp32-camera driver (PID 0x5640); the
  XIAO Sense pin map was already correct.
- To revert to upstream AP-based WiFi provisioning, set
  `WIFI_FORCE_STA_CREDENTIALS false`. To disable autofocus, set
  `ENABLE_OV5640_AUTOFOCUS false`.

---

## Template for new entries

```
## 1.1.2-xiao-ov5640.N — YYYY-MM-DD

### Added / Changed / Fixed / Removed
- ...
```
