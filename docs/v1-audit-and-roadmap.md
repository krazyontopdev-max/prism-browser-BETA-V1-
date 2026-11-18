# Prism V0.5–V1.0: initial audit and proposed delivery plan

Date: 15 November 2026. Status: audit and proposal only; application remains V0.4.
This document follows the supplied master development prompt. No new milestone
has been implemented, built, deployed or accepted during this audit.

## Existing implementation

- `CMakeLists.txt` builds C++17 with installed Qt 6.12 Widgets, WebEngine,
  WebChannel and Concurrent. `build.cmd` selects MSVC v143 14.44 x64 and Release.
- `src/main.cpp` creates one `BrowserWindow`. `src/browserwindow.cpp` owns tabs,
  shared named profile, navigation, download dialogs, permission and HTTP/proxy
  authentication prompts, fullscreen, themes, recently closed URLs and sleeping.
  Existing externally supplied test profiles are not owned by the window.
- `BrowserWindow` destroys its central widget/pages before its owned profile.
  Its sleeping recommendations, input protection, Keep Awake and supported
  BFCache setting must survive each milestone.
- `src/address.cpp` resolves addresses and Google queries. Search and Home are
  not yet backed by a general browser settings model.
- `NewTabStore` owns local photo/shortcut settings and bounded network fetching;
  `NewTabBridge` exposes internal-page controls. The store currently chooses
  the application's ordinary local-data directory independently of WebEngine's
  profile. Adding private windows requires isolating this native storage too;
  an OTR WebEngine profile alone would not prevent native favicon/settings writes.
- Forty offline WebP photos exist. CMake and deployment copy images, catalog,
  credits, provenance and their CC0/BY/BY-SA terms. Preserve these files unchanged.
- The three CTest suites cover addresses, windows and new tabs. Existing
  coverage includes page release/wake, navigation-cache state, consent,
  authentication, downloads and wallpapers. These are prior results; this audit
  did not rerun or certify them.
- `deploy.cmd` copies Qt dependencies and the photo pack. `launch.cmd` prefers
  the deployed executable. No installer or root application source license was
  found. Deployment currently excludes the MSVC runtime and does not provide
  a complete Qt/Chromium distribution-notice package.
- No Shields interceptor, private-window action, searchable browser settings,
  bookmark/history manager, local-profile selector or persistent session
  restoration was found in the inspected entry points and build sources.

## Constraints confirmed locally

The installed `qt_lib_webenginecore_private.pri` lists
`webengine-proprietary-codecs` and `webengine-webrtc-system-openh264` as disabled;
WebRTC is enabled. Previous recorded runtime probes reported H.264 unavailable
and VP9/AV1 available. No new playback or calling test was run here.

Qt documents proprietary codecs as a build-time option, with separate
distribution licensing obligations. No verified stock Windows package enabling
H.264/AAC has been identified by this audit. V0.8 must investigate official
packages and run actual playback tests; it cannot pass merely on page loading.
An engine change still needs separate approval. Unresolved major multimedia
incompatibility blocks V1.0 acceptance.

The installed public `qwebengineprofile.h` exposes cache clearing, visited-link
clearing, cookies and permission management, but no comprehensive per-origin
storage deletion operation. Cookies are not all site data: IndexedDB, service
workers and local storage also matter. Do not label cookie deletion as complete
site-data deletion. A full-profile reset after safe shutdown is a possible
product design, not an implemented or approved substitute for per-site removal.

The enclosing Git root is the user's home, rather than an isolated Prism repo.
Do not stage or commit that home directory. A milestone checkpoint can preserve
only Prism's source and documentation until project-specific Git setup is agreed.

## V0.5 design choice to settle first

### Proposed: native domain-based Shields

Use Qt's supported profile request interceptor and a maintained domain list,
with HaGeZi Light as the candidate. Its filter data is GPL-3.0; ship the license,
attribution, upstream location and reproducible snapshot details separately.
The application's own source license remains a separate owner decision. List
redistribution terms and the limited filtering scope need approval before bundling.

