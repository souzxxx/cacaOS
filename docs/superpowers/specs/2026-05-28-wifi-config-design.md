# WiFi Config (sem reflashar) — Design

**Date:** 2026-05-28
**Target:** CacaOS on ESP32-2432S028R (WROOM, no PSRAM)
**Status:** Approved design, ready for implementation plan

## Goal

Let the user change the WiFi network from the device screen, without
recompiling/reflashing. Today `wifi_mgr` reads `WIFI_SSID`/`WIFI_PASS` as
compile-time constants from `config.h` (`wifi_mgr.cpp:98`). After this change,
credentials are read from NVS when present, falling back to `config.h` so the
device still works out of the box.

## Non-goals

- Encrypting the stored password (NVS plaintext is acceptable for this gift
  device — YAGNI).
- Enterprise/802.1X networks, captive portals, or hidden-SSID auto-discovery
  beyond a manual "Outra" entry.
- Multiple saved networks / roaming. Exactly one saved network at a time.

## Architecture

Two pieces:

1. **`src/system/wifi_mgr.{h,cpp}`** (refactor) — credential resolution moves
   from compile-time constants to an NVS-backed lookup with `config.h` fallback,
   plus a new API to apply+persist new credentials.

2. **`src/apps/wifi_config/wifi_config.{cpp,h}`** (new) — a screen launched
   from inside the `settings` app (not a new homescreen icon). Handles the
   async scan, network list, password entry, connect attempt, and result.

The `settings` app gets one new row/button ("WiFi") that calls
`wifi_config_show()`.

## Credential resolution

```
wifi_mgr resolves credentials at begin() and apply() time:
  NVS namespace "wifi", key "ssid" non-empty?  -> use NVS ssid + pass
  else                                          -> use WIFI_SSID / WIFI_PASS (config.h)
```

`config.h` remains the factory default. A successful on-screen change writes to
NVS, which then takes precedence on every subsequent boot.

### NVS schema

- Namespace: **`"wifi"`** (new; add to the reserved-namespaces list in
  `storage.h`).
- `ssid` — string, max 32 chars.
- `pass` — string, max 63 chars.
- "Configured" is implied by a non-empty `ssid`.

## wifi_mgr API changes

New public functions (header):

- `bool wifi_mgr_apply_credentials(const char* ssid, const char* pass);`
  Attempts to connect with the given credentials. **Persists to NVS only on
  success.** Returns `true` if connected within the timeout, `false` otherwise.
  On failure the previously saved NVS credentials are left untouched so a bad
  attempt never bricks a known-good network.
- `const char* wifi_mgr_current_ssid(void);`
  Returns the SSID currently in use (resolved NVS-or-config), for the UI to
  display and to mark the active row in the scan list.

Changed behavior:

- `wifi_mgr_begin()` resolves credentials via NVS→config.h instead of reading
  `WIFI_SSID`/`WIFI_PASS` directly.

Internal helper:

- `static void resolve_credentials(char* ssid_out, char* pass_out);` —
  single source of truth for the NVS→config.h fallback, used by both
  `begin()` and `apply_credentials()`.

## Screen flow (`wifi_config`)

1. **Open** → kick off an **async scan** (`WiFi.scanNetworks(true)`). An
   `lv_timer` polls `WiFi.scanComplete()` (no blocking — protects the LVGL
   tick). Show a "procurando redes…" state with a spinner.
2. **List** → on scan complete, build a scrollable list:
   - Deduplicate SSIDs, sort by RSSI (strongest first).
   - Each row: SSID label + 🔒 if encrypted. The currently-active SSID is
     marked (e.g. a check or highlight).
   - A trailing **"➕ Outra (digitar)"** row for hidden/manual SSID entry.
3. **Tap a network** → password screen: a one-line `lv_textarea` +
   `lv_keyboard` (same pattern as the tamagotchi naming wizard,
   `tamagotchi.cpp:1158-1173`). Open networks (no 🔒) skip the password step.
   The "Outra" row first asks for the SSID, then the password.
4. **Conectar** → call `wifi_mgr_apply_credentials(ssid, pass)` (attempts
   connection *before* persisting).
5. **Result**:
   - ✓ connected → show success (network name + IP), persist already done by
     `apply_credentials`. Return to settings after a short confirmation.
   - ✗ failed (wrong password / timeout ~10s) → show an error with a retry
     option. Previously saved NVS credentials are preserved.

## Error handling

- **Scan returns nothing** → "nenhuma rede encontrada" + a retry button that
  re-runs the scan.
- **Wrong password / connect timeout** → clear error message; old credentials
  intact; retry or back.
- **Scan while connected** → supported; STA scan works on a connected radio.
- **Screen closed mid-scan/connect** → cancel the polling `lv_timer` and free
  the scan results in the screen's delete handler so nothing dangles.

## Memory

- Scan list of ~10–20 buttons + keyboard. Comfortable now that LVGL allocates
  from the general heap (CLIB allocator) rather than a fixed pool.
- `WiFi.scanNetworks` results are freed with `WiFi.scanDelete()` once the list
  is built.

## Files touched

| File | Change |
|------|--------|
| `src/system/wifi_mgr.h` | add `wifi_mgr_apply_credentials`, `wifi_mgr_current_ssid` |
| `src/system/wifi_mgr.cpp` | NVS→config.h credential resolution; apply+persist; reconnect |
| `src/system/storage.h` | document the new `"wifi"` reserved namespace |
| `src/apps/wifi_config/wifi_config.h` | `void wifi_config_show(void);` |
| `src/apps/wifi_config/wifi_config.cpp` | scan / list / password / connect flow |
| `src/apps/settings/settings.cpp` | add a "WiFi" row that calls `wifi_config_show()` |

## Testing

No on-device hardware available yet (board still arriving). Verification is:

- `pio run` (both `cyd` and `sim`) stays green.
- The existing post-boot heap log shows the scan/list flow stays within budget
  when exercised on the board.
- On-board manual test when hardware arrives: change network, verify it
  reconnects and persists across reboot; verify a wrong password preserves the
  old network.

## Open questions

None outstanding. Optional follow-up (not in this scope): a "esquecer rede /
voltar ao padrão" action that clears the NVS `"wifi"` namespace to fall back to
`config.h`.
