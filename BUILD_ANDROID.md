# Warzone2100 Mobile - building the Android app

Requirements: JDK 17, Android SDK + NDK (see platforms/android/build.gradle), CMake, Ninja.

1. (Release only) create the signing key: `cp platforms/android/keystore.properties.example platforms/android/keystore.properties`,
   edit it, then run `bash platforms/android/tools/create_release_keystore.sh`.
2. Build: `cd platforms/android && ./gradlew :app:assembleDebug` (test) or `:app:assembleRelease` / `:app:bundleRelease` (store).
3. Game data: the `data/` folder is part of the source (see "Game data" below). Gradle packs it into base.wz and mp.wz.
4. Translations: `python3 platforms/android/tools/po2mo.py`. Credits: `python3 platforms/android/tools_gen_credits.py`.

Warzone2100 Mobile is an unofficial adaptation of Warzone 2100 (GPL v2+). See COPYING, COPYING.NONGPL and AUTHORS.


## Game data
The `data/` folder is included (upstream commit 253e370c2af78228a822df6afa82b6261dc94656 with its submodules texpages, fonts, music and mods, except `data/terrain_overrides/high`). Without it the game stops at start with "Unable to load: intfac.img".
