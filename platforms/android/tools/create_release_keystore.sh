#!/usr/bin/env bash
# Creates the release keystore described in platforms/android/keystore.properties.
# Run on your own computer (needs a JDK):  bash platforms/android/tools/create_release_keystore.sh
set -euo pipefail
cd "$(dirname "$0")/.."
get() { grep -E "^$1=" keystore.properties | head -1 | cut -d= -f2-; }
STORE_FILE="$(get storeFile)"; ALIAS="$(get keyAlias)"; PASS="$(get storePassword)"
if [ -e "$STORE_FILE" ]; then echo "$STORE_FILE already exists - not overwriting."; exit 1; fi
keytool -genkeypair -v -keystore "$STORE_FILE" -alias "$ALIAS" -keyalg RSA -keysize 2048 -validity 10000 \
  -storepass "$PASS" -keypass "$PASS" -dname "CN=Jouri, O=Warzone2100 Mobile"
echo "Created $STORE_FILE. Back it up somewhere safe: if you lose it you cannot update the app."
