#!/bin/sh
# Refresh tracked transparent masks after changing the fixed Phosphor SVGs.
set -eu

PROJECT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SOURCE="$PROJECT/assets/settings-icons/source/phosphor-duotone"
OUTPUT="$PROJECT/assets/settings-icons/source/masks"
mkdir -p "$OUTPUT"

render() {
    sips -s format png -z 512 512 "$SOURCE/$2.svg" --out "$OUTPUT/$1.png" >/dev/null
}

render time clock-duotone
render bot robot-duotone
render display sun-duotone
render sound speaker-high-duotone
render power battery-charging-vertical-duotone