Standard mode would block listed third-party requests while preserving top-level
navigation and essential first-party services. Strict would apply stronger listed
subresource blocking. Both need per-site disable, explicit exceptions and safe
fallback when the top-level site is unknown. Neither mode can guarantee that all
sites work; compatibility failures require a test and an exception or fix.

This approach avoids another filtering-engine dependency. It cannot hide page
elements, implement EasyList syntax or reliably remove ads served from the same
hosts as essential content, including YouTube video ads. UI must describe these
limits and must not imply uBlock Origin or Brave Shields equivalence.

### Alternative: maintained filter-syntax engine

Evaluate a maintained engine and EasyList/EasyPrivacy integration before adding
it. This offers finer URL/context rules and potentially cosmetic filtering, but
adds dependency, licensing, Windows deployment and update compatibility work.
Do not create a home-grown partial EasyList parser and silently ignore rules.
No engine/library has been selected or added.

## Sequential milestone plan

Each milestone requires a focused failing regression first, the smallest code
change, Release build, relevant suites and a real-browser check. Run the complete
CTest command before acceptance. Record results and limitations in a short
milestone report, update version metadata only when its scope passes, and retain
a project-only source checkpoint. Do not advance through a failed gate.

### V0.5 — Privacy and Shields

1. Settle the filtering scope and list licensing above. Add a native Shields
   component and profile-level request interceptor through `prism_core`.
   Keep interception bounded: no disk/network operations or synchronous UI calls
   in the request handler. Use immutable rule snapshots and safe ownership.
2. Test host-boundary matching, internationalized hosts, unknown initiators,
   modes, per-site exceptions and representative analytics/advertising endpoints.
   Verify ordinary first-party requests, redirects and main navigation still work.
3. Add HTTPS upstream updates with bounded response size/time, strict format
   validation, license/provenance metadata, atomic replacement, rollback to the
   last valid list and a bundled offline snapshot. No Prism-operated service.
4. Add an explicit private window with an unnamed OTR profile and isolated native
   new-tab/session/history storage. Destroy all its pages before its profile.
   Downloaded files remain on disk and the UI must say so. Test cookies and
   native writes cannot cross into ordinary browsing.
5. Add cookie listing/deletion, a minimal local visit-history store with clearing,
   and permission review/revocation using Qt APIs. Keep private visits out of
   that store. Resolve comprehensive site-data deletion limitations before
   claiming V0.5 complete. Do not remove live profile folders or bypass APIs
   with hidden CDP.
6. Check Google Search, YouTube, authentication and downloads with Standard and
   site exceptions. Capture observed results; do not promise authenticated/media
   flows that were not completed.

### V0.6 — Searchable Settings

Reuse current theme, accent, new-tab and sleep settings. Add one searchable
Widgets settings window with the requested fifteen sections, validated values,
persisted defaults and controls tied to real browser actions. Add search-engine,
Home/startup, download-location and session preferences. Route new-tab Settings
to the same controls without losing photo features. Test persistence, invalid
inputs and search/Home integration. Implement the minimal working profile,
bookmark and session prerequisites needed by these controls here; V0.7 then
completes their management workflows. Do not defer working controls or ship
decorative toggles to satisfy a version boundary.

### V0.7 — Browser data and essential features

Extend the local history and settings foundations to bookmarks/folders, download
records and session snapshots per ordinary profile. Use atomic writes and
validated, bounded import/export; preserve the legacy default WebEngine directory. Private sessions
must never persist browsing records. Complete management/search/edit/delete,
local profile isolation, restart/session restore, recently closed tabs, tab menus,
download actions and shortcuts. Test corrupt files, duplicate imports, profile
boundaries, unsaved edits and interrupted writes. Never overwrite an existing
profile with imported settings or restore arbitrary local executable paths.

### V0.8 — Multimedia gate

