# Skiff

**Version** 0.3.1 · **Platform** HarmonyOS 6.1 (API 24) · **License** MIT

**SSH, port forwarding and a built-in browser — on a HarmonyOS phone.** Rebuilt for portrait use: password or key login, one-tap local forwarding, and a browser that opens your server-side panel the moment the tunnel is up.

[中文](README.md) · [Changelog](CHANGELOG.md)

## Why this exists

I am a user first, not an ops engineer.

My services run on a cloud server, but almost everything I do with them happens on a phone — while commuting, queuing, or lying in bed. I do not want to expose panel ports to the public internet. The safer pattern is to expose only SSH, forward the internal port over a tunnel when needed, and open it in a browser.

On a desktop this is fluid: `ssh -L 8080:127.0.0.1:3000 user@host`, then open the browser. On a phone the flow falls apart.

I tried what already exists, and none of it fit:

- **Full-featured clients**: desktop-class SSH tools ported to mobile. Menus, sheets and dialogs stacked several levels deep — painful on a 6-inch screen. And once the tunnel is up you still switch to another app to open the link.
- **Lightweight options**: terminal only. Tunnels mean typing the command by hand, or are not supported at all.
- **What all of them miss**: no graphical one-tap tunnel setup, and no built-in browser that opens the forwarded local port. You end up bouncing between two apps.
- **Language**: several well-made tools ship with an English-only UI.

So I built the tool I wanted: **portrait-first, three steps to a working tunnel, a built-in browser that opens it immediately, and a UI in both Chinese and English.**

AI tooling took the repetitive engineering work off my hands, and the protocol core builds on an existing open-source implementation (upstream [CrossShellNext](https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext)), so the effort went into "how should this feel on a phone".

## What it replaces

| Before | After |
|---|---|
| Two apps on the phone: an SSH client and a browser | One app, tunnel and browser in the same session |
| Type `ssh -L 8080:127.0.0.1:3000 user@host`, then paste the URL into a browser | Enter the port and target, flip the switch, tap **Browse** |
| You only know the tunnel works when the browser fails | The local port is probed first; you get a hint instead of a blank page |
| The switch says "on" but the tunnel is dead | Periodic health checks rebuild dead forwards automatically |
| Digging through an English UI for settings | Chinese and English, switched instantly in Settings |

## Features

### Connections and authentication

- **The host list is the home screen**: no auto-connect on launch; each card shows a status dot, the host name and live CPU / RAM / network usage (sampled every 5 seconds, greyed `--` when offline)
- **Three shortcuts per card**: **Terminal / Tunnels / Files** connect the host and jump straight into that view
- **Authentication**: password (masked) and private key (RSA 2048 / ECDSA P-256 / Ed25519, generatable inside the app)
- **Host key verification**: the server fingerprint is recorded on first connect (TOFU) and compared before every later authentication; a changed key is rejected with the actual fingerprint shown. Editing the address or port clears the stored key.
- **Concurrent sessions**: different hosts stay connected side by side
- **Proxy**: SOCKS5 and HTTP, with a connectivity test before connecting

### Terminal

- Real PTY output rendered by xterm.js, bundled Cascadia Mono, adjustable font size
- **Six-row extended keyboard**: letters, digits and symbols, control keys, arrows and editing keys in separate groups; `Shift` / `Caps` behave independently, `Ctrl` / `Alt` combinations pass through, `Del` and `⌫` send their own control sequences
- One segmented control switches between Terminal / Tunnels / Files inside a session; the session bar keeps the host name and disconnect action
- Output is batched per frame (16 ms / 32 KB thresholds) so long scrolls stay smooth

### Port forwarding

- Separate forms for **local (L) / remote (R) / dynamic (D)**, each generating the equivalent `ssh -L/-R/-D` command line for verification
- Toggles start and stop real forwards; failures roll back to off with a reason, and are disabled while the host is not connected
- **10-second health checks** compare real forward counters: unhealthy tunnels are stopped and rebuilt with backoff, and a dead forward is never reported as "on"
- A local forward jumps to the **built-in browser**, probing the local port first

### Browser

- ArkWeb filling the session area, with an icon toolbar (close / back / forward / reload)
- Load failures give a reason and a retry entry; timeouts show a diagnostic overlay instead of a black screen

### Files (SFTP)

- Directory browsing, breadcrumbs, multi-select, new folder, rename, delete (recursive for folders)
- Upload and download queue with progress, pause, resume and cancel; failures surface the original kernel error

### Logs and diagnostics

- **Session logs**: one file per session, viewable, exportable and clearable, keeping the latest 19
- **Application logs**: connections, forwards, exceptions and crashes written daily to the sandbox, shown separately from session logs
- **Global error capture**: uncaught exceptions and crash reasons persisted for later inspection

### Appearance and language

- Light and dark themes, applied instantly without rebuilding pages
- Chinese and English, Chinese by default, switched in Settings and persisted; system controls follow the app language
- Hosts, tunnels, theme, language, font size and connection behaviour all survive a restart

## Use cases

**1. Open a server panel from your phone**
Container managers, dashboards and admin panels usually listen on `127.0.0.1` only. Create a local forward that maps the panel port to the phone, tap **Browse**, and keep the port closed to the internet.

