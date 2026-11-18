# Prism Browser V1

## Human and AI contributions

The project owner's estimate for Prism's own application code is 60% human-written
and 40% AI-written. The owner also estimates that 60% of the total code was
human-written and subsequently edited with AI assistance. That edited share
overlaps the human-written 60%; it is not an additional share.

These are owner-provided estimates, not an independently measured authorship
audit. They exclude Qt, Chromium and other third-party code, which retain their
original authorship and licences. Human authorship and AI editing describe
different stages of development.

V0.5 adds domain-based Shields, isolated private windows and local privacy/history
controls. V0.6 adds searchable Settings, search/Home/download configuration,
local profiles, bookmarks and URL session restoration. V0.7 adds logical folder
labels, transactional Prism JSON bookmark import/export, download records and
management, history search, persistent recently closed web tabs and tab menus.
Folder labels are edited with bookmarks and searchable; this preview does not
provide a hierarchical empty-folder manager or Chrome HTML bookmark import.

Run `package-preview.ps1` after a successful Release build to produce an app-local
portable distribution in `dist/PrismPreview`, including offline photos, filter
licenses, the WebP plugin and signed Microsoft runtime files. Close older ordinary
Prism windows before opening its `Prism.exe`. Use `Prism.exe --private` for an
isolated preview. `Prism.exe --profile work` opens a separate local profile.
Settings is **Ctrl+,**, bookmarks **Ctrl+Shift+O**, history **Ctrl+H**, downloads
**Ctrl+J**, bookmark current web page **Ctrl+D**, private window **Ctrl+Shift+N**.

This is **not V1.0 ready**. Stock engine H.264/AAC, final calling/media acceptance,
clean-machine validation and distribution-license acceptance remain unresolved.
Local preview install, upgrade and uninstall have passed, preserving test profile
data. Installer source is `installer/prism.iss`; Inno Setup 7 is installed.
These local results do not establish V1.0 acceptance. See
`docs/v1-progress.md`, `PRIVACY.md` and `THIRD-PARTY-NOTICES.md`.

A small desktop browser built with C++17, Qt 6.12 Widgets and the installed
Qt WebEngine. The Windows build uses MSVC v143 x64. The stock preview uses prebuilt
Qt. The separately approved codec build is isolated in `Prism-WebEngine-Lab`,
outside this project, and never installs into `C:\Qt`.

## Open Prism

Double-click **launch.cmd** in this folder. It selects `dist\PrismPreview`, then
the older `dist\Prism`, then `build`. Installed previews also have a Prism shortcut
and executable under `%LOCALAPPDATA%\Programs\Prism`. Keep packaged files together.
The launcher preserves options such as `launch.cmd --private`,
`launch.cmd --profile work`, or `launch.cmd --profile work --private`.

## What is included

- Flat neutral chrome and a clean new-tab page, with no gradients, decorative effects or animations.
- **… > Appearance > Accent color:** five presets or a custom color, saved between launches.
  Color changes preserve the current page and search draft. Accents are limited to small highlights.
- Real websites, movable tabs, page titles, favicons, and a new-tab button.
- Back, Forward, Reload/Stop, and Home with normal page history.
- Rounded address field for URLs, local files, and Google searches.
- A new-tab page with Google search, a live 12/24-hour clock, editable favicon shortcuts,
  local-photo imports, hourly shared wallpaper rotation, credits and fullscreen controls.
- New-tab Settings supports built-in, custom, mixed, fixed and no-wallpaper modes.
  Forty licensed 1440p WebP photographs are included and work offline, ten per category.
  Keep the `wallpapers` folder beside Prism.exe. See [credits](wallpapers/CREDITS.html)
  and [wallpaper setup](wallpapers/README.md). Additional hosted catalogs remain optional.
- System, Light, and Dark appearance under **… > Appearance**. System is the default.
- Last-used tab selection when closing the active tab, even after reordering tabs.
- Reopening up to 20 recently closed tab URLs, persisted for ordinary profiles.
- Inline address/load errors with retry, plus WebEngine's page error details.
- **… > Tab resource savings:** Balanced freezes eligible inactive pages after
  five minutes and preserves their state. Maximum Savings freezes after one
  minute, then releases untouched eligible web pages after ten minutes if Qt recommends it;
  those pages reload on selection. Freezing alone does not release renderer RAM.
  Off resumes frozen tabs and disables automatic sleeping; already released pages
  reload when selected, rather than starting every background page at once.
- **Keep this tab awake** protects background work you want to continue.
- Download save dialogs, progress and cancellation. Active downloads protect
  tabs from sleeping and prevent accidental tab/window closure.
- Origin-labelled consent for camera, microphone, location and clipboard access.
- HTTP/proxy sign-in prompts and website fullscreen (Esc exits).