Investigate official stock package options before proposing engine work. Add a
small reproducible local playback fixture with legitimately redistributable
samples and record actual decoded/playing progress for H.264/MP4, AAC, VP9,
AV1/WebM and audio. Test fullscreen, mute and permission revocation. Review
supported Qt desktop-capture selection and media APIs for mic/camera/screen share.
Test YouTube/TikTok and representative calling sites, plus Claude/DeepSeek;
mark signed-in flows/calls unverified unless completed with authorized accounts.
If stock Qt cannot meet required multimedia, document a precise blocker and ask
before any alternate/custom engine build or distribution-license commitment.

### V0.9 — Reliability and packaging validation

Test crash/session recovery, download errors, HTTPS rejection, permission changes,
focus/keyboard access, tab menus, wallpaper credits and compact layouts. Assemble
version/icon metadata and local installer assets. Validate clean install, upgrade
with existing data, uninstall preserving data by default and an explicit optional
data-removal choice. A clean Windows VM/machine may be needed; do not call local
deployment a clean-install test.

### V1.0 — Public acceptance

Obtain the owner's application source-license choice. Provide reproducible Windows
x64 Release build/package instructions, installer, dependency notices and required
source/relinking materials, contributor guide, privacy policy, honest feature and
compatibility documents, release notes and final regression evidence. Keep all 40
photos and their terms offline. Resolve V0.8 blockers and packaging failures before
declaring V1.0 ready. Signing/publishing and any engine replacement require their
own explicit authority; neither is implied by producing a local installer.

## Usage-tool findings and boundaries

Token Optimizer 5.13.71 detected Codex and Claude session logs, an OpenCode
directory, and low-confidence Cursor presence. Fleet scan collected 18 Codex and
one Claude run; OpenCode collection is unsupported and Cursor has no recorded
tallies. Those zero counts do not establish zero usage. No relevant prior Prism
checkpoint was returned by the recovery helper.

Coach estimates installed discovery/startup overhead at 19,497 tokens (4.1% of
the tool's assumed 475,000-token window), including 5,339 skill-inventory tokens.
These are estimates, not direct measurements of this turn's complete prompt.
Its current-session resource score is 49.4/100, with bloated tool results as the
largest reported drag. A compact at a milestone boundary, short excerpts and
bounded logs are appropriate. Keep implementation/checks local and sequential;
no new agents were started for this audit.

The fleet's legacy memory and monthly-token projections conflict with the Codex
state measurement and assume call volumes; they are not verified savings.
Dollar savings are unavailable, rather than $0. Four old child records are marked
open in state; this does not prove ongoing execution or idle billing. Do not
alter memory, close unrelated chats or disable skills based on that label.
Doctor reports absent compact guidance and hooks. Installation would alter Codex
configuration, so it was not done as part of browser development.

No dashboard was opened: the dashboard skill explicitly requires a request to
see it. No global settings, skills, hooks, accounts, engine binaries or browser
data were changed. Generated usage diagnostics are under `build/token-*.json`.

## Sources

- [Qt request interception](https://doc-snapshots.qt.io/qtwebengine/qwebengineurlrequestinterceptor.html):
  requests stall while handled, so interceptors must not block on UI or I/O.
- [Qt profiles](https://doc-snapshots.qt.io/qtwebengine/qwebengineprofile.html):
  OTR profiles, cookie/cache policy and persistent permissions.
- [Qt audio/video codecs](https://doc-snapshots.qt.io/qtwebengine/qtwebengine-features.html#audio-and-video-codecs):
  proprietary codec build configuration and licensing warning. The requested
  versioned Qt 6.12 web pages were inaccessible; current module docs identify
  WebEngine 6.140.0, and local installed headers/configuration were checked.
- [HaGeZi lists](https://github.com/hagezi/dns-blocklists) and
  [GPL-3.0 terms](https://github.com/hagezi/dns-blocklists/blob/main/LICENSE).
- [EasyList licensing](https://easylist.to/pages/licence.html): GPL-3.0-or-later
  or CC-BY-SA-3.0-or-later, with attribution, for relevant repository contents.