**2. Quick config edits and log reading**
A full PTY: `vim`, `tail -f`, `systemctl` all work. The six-row keyboard supplies the `Esc`, `Tab`, `Ctrl` and arrow keys phone keyboards lack.

**3. Moving files between phone and server**
Upload and download in the SFTP panel with a queue and progress; failures show the kernel's own message instead of a generic "upload failed".

**4. Understanding what actually happened after a disconnect**
Session and application logs are stored separately, so you can read them by timestamp instead of blindly reconnecting.

## Screenshots

Rendered on a 390×863vp portrait baseline; host name and address are placeholders.

| Hosts | Terminal |
|:---:|:---:|
| ![Hosts](docs/preview/hosts-en.png) | ![Terminal](docs/preview/terminal-en.png) |

| Port forwarding | File browser |
|:---:|:---:|
| ![Port forwarding](docs/preview/tunnels-en.png) | ![File browser](docs/preview/files-en.png) |

## Running it

### Requirements

- DevEco Studio with the SDK aligned to API 24 / HarmonyOS 6.1; `DEVECO_HOME` pointing at the install directory
- A HarmonyOS emulator (x86_64) or device (arm64-v8a)
- Build caches preferably on a non-system drive (`HVIGOR_USER_HOME`)

### Build and package

`release.py` handles version bump, build, removal of unconfigured signing entries, and artifact plus SHA256 collection in one pass:

```bash
git clone <this repository>
cd Skiff
python release.py                  # release build (arm64-v8a)
python release.py --abi both       # both ABIs
python release.py --debug          # debug build
python release.py --no-bump        # keep the current version
python release.py --dry-run        # print the steps only
```

On Windows you can double-click `release.bat`. Artifacts land in `产出/` outside the repository, together with `.sha256` files.

### Install

```bash
hdc install 产出/Skiff-<version>-arm64-v8a-release-unsigned.hap
```

You can also open the project in DevEco Studio and run it, which is equivalent to
`hvigorw assembleHap --mode module -p product=default -p buildMode=debug --no-daemon` (`hvigorw` ships with DevEco Studio, not this repository).

### First run

1. Tap **+** on the home screen: enter address, port and user, choose password or key, save
2. Tap **Connect** on the card; the host key fingerprint is recorded on first use (TOFU)
3. Tap **Terminal / Tunnels / Files** on the card to enter a session; the segmented control switches views at any time
4. In Tunnels, tap **+ New forward**: choose the type (L/R/D), enter the local port and target, save, then flip the switch
5. With a local forward on, tap **Browse** to open the forwarded port in the built-in browser

### Native dependencies

The C++ core statically links **libssh2 1.11.1 + OpenSSL 1.1.1w**. Both `entry/libs/arm64-v8a/` and `entry/libs/x86_64/` are included and selected by the build script via `OHOS_ARCH`.

## Current limitations

- **Unsigned**: all artifacts are unsigned and only installable on debug devices; distribution requires your own signing
- **Limited authentication**: password and public key only; keyboard-interactive is not implemented, so servers requiring 2FA cannot be used
- **Background persistence not fully verified**: the "keep tunnels in background" option exists, but long-running background survival needs real-device testing
- **"Keep-alive" switch semantics**: when disabled the core still sends a probe every 60 seconds by default; the switch really switches to application-level heartbeats
- **Uneven forward coverage**: local (L) forwarding and the built-in browser are verified against a live server; remote (R) and dynamic (D) are implemented but not individually tested
- Desktop-shaped pages remain in the codebase but are not reachable from the mobile navigation

## Architecture

```
ArkTS UI        pages/mobile/*  host list / terminal / tunnels / files / browser / settings
      ↓         common/  SshService · KeyStore · Store · AppLog · PortProbe
NAPI bridge     entry/src/main/cpp/napi*.cpp
      ↓
C++ core        libssh2 (session / channels / SFTP) + forward threads + keepalive thread
```

Hosts and settings are persisted through a single preferences file (`skiff_store`).

## Milestones

| Phase | Scope | Status |
|---|---|---|
| P0 | Real connections and a real terminal | Done |
| P1 | Real L / R / D forwarding plus the built-in browser | Done |
| P2 | Real SFTP transfers with queue and cancellation | Done |
| P3 | Persistence, keystore and host fingerprints | Done |
| P4 | Session logs, application logs, automatic reconnect | Done |
| P5 | Dual-ABI packaging (arm64-v8a + x86_64) | Done |
| P6 | Truthfulness pass: global heartbeat, tunnel health checks, foreground liveness, removal of fake states | Done |

## Contributing

Issues and pull requests are welcome. For UI changes, please verify by actually operating the app on an emulator or device rather than relying on screenshots.

## License and credits

Released under the [MIT License](LICENSE).

- Upstream: [CrossShellNext](https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext) — SSH / Telnet / SFTP protocol core and the desktop implementation
- Terminal font: [Cascadia Mono](https://github.com/microsoft/cascadia-code) (MIT)
- Terminal rendering: [xterm.js](https://xtermjs.org/) (MIT)

---

**Skiff** — pocket-sized remote operations. The boat is ready; just cast off.
