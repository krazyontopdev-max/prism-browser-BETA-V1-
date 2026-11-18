# Prism V0.2 — Windows results

## What changed

The existing C++17/Qt Widgets browser was upgraded in place. Reload/stop icons
are cached instead of redrawn on every progress update. Duplicate tab activation
work was removed. Web pages and the shared persistent profile are retained.
No HTTP user-agent spoofing, cache clearing, security bypass, GPU-disable flag,
third-party performance dependency or Chromium rebuild was introduced.

Balanced freezes eligible inactive tabs after five minutes; Maximum Savings
uses one minute; Off disables the policy and wakes pages. One coarse 15-second
timer runs only when savings is enabled with multiple tabs. Qt's recommended
lifecycle state, visibility, loading, audio, DevTools, permission state, downloads
and per-tab Keep awake prevent unsafe automatic freezing. Frozen retains the
DOM/JavaScript state and renderer; it is not a RAM-releasing discard operation.

Downloads now have asynchronous save dialogs, progress, cancellation and close
protection. Camera/microphone/location/clipboard requests require explicit consent
with an origin shown. Basic HTTP and proxy authentication prompt for credentials;
Qt requires that particular response synchronously, using a dialog event loop.
Website fullscreen works and Esc exits it. Normal popup/new-tab behavior and TLS
verification remain in place.

## Problems investigated

- No automatic reload loop, repeated page recreation, injected website scripts,
  disabled GPU setting, cache disabling or request interceptor was found in V0.1.
  UI icon repainting and duplicate activation were confirmed unnecessary work.
