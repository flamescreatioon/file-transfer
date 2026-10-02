# AirBridge

AirBridge is a local-network file and clipboard sharing tool. It connects a native desktop service (Windows or Linux) with an Android Flutter app, discovers nearby devices on the same LAN, and transfers data directly between them.

The desktop app also starts a browser-based dashboard for sending files and clipboard text from a computer.

> [!WARNING]
> AirBridge currently uses unauthenticated, unencrypted HTTP. Use it only on a private network that you trust. Anyone who can reach TCP port `53536` may be able to send clipboard text, access the dashboard, upload files, or download received files.

## Features

- Automatic peer discovery over the local network
- Manual connection by IP address when discovery is unavailable
- Direct file transfer with progress reporting
- Clipboard-text sharing and a local history of the last 10 received items
- Desktop browser dashboard for selecting peers, uploading files, and downloading received files
- Duplicate-file handling (`file (1).ext`, `file (2).ext`, …)
- Filename sanitization and cleanup of incomplete received files
- Android confirmation prompt before accepting an incoming file

## Architecture

```text
                         UDP broadcast + multicast
                     port 53535, every 2 seconds
       ┌────────────────────────────┬────────────────────────────┐
       │                            │                            │
┌──────▼──────┐              ┌──────▼──────┐              ┌──────▼──────┐
│ Desktop app │◄────────────►│ Android app │◄────────────►│ Other peer  │
│ C++ server  │  HTTP :53536 │ Flutter app │  HTTP :53536 │              │
└──────┬──────┘              └─────────────┘              └─────────────┘
       │
       └── opens a local web dashboard at http://localhost:53536
```

Desktop received files are stored in `desktop/received_files/`. Android received files are written to the external-storage directory supplied by Android, falling back to the app documents directory if necessary.

## Repository layout

```text
.
├── desktop/                  # Native C++ desktop server and embedded web UI
│   ├── main.cpp              # Discovery, HTTP server, transfers, clipboard support
│   ├── protocol.hpp          # Socket helpers and shared ports
│   ├── web_assets.hpp        # Embedded dashboard HTML/CSS/JavaScript
│   ├── compile.bat           # Windows build script
│   └── compile.sh            # Linux build script
└── mobile/                   # Flutter Android client
    ├── lib/services/         # Discovery, HTTP service, transfer state
    ├── lib/views/            # Mobile UI and file-acceptance dialog
    └── android/              # Android project configuration
```

## Requirements

### Desktop

- Windows with MinGW g++ or LLVM clang++, **or** Linux with g++ or clang++
- A browser for the desktop dashboard
- On Linux, `xclip` is required if you want incoming text copied to the system clipboard

### Android

- Flutter SDK compatible with Dart `^3.11.5`
- Android device or emulator with LAN access
- Android build tooling configured through Flutter

Both devices must be connected to the same local network. Client isolation, guest Wi-Fi, VPNs, or host firewalls can prevent discovery and transfers.

## Run the desktop service

### Windows

From the repository root:

```powershell
cd desktop
.\compile.bat
.\airbridge.exe
```

The build script requires either `g++` or `clang++` on `PATH`. When the service starts, it displays its local IP address and opens the dashboard at `http://localhost:53536`.

If Windows Firewall asks for permission, allow the app on **Private networks** so the phone can reach it.

### Linux

```bash
cd desktop
chmod +x compile.sh
./compile.sh
./airbridge
```

Install a compiler if needed, for example on Debian/Ubuntu:

```bash
sudo apt install build-essential
```

## Run the Android app

```bash
cd mobile
flutter pub get
flutter run
```

On first launch, allow the storage/media permissions requested by the app. AirBridge starts discovery and its local HTTP service automatically; the control in the top-right corner can stop or start those services.

To create a release APK:

```bash
cd mobile
flutter build apk --release
```

The Android application ID is `com.airbridge.airbridge` and the current app version is `1.0.0+1`.

## How to use it

1. Start AirBridge on the desktop and Android device.
2. Wait for the other device to appear in the discovered-devices list.
3. If it does not appear, use **Connect to Device by IP** in the Android app and enter the desktop IP shown in its console.
4. Select the peer:
   - On desktop, select it in the browser dashboard, then choose a file or send clipboard text.
   - On Android, tap it, then choose **Send File** or **Send Clipboard Text**.
5. Confirm inbound file requests on Android when prompted.

Received desktop files can be downloaded from the dashboard’s **Received Files** list.

## Network protocol and API

AirBridge advertises peer availability with the following UDP payload on port `53535`:

```text
AIRB_DISCOVER:<device-name>:<os>:53536
```

Each client runs an HTTP server on TCP port `53536`. Requests carrying `X-Device-Name` and `X-Device-OS` also register or refresh the requesting peer.

| Method | Endpoint | Purpose |
| --- | --- | --- |
| `GET` | `/` | Embedded desktop/mobile web dashboard |
| `GET` | `/api/status` | Device details, discovered peers, received files, and last clipboard text |
| `POST` | `/api/clipboard` | Send raw text clipboard content |
| `POST` | `/api/send_clip` | Dashboard endpoint that relays JSON clipboard data to a selected peer |
| `POST` | `/api/upload` | Upload a file directly to the receiving peer; uses `X-File-Name` and `X-File-Size` |
| `POST` | `/api/send_file` | Desktop dashboard proxy endpoint for a selected peer |
| `GET` | `/api/download?file=<name>` | Download a received file |

## Troubleshooting

| Problem | What to check |
| --- | --- |
| No device appears | Confirm both devices are on the same LAN, both services are running, and UDP/TCP ports `53535` and `53536` are allowed through the firewall. Then use manual IP connection. |
| Manual connection fails | Verify the IP address and that the target app is running. Do not use `localhost`; it refers to the current device. |
| Desktop build fails | Install MinGW/LLVM on Windows or g++/clang++ on Linux, then make sure the compiler is on `PATH`. |
| Android cannot receive a file | Accept the in-app transfer prompt and grant the requested storage/media permissions. |
| Clipboard does not update on Linux | Install `xclip`; the desktop client uses it for system clipboard writes. |
| Transfer stalls | Check Wi-Fi stability, firewall/VPN settings, and that the peer has not stopped its service. Socket operations time out after 15 seconds on desktop. |

## Development notes

- The desktop dashboard is compiled into the executable from `desktop/web_assets.hpp`; edit that file to change the desktop web UI.
- Android state and protocol handling live in `mobile/lib/services/airbridge_state.dart`.
- The mobile UI is in `mobile/lib/views/home_view.dart` and uses `provider` for state management.
- The desktop build uses C++17 and links `ws2_32` and `shell32` on Windows.

### Verification

The desktop project was verified with the Windows g++ command used by `compile.bat`:

```powershell
g++ -O3 -std=c++17 desktop\main.cpp -o airbridge.exe -lws2_32 -lshell32
```

## Security and limitations

- There is no pairing, authentication, TLS, encryption, access control, or integrity verification.
- Clipboard data and files travel in clear text over the LAN.
- Peer names and operating-system labels are broadcast data and should not be treated as trustworthy identities.
- The desktop receiver accepts uploads without a confirmation prompt; the Android receiver asks the user before accepting one.
- This project supports IPv4 LAN discovery only. Discovery uses broadcast and multicast (`224.0.0.1`), which some network equipment blocks.

Before using AirBridge outside a trusted private LAN, add authenticated pairing and encrypted transport.
