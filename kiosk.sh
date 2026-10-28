#!/usr/bin/env bash
# Full-feature web game (all 50+ soundtracks, gore, scope etc.) fullscreen on a Raspberry Pi 5.
# For boot-to-game: add this script to ~/.config/autostart (desktop) or a systemd user service.
sudo apt-get install -y chromium-browser >/dev/null 2>&1 || sudo apt-get install -y chromium >/dev/null 2>&1
exec chromium-browser --kiosk --autoplay-policy=no-user-gesture-required --noerrdialogs "file://$(cd "$(dirname "$0")" && pwd)/trypicodoom.html" 2>/dev/null || exec chromium --kiosk --autoplay-policy=no-user-gesture-required "file://$(cd "$(dirname "$0")" && pwd)/trypicodoom.html"
