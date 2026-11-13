#!/usr/bin/env bash
# Builds FIREPOWER as a PS5 app folder and ZIP, the same way QUANTUM LOCKDOWN
# builds (the ps5-homebrew-starter kit and the PS5 payload SDK: run the
# starter's tools/setup.sh once first). Put this repository in the starter's
# folder, beside kit/ (here: firepower/).
#
#   firepower/build.sh                    -> build/firepower/PPSA99908/ and PPSA99908.zip
#   firepower/build.sh deploy <PS5 IP>    -> also copies it to /data/homebrew over FTP
#   firepower/build.sh sim [script]       -> the PC test build instead (no PS5 needed):
#                                            plays a scripted controller, saves screenshots
#
# The game's own files come from YOUR Firepower disk, once:
#   python3 firepower/tools/extract_assets.py /path/to/Firepower.adf
set -euo pipefail

usage() { echo "usage: firepower/build.sh [deploy <PS5 IP> | sim [script.txt]]" >&2; exit 2; }
deploy_host=
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(dirname -- "$here")

if [[ ! -f $here/assets/fp/tiletypes.bin ]]; then
    echo "The game's files aren't there yet (assets/fp). Run, once:" >&2
    echo "  python3 $here/tools/extract_assets.py /path/to/Firepower.adf" >&2
    [[ ${1:-} == sim ]] && exit 1
    echo "(Building anyway: copy assets/fp over FTP into the app's assets folder later.)" >&2
fi

case ${1:-} in
    "") [[ $# -eq 0 ]] || usage ;;
    deploy) [[ $# -eq 2 && -n $2 ]] || usage; deploy_host=$2 ;;
    sim)
        out=$here/build/sim
        mkdir -p "$out/shots"
        g++ -std=c++20 -O2 -I"$here/src" "$here/src/game.cpp" "$here/src/gfx.cpp" "$here/src/assets.cpp" \
            "$here/sim/sim_main.cpp" -o "$out/sim"
        "$out/sim" "${2:-$here/sim/tour.txt}" "$out/shots" "$here/assets/fp/"
        if python3 -c 'import PIL' 2>/dev/null; then
            python3 -c 'import sys, glob, os
from PIL import Image
for f in glob.glob(os.path.join(sys.argv[1], "*.ppm")):
    Image.open(f).save(f[:-4] + ".png"); os.remove(f)' "$out/shots"
        fi
        echo "Screenshots: $out/shots"
        exit 0 ;;
    *) usage ;;
esac

home=${PS5_STARTER_HOME:-$HOME/.ps5-starter}
kit=$home/kit-work
sdk=${PS5_PAYLOAD_SDK:-$home/ps5-payload-sdk}

# 1. The kit, copied to the work folder whenever kit/ changed (as the starter's own apps do).
#    Its .deps (the SDK and the tools: big, and linked rather than copied) are left
#    where they are.
stamp=$(cd "$repo/kit" && find . -path ./.deps -prune -o -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -c1-16)
if [[ ! -f $kit/.kit-stamp || $(cat "$kit/.kit-stamp") != "$stamp" ]]; then
    echo "Preparing the kit..."
    mkdir -p "$kit"
    find "$kit" -mindepth 1 -maxdepth 1 ! -name .deps -exec rm -rf {} +
    (cd "$repo/kit" && tar cf - --exclude=./.deps .) | (cd "$kit" && tar xf -)
    echo "$stamp" > "$kit/.kit-stamp"
fi
# the SDK: the work folder's link to it (the kit's own copy, or the setup's)
[[ -d $repo/kit/.deps/native/ps5-payload-sdk && ! -d $sdk ]] && sdk=$repo/kit/.deps/native/ps5-payload-sdk
kit_sdk=$kit/.deps/native/ps5-payload-sdk
[[ ! -L $kit_sdk || -e $kit_sdk ]] || rm -f "$kit_sdk"
if [[ ! -e $kit_sdk && -x $sdk/bin/prospero-lld ]]; then
    mkdir -p "$kit/.deps/native"
    ln -s "$sdk" "$kit_sdk"
fi
# the kit's other dependencies (its tools), linked in the same way if missing
if [[ -d $repo/kit/.deps ]]; then
    for d in "$repo"/kit/.deps/* "$repo"/kit/.deps/native/*; do
        [[ -e $d ]] || continue
        rel=${d#"$repo"/kit/}
        [[ -e $kit/$rel || -L $kit/$rel ]] && continue
        [[ $rel == .deps/native ]] && { mkdir -p "$kit/$rel"; continue; }
        mkdir -p "$(dirname -- "$kit/$rel")"
        ln -s "$d" "$kit/$rel"
    done
fi

# 2. The game's files beside the kit's own (in kit-work/firepower). Only src/ is
#    compiled: the PC test build (sim/) stays out.
rm -rf "$kit/firepower"
mkdir -p "$kit/firepower"
cp -r "$here/src" "$here/sce_sys" "$here/assets" "$kit/firepower/"
find "$kit/firepower/assets" -name 'README*' -delete
bash "$kit/tools/validate-assets.sh" "$kit/firepower/sce_sys"

# 3. Build. The kit compiles every .c/.cpp under firepower/src.
vars=(APP_SOURCE_DIR=firepower/src APP_PARAM=firepower/sce_sys/param.json
      APP_SCE_SYS=firepower/sce_sys APP_ASSETS=firepower/assets)
if [[ -n $deploy_host ]]; then
    make -C "$kit" "${vars[@]}" deploy PS5_HOST="$deploy_host"
else
    make -C "$kit" "${vars[@]}"
fi

# 4. The result, next to the sources.
title=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["titleId"])' \
    "$here/sce_sys/param.json")
rm -rf "$repo/build/firepower"
mkdir -p "$repo/build/firepower"
cp -r "$kit/dist/$title" "$kit/dist/$title.zip" "$repo/build/firepower/"
echo
echo "App folder: build/firepower/$title"
echo "ZIP:        build/firepower/$title.zip"
