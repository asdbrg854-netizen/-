# Warzone 2100 for Android — native port work-in-progress

This project is an **SDL3/C++ Android application**, not a WebView wrapper. The current target is arm64-v8a and Android 9 / API 28 or newer. It reuses the upstream game engine and assets and uses SDL's Android activity.

## Build prerequisites

Install Android SDK platform 35, NDK 27.2.12479018, CMake 3.22.1, Java 17 or newer, Gradle 8.9, and a full vcpkg checkout. The repository's `vcpkg.json` fixes a baseline; a shallow vcpkg checkout is insufficient. Set `ANDROID_HOME`, `ANDROID_NDK_HOME` (and preferably `ANDROID_NDK_ROOT`), and `VCPKG_ROOT` before building; the vcpkg Android triplet reads `ANDROID_NDK_HOME` directly. Gradle is configured with a 4 GiB heap to package the large game archives; allow several GiB of free disk space for native intermediates and APK outputs.

## Build

From this directory:

```sh
./gradlew --no-daemon -PvcpkgRoot="$VCPKG_ROOT" :app:assembleDebug :app:assembleRelease
```

If the Android dependencies have already been installed into another CMake build directory for the same ABI and vcpkg baseline, you may also pass `-PvcpkgInstalledDir=/path/to/vcpkg_installed` to reuse them. Debug and Release each stage assets from their matching native CMake build, and Gradle tracks the staged files as task inputs. The generated asset manifest fingerprints the packaged data; on launch, BootstrapActivity compares it with the private copy and refreshes stale or incomplete data before starting the engine. The expected Debug artifact is `app/build/outputs/apk/debug/app-debug.apk`; Release is under `app/build/outputs/apk/release/`. The first build compiles the game dependencies for arm64 and may take a long time. First launch copies the core archives and campaign/mod/music data to private storage before passing `--datadir` to the native game.

## Current scope and known gaps

The first target is a native arm64 build using the upstream C++ engine, SDL3, and OpenGL ES. PhysicsFS is initialized with Android's JNI context and uses the app's private files directory. The upstream code already handles touch taps through SDL mouse events and has two-finger pan/pinch support; device testing is still required. Multiplayer, videos, Vulkan, high terrain, and 32-bit ABIs are intentionally excluded from this initial target. No APK should be treated as release-ready until it has been installed and tested on an Android device.
