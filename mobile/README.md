# AirBridge Android client

This directory contains the Flutter Android client for AirBridge.

## Install and run

```bash
flutter pub get
flutter run
```

Grant the storage/media permissions on first launch. The app starts LAN discovery automatically; both the Android device and its peer must be on the same local network.

## Build

```bash
flutter analyze
flutter test
flutter build apk --release
```

The root [README](../README.md) contains full desktop setup, network requirements, usage, troubleshooting, and security information. Follow [CONTRIBUTING.md](../CONTRIBUTING.md) for versioning and release steps.