- Google unusual traffic was reproduced with a single diagnostic search.
  The warning was preserved. Its exact server-side cause is not established.
  Chrome's existing cookies, identity, engine and session differ from Prism's;
  this does not establish which difference caused Google's decision.
  [Google's explanation](https://support.google.com/websearch/answer/86640?hl=en)
  describes network/automated-traffic causes; it does not provide a client-side fix.
- YouTube loaded JavaScript content and its logged-out consent/history screen.
  That run made 128 resource requests and had no completed content thumbnails;
  this is not evidence of broken GPU rendering or a fixed thumbnail delay.
- WebGL reported **Intel UHD Graphics 620 through ANGLE Direct3D11**. Hardware
  graphics are available; hardware video decoding was not verified.
- The installed engine's media probe returned VP9 `probably` and no H.264 support.
  This is a real compatibility limit of these prebuilt binaries. Proprietary
  codecs/DRM cannot be promised by changing browser settings.
  [Qt media documentation](https://doc.qt.io/qt-6/qtwebengine-features.html#audio-and-video-codecs).
- Missing download and permission handlers were confirmed application gaps and
  implemented. There is no demonstrated cookie-storage defect to repair; shared
  cookies and redirects now have regression coverage.

## Measurements

These are exploratory runs on this Windows 11 laptop, not a controlled laboratory
benchmark. Other applications remained open. No OS cache flush or reboot was
performed. A true cold-start result is **unavailable**.

The Release harness uses an off-the-record test profile, local HTTP pages with
100 gradient blocks, an input and a 100 ms JavaScript interval. Memory covers the
browser process and descendants. Private bytes avoid counting shared DLL pages
multiple times; working-set sums remain in the raw JSON. CPU is percent of one
logical core, not percent of the whole machine. This is a timer workload, not a
truly idle browser. The unused math loop may be optimized away by the JS engine.

| Check | V0.1 | V0.2 | Interpretation |
| --- | ---: | ---: | --- |
| Main entry to first new-tab load | 2,007 ms | 1,972 and 3,003 ms | Warm OS caches; no reliable startup gain |
| 1 tab, private memory before sleeping | 142.6 MiB | 139.9 MiB | Similar |
| 5 tabs, private memory before sleeping | 260.1 MiB | 258.5 MiB | Similar |
| 10 tabs, private memory before sleeping | 404.3 MiB | 409.6 MiB | Slightly higher in this run |
| 5 tabs, mean synchronous switch handler | 4.12 ms | 5.41 ms | No measured improvement |
| 10 tabs, mean synchronous switch handler | 5.69 ms | 6.84 ms | No measured improvement |

The second V0.2 run explicitly verified **4/5 and 9/10 tabs frozen** after aging
their inactivity timestamps. The selected tab stayed Active. Private memory
after freezing was 254.8 MiB (5 tabs) and 406.0 MiB (10 tabs): no major RAM saving.

The 10-tab CPU sample fell from 16.08% to 6.22% after freezing. However, the five-tab
sample rose from 2.06% to 3.10%, and the one-tab control rose from 1.03% to 7.76%
without any tab freezing. These short samples are noisy and do **not** establish a
reliable CPU reduction or regression. Frozen-state behavior is verified; an
overall performance improvement is not.

The first V0.2 scrolling probe collected 60 animation-frame intervals averaging
30.08 ms while requesting small scrolls. This is not input-to-display latency,
and no matching V0.1 scrolling baseline was collected. The second run collected
60 intervals averaging 29.47 ms. No smoother-scrolling claim is made.

### Chrome reference

Installed Chrome 157.0.8103.51 was run with a separate fresh profile and the same
local page workload. Its headless content process tree used 315.1, 388.6 and
414.2 MiB private memory with 1, 5 and 10 tabs. This differs from a visible Widgets
browser, engine version and process model; it is a reference, not an equal
benchmark. Chrome CPU samples are excluded: process turnover made the original
aggregate cumulative-counter subtraction invalid (including a negative result).
The reference script now uses per-process deltas for future runs; it still cannot
count CPU from a process that exits between samples.
There is no evidence here that Prism beats Chrome.

### Invalid samples excluded

The original V0.1 harness checked isLoading immediately after load(), before the
asynchronous load-start signal could arrive. Its public-site and batch-load
timings are invalid and excluded. The same race affected metadata snapshots in
the first V0.2 public-site run; the settled document text confirms content loads,
but those timings and early title/URL snapshots are not used as speed evidence.
This harness issue does not cause the production browser to skip loading.
The public-site harness now waits on loadFinished and takes metadata after the
settling interval for future runs. The recorded old samples were not rewritten.

The benchmark was also corrected to wake/reset tabs between workload sizes and
to settle visibility updates before measuring frozen-state counts.

## Website and regression coverage

- Example Domain: real HTTPS title and document text, including the automated test.
- Wikipedia: settled article text and seven completed images.
- YouTube: actual JavaScript UI/consent/history screen. Playback and thumbnail-feed
  performance are not verified; the consent barrier was not bypassed.
- Google: unusual-traffic page reproduced, **not** a successful search-results test.
- Local HTTP fixtures: redirects, shared cookies, authenticated navigation,
  downloads to a chosen file, invalid input, failed/stopped loads, popup tabs,
  titles, navigation, themes, shortcuts, moved-tab recency, sleep/wake with
  preserved JavaScript state/no reload, Keep awake, Off and permission denial.

Final direct GUI suite: 19 passed, zero failures/skips. The file dialog emitted
one harmless Qt font-size warning; the saved file contents were verified.
Final CTest run: both suites passed in 33.23 seconds. The deployed Release binary
matched the built executable's SHA-256 and was launched successfully; its V0.2
start page was inspected on the desktop.

## Remaining limits and recommendation

No universal safe detection exists here for important WebSocket activity,
uploads, silent jobs or every site's unsaved state. Use Keep this tab awake for
such work. No automatic page discard is used. Capture/audio protections rely on
Qt's signals/recommendation; real calls, DRM playback and device permissions have
not been tested. Screen sharing, web notification presentation, mouse lock and
local-font permissions are not implemented. Downloads do not resume across app
restarts. Closed-tab restoration still restores URL only. Clean-PC, macOS and
Linux deployment remain untested.

Keep the current Qt implementation for this release: the measurements do not
justify a rewrite. If current Chromium compatibility, H.264/DRM and matching the
installed system browser are essential, evaluate a separate Windows WebView2
prototype before deciding. Its Windows-only engine would require a separate
cross-platform strategy. No architecture replacement was made.

## Reproduce and launch

Run `build.cmd`, then `test.cmd network`. With Qt's bin directory on PATH, run
`build\prism_benchmark.exe <absolute-output-json-path>`; set
`PRISM_NETWORK_TESTS=1` for the optional public-site diagnostics. The tab age is
artificially advanced only by the test harness. The preserved
`build\prism_benchmark_v01.exe` is the original baseline harness binary.

`node tests\chrome-benchmark.mjs` runs the isolated headless Chrome reference.
Node is used only by this optional measurement script, not by Prism.

Raw files: `build\baseline-v01.json`, `build\results-v02.json`,
`build\v02-local-confirmation.json`, `build\chrome-results.json`,
`build\v02-tests.txt`, `build\v02-ctest.txt`, `build\v02-build.txt` and
`build\v02-deploy.txt`. Some website diagnostic text can contain network details;
review before sharing these raw files.

Double-click **launch.cmd**, or **dist\Prism\Prism.exe**. The existing profile,
cookies and theme settings are preserved. Choose savings mode from the **…** menu.
