[简体中文](AGENT.zh_CN.md)

# Agent runbook: install Heart Reader

This package authorizes preparation. Before any device write confirm the exact target, firmware and settings impact with its owner. macOS and an 8MB ESP32-C3 FoloToy AI Passport only. Run commands from the extracted directory on the friend's Mac, never install their service on the distributor's machine.

## 1. Prepare

Read README.md. Run `python3 install.py --verify` and `python3 install.py --prepare`. Network is needed for pinned dependencies; no global Python packages or ESP-IDF installation. Explain missing Python/access requirements without asking for passwords/tokens.

Run `python3 install.py --devices`. Identify a unique Passport serial by unplug/replug with the owner. Do not operate Dot/other ESP32s or reuse the distributor's device identity.

## 2. Install after approval

Settings-preserving mode requires an identical partition layout:

```bash
python3 install.py --install --serial '<confirmed friend serial>' --approve-flash
```

Factory/other firmware may have incompatible partitions. Only after separate consent to replace firmware and settings use:

```bash
python3 install.py --install --serial '<confirmed friend serial>' --approve-flash --fresh-install
```

The script checks chip/8MB capacity and writes only that device, records its identity, creates a new key, installs the user service and opens setup. Fresh mode writes full.bin at 0x0; segmented mode writes 0x0/0x8000/0x10000 without touching NVS. Never flash the application at 0x0 or erase the whole chip. Existing installation/port 8765 conflicts stop setup; inspect ownership rather than overwriting or stopping other jobs.

Let the owner enter Wi-Fi on http://127.0.0.1:8765/setup. It uses the current Mac's Bonjour name. Never put the password in chat/files/logs. USB is required for initial provisioning, Wi-Fi handles routine updates. Check same-LAN/2.4GHz/Bonjour/network isolation; do not change firewall/power settings without authorization.

## 3. Authorize and publish real content

Discover the Agent's official Heart Ball MCP integration and let the friend authorize their own account. Actually call both recall_memory and recall_note. Installed tools alone do not prove access. On missing tools/authorization stop publication; no distributor credentials or invented content.

Run `.venv/bin/python tools/quote_window.py --prepare`. Process only update=true and use its issue_date/start_date/date/covered_days/missing_days. Holidays exit. First install has no buckets, so retrieve every missing day. Later preserve existing daily selections and fetch only missing days. Only 2026 holidays are maintained; verify future years against published official notices, test and synchronize the runtime, never guess.

Query both record types for each day, consume all pages. Use Shanghai millisecond boundaries, left-open/right-closed: midnight minus 1 ms through next midnight minus 1 ms. Start with summaries then check candidate original sentences/times/context. Filter/store huge responses before model context. Distinguish the owner, other people, played media and AI dialogue.

Select at most five per day, preferably three. Favor useful, faithful, standalone extracts, text at most 32 characters (hard limit 36), source at most eight (hard limit ten), real day and HH:MM. Clearly label recording extracts; exclude private identifiers, internal organization details and unsupported medical assertions. Use prepare_quotes.inventory characters, and select fewer rather than inventing.

Write private candidate JSON with date equal to the planned content end, reviewed_days containing fully checked missing dates, and quotes containing day/text/source/time. A fully checked empty day can be reviewed with no quotes; failed authorization/incomplete pagination cannot. Never publish placeholders; keep evidence private.

```bash
.venv/bin/python tools/publish_quotes.py 'private/<candidate>.json' --issue-date '<issue date>'
```

Publisher validates, preserves old buckets, concatenates/deduplicates daily selections, atomically writes project and runtime editions. Candidate schema differs from final packs: do not feed candidates directly to prepare_quotes.py. Final packs can be checked with `.venv/bin/python tools/prepare_quotes.py 'private/editions/<issue date>.json'`. Limit 48 quotes/9000 UTF-8 bytes; seven days at five yields 35. Entirely empty/failed editions retain the previous content.

## 4. Accept and schedule

Verify authenticated /v1/quotes and /v1/context locally. Read private/device-key programmatically; never print it. Output only issue/range/count/covered_days/calendar count/weather availability. Before 09:35 serving the preceding edition is correct; use mac_bridge.select_pack with an explicit issue-day 09:35 timestamp to test staging without releasing early.

Ask the owner to inspect Chinese glyphs, clock/lunar/weather, buttons and ten-second rotation. Initial pending/empty content is normal. Only observed device fetch/import or owner confirmation counts as device success.

Create exactly one weekday 09:25 Asia/Shanghai task in this Agent: compute plan, skip holidays, retrieve both sources/all pages, curate only missing days, validate/publish and verify service. Bridge release is 09:35 and the device polls every minute. Monday covers the complete prior week; Tuesday current Monday; Wednesday Monday–Tuesday; Thursday Monday–Wednesday; Friday Monday–Thursday. Holiday Monday postpones the full prior week to the first office day, with current accumulation starting the following office day. All weekends skip updates, including makeup weekends.

Actually save the schedule and verify timezone/next run. If unsupported, report pending rather than equating documentation with completion. Verify awake/app-exit requirements. Stay quiet on success, notify actionable failures. No new paid services or duplicate schedules in other Agents.

## 5. Recovery and report

The extraction directory's private holds this owner's buckets, editions, binding and key. Runtime is ~/Library/Application Support/AIPassportHeartBridge, LaunchAgent local.ai-passport.heart-bridge. It uses the extraction directory's .venv: do not move/delete it. Restore only a genuinely stopped service with `.venv/bin/python tools/install_mac_bridge.py`; also use it to synchronize verified calendar changes. Do not start a competing port 8765 service.

Retain content on failure. Never clear Wi-Fi/buckets/keys or upload private wholesale. Uninstall with `.venv/bin/python tools/install_mac_bridge.py --uninstall` and remove this Agent schedule; factory firmware is not restored.

Separately report package verification, dependencies, flash, account retrieval, first publication, screen/buttons and saved schedule. Mark unobserved scheduled trigger, weekly/holiday transitions and endurance pending. Build success is not friend-device acceptance.