The sleep policy respects WebEngine's recommendation and protects the selected
tab, audible/loading/inspected pages, pending or granted permissions, and all tabs
while a download is pending or running. Off wakes frozen pages and stops the
policy timer. Permission grants conservatively keep their tab awake until closed.
WebSockets, uploads and arbitrary background jobs cannot all be identified safely;
use Keep this tab awake for these. Internal new tabs are frozen rather than
discarded, preserving search drafts. Tabs receiving page input stay frozen for
their lifetime, protecting edits Qt can miss, including across history restores.
Qt also protects recognized forms and PDFs. Arbitrary application state is not
guaranteed in Maximum Savings; use Keep Awake for ongoing background work.

Favicons use the profile's existing icon cache, icons seen while browsing, and
bounded downloads from websites and their declared icon CDNs. No favicon proxy
is used. Failed icons have a retry delay and a neutral globe fallback. Dark
favicons get a light backing to remain visible on the black sidebar.

See [V0.4 results](docs/v04-report.md) for process-tree measurements and limitations.

See [V0.2 report](docs/v02-report.md) for measured results and compatibility limits.
These exploratory measurements do **not** establish faster startup, lower RAM,
or an overall speed advantage over Chrome.

V0.2.1 enables Chromium's supported back/forward cache for completed web pages in
Balanced and Off modes. Eligible history navigation restores the existing
document instead of rebuilding it. Maximum Savings prevents new admissions;
existing cache entries expire under Chromium's own policy. Retained real pages
can cost RAM. See the [V0.2.1 engine investigation](docs/v021-report.md) for the
targeted navigation gain, noisy general measurements and the separate engine lab.

Reopening restores the tab's URL, not its form entries or full history.
Closing the last tab opens a fresh new-tab page. Use the window's close button to quit.
Website cookies/cache, appearance and window size use Qt's normal per-user storage.

## Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| Ctrl+T | New tab |
| Ctrl+W | Close tab |
| Ctrl+Shift+T | Reopen closed tab |
| Ctrl+L, Alt+D, F6 | Select address field |
| Ctrl+R, F5 | Reload |
| Esc | Stop loading / exit website fullscreen |
| Alt+Left / Alt+Right | Back / Forward |
| Alt+Home | Home |
| Ctrl+Tab / Ctrl+Shift+Tab | Next / previous tab |

## Build and test

Installed dependencies used on this machine:

- Qt: `C:\Qt\6.12.0\msvc2022_64`
- Qt Widgets, WebEngineWidgets, WebChannel, Positioning, and Qt Test.
- MSVC v143: 14.44.35207, compiler 19.44.35230, x64.
- CMake and Ninja from `C:\Qt\Tools`.

Run **build.cmd** to configure and build Release. It discovers Visual Studio
Build Tools using vswhere and selects the installed 14.44 toolset explicitly.
Run **test.cmd** for the deterministic tests, or **test.cmd network** to also
check a real public HTTPS website. Test results are under `build\Testing`.

After a successful Release build, run **package-preview.ps1** to create
`dist\PrismPreview` and `dist\Prism-Browser-1.0.0-Windows-x64.zip`. It includes
app-local Qt, signed installed Microsoft runtime redistributables, filters,
the WebP plugin, notices and all forty offline photos. Close an executable before
replacing its package files. `deploy.cmd` remains an older development-only path.

For the stock preview installer, run `package-preview.ps1 -Installer`, then
compile `installer\prism.iss` with Inno Setup 7. Its default output is
`dist\Prism-Browser-1.0.0-Setup.exe`. Engine candidates use a separate staging
folder, installer identity and profile; do not replace working Qt libraries with
an unvalidated build. Production screen sharing defaults off until the separate
engine passes source-consent and actual selected-window capture tests.

Run `tests\launcher_tests.ps1` in PowerShell for the focused Windows launcher
regression. It uses the installed MSVC toolset and isolated stubs, without opening
real browser profiles. `prepare-source-bundle.ps1` downloads checksum-verified
Qt sources. Exact custom-engine provenance, patches and codec redistribution
rights remain separate requirements; a local build is not public clearance.

For later edits, `build.cmd --incremental` skips the explicit configure step;
Ninja still regenerates CMake files when necessary.

Generic CMake build, from a compatible MSVC x64 developer prompt:

```bat
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.12.0\msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

## Quick playtest

1. Open Prism. Search from the start page, then enter `https://example.com` in the address field.
2. Open three tabs. Switch between them, then close the active one. Check that the last-used tab returns.
3. Press Ctrl+Shift+T. Try Back, Forward, Reload, and Home.
4. Switch between Light, Dark, and System appearance in the menu.
5. Enter `https://bad host.com`. The current page should remain, with a helpful error.

Only Windows is built and tested. macOS/Linux builds remain unverified.
The owner reports Meet camera, microphone and two-way audio/video working in the
stock preview, with screen sharing failing. The stock engine plays the tested
VP9/AV1/WebM and WAV fixtures; H.264/AAC MP4 and AAC audio fail. The custom engine
must pass actual playback before acceptance. DRM and final authenticated-site
acceptance remain open. No overall performance advantage over Chrome is claimed.
See [current release work](docs/v08-release-work.md) and [acceptance](docs/v1-acceptance.md)
for the precise tested build, skipped checks and remaining limitations.
