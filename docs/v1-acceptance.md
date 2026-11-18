# Prism preview acceptance — 15 November 2026

## Private codec candidate — 18 November 2026

The custom engine and Prism Release completed. The candidate uses its own
WebEngine DLLs, worker, resource packs and locales. `C:\Qt` and the normal
Prism profile were not used as deployment targets.

- Four local regression suites: 79 passed, zero failed, three opt-in skips.
  All three launcher branches forwarded the exact original arguments.
- H.264/AAC MP4, standalone AAC, VP9, AV1 and PCM WAV advanced their playback
  clocks. Each video decoded frames. Local WebRTC decoded four remote frames.
- The physical device test produced 21 camera frames and a live microphone
  signal. The generated window sharing test decoded three frames with the
  expected colors, then stopped the stream. Picker cancellation, navigation
  and tab close passed in the local regression run.
- A public HTTPS page rendered. Settings search and apply passed, and the
  captured Settings screenshot showed the existing dark layout.
- WebGL used Intel UHD 620 through ANGLE and Direct3D 11.
- The installed candidate restored the saved Example Domain tab after a forced
  exit of an isolated test process and acceptance of the recovery dialog.
  Normal exit cleared the running marker and kept the configured startup mode.
- YouTube and TikTok rendered public pages. YouTube showed a consent screen.
  DeepSeek reached its sign-in page. Claude returned an HTTP 403 verification
  page. None of these page checks establishes streaming or sign-in acceptance.
- The private Windows x64 installer passed upgrade, uninstall and fresh reinstall
  on this computer. Uninstall kept the test profile, and all 183 installed payload
  files matched staging by SHA-256. These include forty wallpapers, filters, Qt
  dependencies, Windows positioning plugins, notices and license texts.
- The installed package repeated all four regression suites (79 passed, three
  skips), strict codecs, local WebRTC and selected-window capture (four frames).
  Prism launched with its packaged engine and no Qt SDK on PATH. An earlier
  installed window-suite run returned 1 without a case log. Its logged rerun and
  the refined install run passed, so the original failure remains unexplained.

### Performance on this PC

Two runs per engine, alternating stock, custom, custom and stock. The same
visible window size, offline wallpaper and synthetic HTTP pages were used.
Each page touched 32 MiB of JavaScript data. Memory is summed process-tree
Private Bytes, including the WebEngine children. Startup begins at benchmark
`main`, so these are not full Windows cold-start measurements.

| Metric | Stock engine | Custom candidate |
| --- | ---: | ---: |
| New tab ready | 2.50–4.00 s | 2.55–2.73 s |
| Wallpaper decoded | 2.59–4.20 s | 2.68–2.82 s |
| New-tab idle memory | 149 MiB | 160–162 MiB |
| One loaded tab | 195–202 MiB | 202–208 MiB |
| Five loaded tabs | 439 MiB | 454–457 MiB |
| Ten loaded tabs | 756–757 MiB | 786 MiB |
| Ten tabs, Maximum Savings | 285–286 MiB | 295 MiB |
| Ten-tab switch, median frame acknowledgement | 39–46 ms | 38 ms |
| Wake switch, median frame acknowledgement | 47–54 ms | 48–50 ms |

All 40 fixture navigations succeeded. The candidate used about 4% more
private memory with ten loaded tabs. Maximum Savings discarded nine eligible
background tabs while leaving the current one active and saved about 62% in
this workload. In the candidate's retention run, closing after cycling wakes
left 311, 194 and 195 MiB at 5, 30 and 60 seconds. This is one synthetic
retention run, not a general leak or gaming benchmark. Startup ranges overlap.
There is no new Game Mode or Chrome speed claim. Raw data is in
`Prism-WebEngine-Lab/benchmark-{stock,candidate}-{1,2}.json`.

The candidate new-tab idle samples used 0.62–1.24% of one logical core, versus
0.62–1.55% for stock. With Maximum Savings, the candidate used 1.56–2.07% of one
core while the remaining active fixture continued its timer. This measures the
browser process tree, separate from Windows or another game's CPU usage.

The sections below describe the older stock preview. The new results resolve
its local codec and selected-window playback failures. They do not yet prove
an authenticated call, site streaming or public-release acceptance. Public
codec redistribution rights, a final correspondence/relinking review and a test
on clean Windows remain open. The tested installer identifies itself as a
private 0.7.0 candidate while these release gates remain open.

## Working implementation

V0.5–V0.7 add domain-based Shields with safe upstream updates and exceptions,
isolated private windows, local privacy controls, searchable functional Settings,
isolated local profiles, bookmarks with searchable folder labels and transactional
Prism JSON import/export, searchable history with selective deletion, persistent
tab-URL sessions and recently closed tabs, tab menus and download management.
The existing black layout, forty offline photos, custom wallpapers, themes,
navigation, sleeping, Keep Awake and back/forward cache remain in place.

