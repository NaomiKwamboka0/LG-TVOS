#!/bin/sh
# Checks which Kenyan stations are live on YouTube right now and prints
# their permanent channel IDs, for services/channels.c and channels.txt.
# Needs yt-dlp (with deno) and internet. Usage:
#   tools/verify-channels.sh                 # the built-in line-up
#   tools/verify-channels.sh URL [URL...]    # specific addresses
set -u

YTDLP=${YTDLP:-yt-dlp}

if [ "$#" -eq 0 ]; then
    set -- \
        https://www.youtube.com/channel/UChBQgieUidXV1CmDxSdRm3g/live \
        https://www.youtube.com/channel/UCqBJ47FjJcl61fmSbcadAVg/live \
        https://www.youtube.com/channel/UCKVsdeoHExltrWMuK0hOWmg/live \
        https://www.youtube.com/channel/UCt3SE-Mvs3WwP7UW-PiFdqQ/live \
        https://www.youtube.com/channel/UCypNjM5hP1qcUqQZe57jNfg/live \
        https://www.youtube.com/channel/UC_zA9UIWE1fB-jfFk_DBSYw/live \
        https://www.youtube.com/channel/UCZlNMLTVNxzwB0axilK-rDw/live \
        https://www.youtube.com/channel/UCd9nkc2XA77NMxBvQz35I2Q/live \
        https://www.youtube.com/channel/UCmKXJ4C59JjVVJ-axXfpJcg/live \
        https://www.youtube.com/channel/UCUhrpGr_luwUzVaxiW5Jkhw/live
fi

for url in "$@"; do
    printf '%s\n' "== $url"
    "$YTDLP" --ignore-config --skip-download --no-playlist --socket-timeout 20 \
        --print '%(channel_id)s|%(channel)s|%(uploader_id)s|%(live_status)s|%(title).70s' "$url" 2>&1 |
        grep -v '^\[' | tail -n 3
done
