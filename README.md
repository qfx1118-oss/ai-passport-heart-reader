[简体中文](README.zh_CN.md)

# Heart Reader friend installation package
Download the ready-to-install package from [Releases](https://github.com/qfx1118-oss/ai-passport-heart-reader/releases/latest). Use the release asset ZIP; GitHub Source code archives omit compiled firmware and cannot run the installer directly. Development sources are under source; upstream FoloToy and font licenses are retained.


Extract the entire ZIP into a permanent directory on your Mac. Give the adjacent AGENT.md to your local Agent (WorkBuddy, Codex, or another executor with local scripts and scheduling). Send the ZIP, not merely this page, a single binary or a temporary preview URL.

## One instruction for your Agent

Read AGENT.md in this package and install Heart Reader for my Heart Ball and AI Passport, including Wi-Fi setup, my own Heart Ball authorization, first curated content and scheduled updates. Confirm my device and data impact before flashing. Let me complete official login and local Wi-Fi entry, automate the other steps, and report actual verification.

## Requirements

macOS, Python 3.9+, Internet, a USB data cable, and an 8MB ESP32-C3 FoloToy AI Passport. Use your own Heart Ball account and working official MCP authorization. Both devices must share a trusted LAN; the Passport uses 2.4GHz Wi-Fi. Guest isolation can block communication. Keep the extraction directory: the service uses its Python environment. This installer does not support Windows/Linux. ESP-IDF/compiler installation is unnecessary; preparation downloads pinned open-source Python dependencies.

## Behavior and schedule

UP/DOWN browse, OK pauses/resumes, automatic dwell is ten seconds. Date/time, lunar date and model weather near Chengdu Jinjiang appear above seven content-day segments, without a countdown.

Prepare at 09:25 on office days, release at 09:35, poll every 60 seconds in Asia/Shanghai. Tuesday covers Monday, Wednesday Monday–Tuesday, Thursday Monday–Wednesday, Friday Monday–Thursday. Monday covers the complete preceding Monday–Sunday. Weekends and statutory holidays skip updates. If Monday is a holiday, the first office day completes last week, the next starts this week's accumulation. Failures/empty content preserve the previous edition.

The Mac must be awake with the executor running as its scheduler requires, authorized MCP and available model allowance. No paid cloud server is required; your Agent still consumes its own model allowance. Closed-app/sleep behavior must be verified. Weather is fixed to Jinjiang; another city needs a separate change and acceptance.

## Contents and privacy

firmware/full.bin is a verified merged image for offset 0x0; fresh installation may replace existing partitions/settings. application.bin, bootloader.bin and partition-table.bin support segmented installation only after compatibility checks. install.py verifies files, prepares dependencies, writes only the specified device, installs the bridge and opens setup. tools contains portable publication/bridge scripts. source contains the application, board component, build configuration and fonts without a private builtin header; later development requires ESP-IDF 5.5.3, while installation needs no compilation. SHA256.json detects corruption but does not authenticate the distributor. licenses preserves upstream/font licenses; installed dependencies retain their distribution licenses.

No personal extracts are embedded. First boot awaits synchronization. The package excludes the original owner's recordings, selections, device identity, Wi-Fi, keys, OAuth credentials and ELF debugging files. Each friend generates a new local key. LAN HTTP is unencrypted; use a trusted network.

## Human checkpoints

The Agent runs `python3 install.py --prepare` then `python3 install.py --devices`. Identify the Passport by unplug/replug, never the first ESP32. Flash only after approval for the selected firmware/device/data impact. Settings-preserving mode refuses incompatible partitions; only separately approved fresh installation replaces settings. Original-firmware backup is not required, but settings preservation and factory rollback are not promised.

Enter Wi-Fi only on the local http://127.0.0.1:8765/setup page, and authorize Heart Ball yourself through the official flow. The Agent verifies the service and first real content before creating a unique schedule, without loading whole transcripts into model context.

## Uninstall and evidence

Remove this task from your Agent and run `.venv/bin/python tools/install_mac_bridge.py --uninstall` from the extracted directory. It retains content and does not restore factory firmware. Preserve other services.

The functional baseline passed screen/button checks on the original device. This exact private-data-free build requires first acceptance on your hardware. See firmware/build-info.json for build/host evidence; successful compilation/installation does not prove physical rendering.
