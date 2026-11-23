# Setup

How to get the K2 RFID tool running against your own printer and Spoolman. Hardware and pin details are in `CLAUDE.md` ("Hardware Notes").

## 1. Build and flash

1. Install [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).
2. Plug the board in using the **UART** USB-C port (the board has two).
3. Find its serial port: `pio device list`. On macOS it looks like `/dev/cu.usbserial-XXXX`, and on Linux like `/dev/ttyUSB0`.
4. Build and flash:
   ```bash
   pio run -t upload --upload-port <port>
   pio run -t uploadfs --upload-port <port>   # first install only: material database to LittleFS
   ```
   **Run `uploadfs` only on a fresh board.** It rewrites the whole LittleFS image, including the saved `config.json` (Wi-Fi, Spoolman URL, printer IP).
5. Watch the log: `pio device monitor --port <port>` (115200 baud).

To avoid typing the port each time, uncomment `upload_port` / `monitor_port` in `platformio.ini` locally. Don't commit that change.

## 2. First boot: Wi-Fi and addresses

With no saved Wi-Fi, the device opens a setup access point:

1. Connect a phone or laptop to Wi-Fi **`K2-RFID-SETUP`**.
2. The captive portal opens. If it doesn't, browse to `192.168.4.1`.
3. Fill in:
   - **Your Wi-Fi network** and password.
   - **Printer IP:** your K2 Plus's address (see step 3). It's used by *Update DB* to pull the printer's material database.
   - **Spoolman URL (blank = off):** e.g. `http://<spoolman-host>:7912`. Plain HTTP only; `https://` is reported as unsupported.

You can change both later without the portal. Over the serial console, `printer <ip>` and `url <value>` set and save them, and `printer` / `url` print the current values.

## 3. Find your printer

- **Give the printer a DHCP reservation** in your router, so its address doesn't change. The K2 Plus's MAC address is in the printer's network settings or your router's client list. A reservation only takes effect after the printer reconnects to Wi-Fi.
- **To find a printer whose address you don't know:** Moonraker listens on port **7125**. Scan your subnet, for example on macOS:
  ```bash
  for i in $(seq 1 254); do nc -z -G1 192.168.1.$i 7125 && echo "192.168.1.$i"; done
  ```
  Replace `192.168.1` with your own subnet.
- **Ports on the K2 Plus:** Moonraker API `:7125` (no auth on stock firmware) and Creality web UI `:4408`.
  - Read-only queries are safe during a print, e.g. `GET /printer/objects/query?box&filament_rack&print_stats` returns the CFS state per box `T1..T4`.
  - **Don't send gcode while `print_stats.state == printing`.**
  - `box.Tn.measuring_wheel` is a per-box filament odometer (~mm, negative = feed). Moonraker's file API serves `config/box.cfg` and `logs/klippy.log*`. The daily logs are ~100 MB, so stream them through `grep`.

## 4. Spoolman (optional)

The device looks spools up in [Spoolman](https://github.com/Donkie/Spoolman) by tag UID, and keeps an offline copy on the SD card. Tested against **v0.27.1** (SQLite backend, no auth).

- **If Spoolman runs on a host that is also your LAN's DNS server** (common with Home Assistant / Pi-hole setups), `.lan` names stop resolving whenever that host is down. When lookups fail, test by IP before assuming anything else is wrong, or put the IP in the Spoolman URL field.
- **Disposable test instance** for development (Docker):
  ```bash
  docker run -d --name spoolman-test -p 7912:8000 \
    -v "$PWD/.spoolman-test:/home/app/.local/share/spoolman" ghcr.io/donkie/spoolman:0.27.1
  ```
  Point the device at `http://<your-computer-lan-ip>:7912`. `docker pause spoolman-test` simulates a hung server (TCP accepts, no HTTP response).

## 5. Optional: battery fuel gauge

See `docs/hardware/battery-fuel-gauge.md` (MAX17048 on the I2C port).

## 6. Keep your own details local

Don't commit your IPs, hostnames, MAC addresses or serial port paths: this repo is public. Put them in a **`CLAUDE.local.md`** at the repo root. It's gitignored, and Claude Code loads it automatically alongside `CLAUDE.md`. Template:

```markdown
# CLAUDE.local.md: this install's environment (never commit)

## Environment / access
- Spoolman: http://<host>:7912/api/v1/ (<ip>), version <x>, auth: <none/...>
- Printer: hostname <...>, IP <...> (DHCP reservation: yes/no), MAC <...>
  - Moonraker :7125, web UI :4408, SSH: <how you log in>
- Known traps: <e.g. DNS host goes down, printer address changed>

## This machine
- Board serial port: <port>
```
