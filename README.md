# PhishLab ESP32

A captive-portal credential-capture tool for the ESP32, intended for security-awareness
demonstrations. It brings up an open Wi-Fi access point, hijacks DNS so the device's
captive-portal page opens automatically, and serves a fake Google login. Submitted
credentials are kept in memory and viewable on a PIN-protected panel.

## How it works

1. The ESP32 brings up an **open access point** (`Wifi Gratis`).
2. A DNS server answers **every** domain with the ESP32's IP, so the device's OS opens its
   captive-portal page automatically.
3. The user is shown a **fake Google login** page.
4. On submit, the credentials are stored and a response page is returned.
5. Captured entries are kept in **RAM only** and can be viewed on the PIN-protected
   `/creds` route.

## Hardware

- Any **ESP32** dev board (tested on a board with a CP2102 USB-UART bridge).
- A USB cable and a computer to flash it.

## Build & flash

This project uses [`arduino-cli`](https://arduino.github.io/arduino-cli/) and the
[ESP32Async](https://github.com/ESP32Async) libraries.

```bash
# 1. Install the ESP32 core (once)
arduino-cli config init
arduino-cli config add board_manager.additional_urls \
  https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# 2. Install the async libraries (use the ESP32Async forks — others won't compile on core 3.x)
arduino-cli lib install "Async TCP@3.4.10"
arduino-cli lib install "ESP Async WebServer@3.11.0"

# 3. Compile
arduino-cli compile --fqbn esp32:esp32:esp32 .

# 4. Flash (adjust the port for your system)
arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 .

# 5. (optional) Watch the serial log
arduino-cli monitor -p /dev/cu.usbserial-0001 -c baudrate=115200
```

> **Library note:** the libraries must be the **ESP32Async** forks
> (`Async TCP` + `ESP Async WebServer`). The older `AsyncTCP` (dvarrel) and
> `ESPAsyncWebServer` (lacamera) forks fail on ESP32 core 3.x with a
> `mbedtls_md5_starts_ret not declared` error.

## Usage

1. Flash the board and power it on.
2. Connect a device to the **`Wifi Gratis`** network.
3. The captive portal opens automatically with the fake Google login.
4. Browse to `http://8.8.8.8/creds`, enter the PIN, and review the captured entries.

## Configuration

Edit the constants at the top of [`phishlab-esp32.ino`](phishlab-esp32.ino):

| Constant       | Default       | Description                                              |
| -------------- | ------------- | -------------------------------------------------------- |
| `nomeDaRede`   | `Wifi Gratis` | SSID of the fake open network.                           |
| `senhaDaRede`  | `""` (empty)  | Network password. Empty = open network.                  |
| `ipDoPortal`   | `8.8.8.8`     | IP the access point and DNS hijack respond with.         |
| `pinDoPainel`  | `admin`       | PIN for the `/creds` panel. **Change this before use.**  |

> 🔑 The default PIN (`admin`) is a **public placeholder**. Always set your own PIN before
> use, then recompile and re-flash.

## Routes

| Route            | Method | Description                                            |
| ---------------- | ------ | ------------------------------------------------------ |
| `/`              | GET    | Fake Google login page.                                |
| `/`              | POST   | Stores the submitted entry, returns the response page. |
| `/creds`         | GET    | PIN prompt for the panel.                              |
| `/creds-verify`  | POST   | Validates the PIN and shows the captured entries.      |
| `/clear`         | POST   | Clears the in-memory list of entries.                  |

## Notes & limitations

- Entries live in **RAM only** and are lost on reboot.
- The DNS hijack catches most platforms; some devices cache DNS and may need Wi-Fi toggled.

## License

[MIT](LICENSE)
