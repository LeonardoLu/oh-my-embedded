#!/bin/bash
set -euo pipefail
repo_root="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$repo_root"
app_binary="$repo_root/tmp/corallium-build/Build/Products/Debug/Corallium.app/Contents/MacOS/Corallium"
if [[ ! -x "$app_binary" ]]; then
  printf 'Build first: bash apps/Corallium/tools/build.sh macos\n' >&2
  exit 1
fi
xcrun swiftc -swift-version 5 -default-isolation MainActor \
  apps/Corallium/Corallium/Protocol.swift apps/Corallium/Corallium/EventLog.swift \
  apps/Corallium/Corallium/DeviceStore.swift apps/Corallium/Corallium/SelfTests.swift \
  apps/Corallium/tools/TestMain.swift -o tmp/corallium-fixture-tests
./tmp/corallium-fixture-tests --fixtures "$repo_root/protocols/corallium-v1/examples"
"$app_binary" --self-test
"$app_binary" --integration-test
