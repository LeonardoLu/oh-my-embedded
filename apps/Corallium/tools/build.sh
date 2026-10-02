#!/bin/bash
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$repo_root"
case "${1:-macos}" in
  macos)
    xcodebuild -project apps/Corallium/Corallium.xcodeproj -scheme Corallium \
      -configuration Debug -destination 'platform=macOS' -derivedDataPath tmp/corallium-build \
      CODE_SIGN_IDENTITY=- CODE_SIGN_STYLE=Manual build
    ;;
  ios)
    xcodebuild -project apps/Corallium/Corallium.xcodeproj -scheme Corallium \
      -configuration Debug -destination 'generic/platform=iOS' -derivedDataPath tmp/corallium-ios-build \
      CODE_SIGNING_ALLOWED=NO build
    ;;
  simulator)
    xcodebuild -project apps/Corallium/Corallium.xcodeproj -scheme Corallium \
      -configuration Debug -destination 'generic/platform=iOS Simulator' -derivedDataPath tmp/corallium-ios-build \
      CODE_SIGNING_ALLOWED=NO build
    ;;
  *) printf 'Usage: %s [macos|ios|simulator]\n' "$0" >&2; exit 2 ;;
esac
