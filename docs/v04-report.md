# wnloads, permissions, HTTP auth,
cookies, sleeping/Keep Awake, edited inputs, custom/mixed wallpapers, preferences,
compact layouts, icon discovery/cache and page-object cleanup passed tests.
No crash occurred in the measured runs and wake cycles. This does not prove
the absence of all leaks or establish overnight reliability. After rapid live
site wake/close with only a two-second settle, private commit was 745 → 935 MiB;
reloads and remaining engine processes were still involved. The cause of that
retention is not established, and no post-close memory/leak improvement is claimed.
The raw wakeSwitch field acknowledges rAF only, not completed discard reloads.

YouTube rendered its signed-out homepage. TikTok rendered but reported trouble
playing video. Claude and DeepSeek rendered sign-in pages; authenticated chat,
account flows and full media playback were not verified. No challenge was bypassed.
The installed engine reports H.264 unavailable, VP9 and AV1 available. Its build
configuration has proprietary codecs disabled. Fixing that needs a separately
licensed/configured engine build, not downloaded codec DLLs or UI tricks.
Prism V0.4 — performance and polish

Release built, deployed and launched. All three test suites passed (87.30 s).
An additional repeated discard/wake/history/close test passed three cycles;
the closed QWebEnginePage object was released. Final visual inspection confirmed
all six default sidebar shortcuts have real icons, including Discord and Notion.

## Changes

- Hidden new-tab pages no longer receive serialized wallpaper catalogs. Repeated
  activation does not repeat initialization. Clock formatters and minute updates
  are reused, and unchanged wallpaper URLs are retained. Only the selected image
  is assigned; the forty-photo offline pack and hourly selection are unchanged.
- One deadline timer replaces the repeating fifteen-second sleeping poll; it
  stops when no eligible transition remains. Balanced retains frozen documents
  and BFCache. Maximum Savings freezes after one minute and discards untouched,
  eligible HTTP(S) pages after ten minutes only when Qt recommends Discarded.
  Those pages reload on selection. Off resumes frozen pages without reloading
  every already-discarded background tab simultaneously.
- A regression exposed Qt recommending discard for an edited standalone input.
  Native page-input observation now conservatively protects interacted tabs for
  their lifetime, including across history restores. No website input scripts
  were injected. Active/audio/capture/inspected/loading/download/permission and
  Keep Awake protections remain. New tabs and form-submitted pages are not
  discarded. Arbitrary background work still requires Keep Awake.
- Favicons use the browser profile cache and observed browsing icons, then bounded
  root-icon/HTML discovery with canonical redirects and site-declared HTTPS CDNs.
  Catalog/image-download origin restrictions remain unchanged. No favicon proxy
  or fabricated site logo is used. Dark icons get a light backing; unavailable
  icons use a neutral globe. Native fallback decoding accepts raster formats;
  Chromium handles cached SVG icons. Successful icons persist and failed ones
  have a retry delay.
- Tabs have tighter bounds, right-side elision and aligned close buttons. Compact
  new-tab layouts preserve the clock, search, credits and cover-sized wallpaper.
  Browser chrome stays black; no effects or animations were added.

## Measurements

Windows 11, i7-8665U, 4 cores/8 logical processors, 15.8 GiB installed RAM.
The Release harness runs the real BrowserWindow and Qt engine with an isolated
off-the-record profile, fixed wallpaper, identical viewport, and no shortcut
network traffic. It includes the entire descendant process tree. The original
binary is preserved as build/prism_benchmark_v032_v04harness.exe; raw results
are build/v04-before-*.json and build/v04-after-*.json. No build or test suite ran
concurrently with those measurements. Other user applications remained open.

**Private memory below is committed private process memory, not physical RAM.**
Working-set sums include shared pages multiple times. CPU percentages are of one
logical core; divide by eight for whole-machine capacity. CPU samples match PID
and creation time, rather than subtracting totals across exited processes. All
reported workload CPU intervals had zero process churn.

Synthetic HTTP fixture: 150 blocks, a 32 MiB touched JS allocation and a periodic
observable calculation per tab. Two fresh-process runs per version; table values
are the medians of the two samples. This is not a claim about typical websites.

| Fixture | Private MiB before → after | Working-set sum MiB before → after | Idle one-core CPU % before → after |
| --- | --- | --- | --- |
| 1 loaded tab | 186.8 → 182.6 | 360.9 → 358.0 | 0.5 → 0.8 |
| 5 loaded tabs | 430.8 → 431.5 | 829.3 → 831.6 | 2.1 → 2.6 |
| 10 loaded tabs | 759.1 → 763.8 | 1435.2 → 1444.0 | 10.6 → 9.6 |
| 10 tabs, Maximum Savings | 750.1 → 274.9 | 1428.6 → 449.5 | short samples; not a general CPU claim |

Maximum Savings released nine unused main-frame renderers (11 total processes
became 2), approximately 63% less private commit in this fixture. Sleeping ages
were advanced to test the real policy without waiting an hour; this is not a
long-duration soak. Balanced alone retains renderer memory by design.

The same ten-new-tab workload used 588.9 → 505.2 MiB private commit. Median
synchronous switch handler time was 342.4 → 6.4 ms; switch-to-renderer-rAF
acknowledgement was 1205.3 → 52.6 ms (20 samples). This is a renderer response
measurement, not physical display latency. It directly targets redundant new-tab
catalog broadcasts. This workload has one before/after run, not broad statistics.

Ordinary ten-web-tab frame acknowledgement varied from 28.5–162.2 ms before and
43.0–45.9 ms after: no reliable general web-tab speedup is established. Startup
main-entry to decoded wallpaper was 3169–4957 ms before and 3370–3374 ms after in
the paired synthetic runs. OS caches were warm; true cold startup and persistent
user-profile startup were not measured. Single-new-tab private commit stayed
about 136–143 MiB, with roughly 0.6–1.9% one-core idle CPU in those runs. Small
CPU differences are close to Windows counter granularity and sample variability.

The five live URLs were YouTube, TikTok, Claude, chat.deepseek.com and example.com,
in that order, with identical fresh-profile intervals. Active five-tab private
commit was 1042.1 → 885.9 MiB; after Maximum Savings it was 967.4 → 532.9 MiB.
These are single samples of changing websites, not a causal active-page RAM gain.
loadFinished times in milliseconds were YouTube 13105 → 5866, TikTok 12412 →
13143, Claude 4511 → 3663, DeepSeek 806 → 658, example.com 615 → 491. Network
and server responses vary, so these do not establish faster website loading.

An additional read-only snapshot of the pre-existing user browser counted eight
processes: 778.4 MiB private commit, 754.4 MiB working-set sum, 0 CPU ticks over
five seconds. Browser memory was 292 MiB; five renderers accounted for most of
the remainder, plus audio/video-capture services. Its browsing state differed
from the harness and is not used for before/after percentages.

## Reliability and remaining limits

Navigation/history, BFCache document retention, do
Fresh graphics probing reports hardware ANGLE / Intel UHD Graphics 620 / D3D11.
Security, GPU and engine launch defaults were preserved; no sandbox/TLS/isolation
disable flags, process limits, forced working-set trimming or cache purges were
used. Live logs still contain WebRTC STUN DNS failures and some WebGL errors;
their cause is not established. No claim of beating Chrome is made.

Launch with launch.cmd. Keep the packaged wallpapers folder and licenses beside
Prism.exe. Qt lifecycle reference: https://doc.qt.io/qtwebengine/qwebenginepage.html#recommendedState-prop