Bookmarks use logical folder labels, not a hierarchical empty-folder tree.
Session restoration restores URLs, not unsaved forms or full navigation history.
The coalesced session snapshot can lose the final second of changes on a crash;
an isolated forced-termination test now verifies the restart recovery prompt.
Restoration/private isolation have regression coverage; accepting that prompt
end-to-end remains unverified. No user-data reset was run.

## Verification

- Release: compatible MSVC v143 Release build passed; log `build/v09-release-build.log`.
- Regression: `test.cmd`; four suites passed, 95.19 seconds; 76 passed, zero
  failed. Six optional cases skipped: external sites, real HTTPS, actual window
  capture and three source-picker cases gated off for the stock engine's confirmed
  capture deadlock. Earlier explicit HTTPS example.com
  rendering passed; skipping that optional diagnostic here is not new live-site evidence.
- Local playback: VP9 and AV1 decoded 5/6 frames with advancing playback clocks;
  WAV clock advanced. H.264/AAC MP4 and AAC audio failed with media error 4.
  Reproduce using `tests/generate-media.cmd` then the Release
  `build/prism_media_probe.exe`; inspect `build/media-results.json`.
- Private isolation, data validation, imports, URL encoding, completed automatic
  downloads without overwrites, tab lifetimes/wake, BFCache, permissions and local
  wallpapers have automated coverage. This is not a clean-machine certification.
- Portable deployment verified: the packaged executable matches the tested
  Release executable; its WebEngine DLL matches the installed stock engine;
  all forty WebP files and the credits match their project copies. Filter data
  is included. Evidence: `build/package-verification.json`.
- `launch.cmd --private` launched the packaged executable with Qt removed from
  the launching process's PATH. Observed window: "New tab — Prism · Private"
  (PID 2288), using the packaged QtWebEngineProcess. This proves local app-local
  startup, not operation on a clean Windows machine. The preview was left open.
- Google presented a challenge; Shields was not demonstrated to cause it.
  Treat as an external compatibility limitation. No bypass or repeated probing.
- Later physical camera/mic and local WebRTC probes passed. The owner reports
  Meet camera, microphone and two-way audio/video working, but screen sharing
  failing. This is user-reported evidence for the stock preview, not acceptance
  of the new engine. YouTube/TikTok streaming, DRM and Claude/DeepSeek authenticated
  flows still lack final-build acceptance. See `docs/v08-release-work.md`.

## Reproduction and packaging

Use the installed Qt 6.12.0 MSVC x64 and compatible MSVC v143 14.44 toolchain.
Run `build.cmd --incremental`, `test.cmd`, then `package-preview.ps1`.
The script stages app-local Qt and Microsoft dependencies, filter licenses,
all forty offline photos and credits. It creates
`dist/Prism-0.7-Preview-Windows-x64.zip` and `dist/PrismPreview/Prism.exe`.
Keep the whole package directory together. `launch.cmd --private` opens the
preview separately from an existing ordinary Prism window. The default profile
path remains unchanged; `--profile work` creates a separate local profile.

## V1.0 release blockers

1. Stock Qt lacks H.264/AAC. The supported build-time proprietary-codec option
   needs a compatible maintained engine build and applicable licensing. No codec
   DLL injection, Chromium flags, external-player substitution or binary patching
   was used. Installed Qt is unchanged. See the official
   [Qt codec documentation](https://doc.qt.io/qt-6/qtwebengine-features.html#audio-and-video-codecs)
   and the earlier `docs/v021-report.md` feasibility investigation.
2. Stock-engine desktop sharing has a confirmed Windows wait-chain deadlock.
   A separate Qt names-only source-list patch is under test in the isolated build;
   it is not a proven fix. Production screen sharing defaults off until validated.
   Real final-engine calling, streaming and authenticated-site acceptance is incomplete.
3. The owner has now installed Inno Setup 7.1.1, resolving the earlier blocked
   compiler installation. A preview installer compiled successfully and installed
   locally into an isolated test directory. Its executable matched Release,
   stock engine matched installed Qt, all forty photos were present, and its
   rendered new tab and clean closure were observed. Same-version upgrade and
   uninstall also passed locally, preserving the isolated profile data. A default
   local installation launched app-local dependencies successfully. Testing on a
   separate clean Windows machine remains unverified. See `docs/v08-release-work.md`.
4. Qt/Chromium corresponding-source, relinking and other redistribution
   obligations need a verified public-release compliance package. Seven supporting
   Qt source archives and full module license texts have now been collected;
   the custom WebEngine source and patch remain in the private lab. The bundled
   notices identify obligations; links alone do not complete them. Code signing,
   clean-machine validation and public publishing have not been performed.

The portable preview is useful for local testing. These blockers prevent calling
it a fully ready V1.0 under the supplied master prompt. No performance improvement
or Chrome comparison is claimed for this work.
