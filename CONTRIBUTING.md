# Contributing to AirBridge

## Prerequisites

Install the tooling required for the component you are changing:

- Desktop: a C++17 compiler (`g++` or `clang++`). On Windows, MinGW-w64 is the simplest option.
- Android: Flutter with a Dart SDK compatible with the constraint in `mobile/pubspec.yaml`, plus a configured Android SDK/device.

## Local workflow

```powershell
git clone https://github.com/flamescreatioon/file-transfer.git
cd file-transfer
git switch -c feat/short-description
```

Build and test the affected component before committing:

```powershell
# Desktop
cd desktop
.\compile.bat

# Android (from the repository root)
cd mobile
flutter pub get
flutter analyze
flutter test
```

Use a focused Conventional Commit-style message, for example `feat: add transfer cancellation` or `fix(android): handle declined uploads`.

## Versioning and releases

AirBridge uses Semantic Versioning: `MAJOR.MINOR.PATCH`.

- Increase **MAJOR** for incompatible protocol or public-API changes.
- Increase **MINOR** for backward-compatible features.
- Increase **PATCH** for backward-compatible fixes.

`VERSION` is the canonical release version. Android’s `mobile/pubspec.yaml` must use the same semantic version, with a monotonically increasing Android build number after `+` (for example, `1.1.0+12`).

For a release:

1. Update `VERSION`.
2. Update `mobile/pubspec.yaml` to `<VERSION>+<next-build-number>`.
3. Add a dated release section to `CHANGELOG.md`.
4. Build and verify desktop and Android artifacts.
5. Commit the release with `chore(release): v<version>`.
6. Create an annotated Git tag and push the branch and tag:

```powershell
git tag -a v1.0.0 -m "AirBridge v1.0.0"
git push origin main --follow-tags
```

Never commit generated executables, APKs, Flutter build output, IDE files, or `desktop/received_files/`. Attach distributable artifacts to a release instead.
