# Prism V0.2.1 — WebEngine investigation and results

## 1. Baseline and measurement scope

Read `docs/v02-report.md` before making changes. Preserved the V0.2 benchmark
binary and collected new Windows 11 Release measurements before enabling the
engine setting. Raw results are in `build/v021-baseline.json`,
`v021-baseline-extended.json`, `v021-before-clean.json` and `v021-after-clean.json`.

The extended harness waits for loadFinished instead of sampling the asynchronous
isLoading flag too early. It also samples the idle new-tab page over ten seconds.
Both versions use an isolated off-the-record profile, not the user's live profile.
The tab workload is the same local HTTP fixture: 100 blocks and a 100 ms JS timer.
It is not a realistic mixed-site RAM benchmark. The initial math loop's result is
unused and can be optimized away. Synchronous tab-handler time is not display latency.

| Sequential sample | V0.2 | V0.2.1 |
| --- | ---: | ---: |
| Warm-cache main entry to new-tab load | 2,873 ms | 2,573 ms |
| New-tab private memory | 122.0 MiB | 120.9 MiB |
| New-tab working-set sum | 284.7 MiB | 280.4 MiB |
| 1 loaded tab, private memory | 232.2 MiB | 133.0 MiB |
| 5 loaded tabs, private memory | 336.0 MiB | 261.4 MiB |
| 10 loaded tabs, private memory | 502.7 MiB | 420.0 MiB |
| 10 loaded tabs, working-set sum | 1,190.7 MiB | 1,081.4 MiB |
| 5-tab synchronous switch handler, mean | 39.17 ms | 7.72 ms |
| 10-tab synchronous switch handler, mean | 16.07 ms | 9.33 ms |

**These numbers do not establish general RAM, startup, CPU or tab-switch gains.**
The V0.2 sequential sample had a large unexplained transient: 222.9% of one core
during the new-tab sampling interval. The earlier extended baseline recorded
2.03%, and V0.2.1 recorded 0.156%. Aggregate process CPU counters also become
unreliable when processes exit or change between samples. Do not interpret this
as a 99% CPU improvement. The initial extended baseline overlapped a later build
and is secondary evidence; the subsequent before/after runs were sequential with
no Prism compilation running, but other applications remained open.

The earlier V0.2 extended ten-tab private result was 424.8 MiB. Against that,
420.0 MiB is essentially unchanged, consistent with the previous roughly
404–410 MiB history. Private memory is committed private process memory, **not**
total system RAM usage. Working-set sums include shared pages more than once.
The process tree, including Qt renderer children, is counted in both measures.

True OS-cold startup is unavailable: no reboot, system cache purge or standby-list
manipulation was performed. Fresh processes ran with already warm OS/driver caches.
There is no measured improvement in first website loading or scrolling yet.

## 2. Confirmed bottleneck and measured improvement

Qt's source disables Chromium's back/forward cache by default. Without it,
returning to an eligible page runs that document's initialization again even
when the HTTP response is already cached. This is separate from disk caching.

A separate Release executable tested the unchanged installed engine with the
setting off and on. Two local eligible pages each deliberately spend 100 ms in
initial JavaScript. Eight Back/Forward pairs were measured for each variant.

| Engine probe | Default off | Enabled |
| --- | ---: | ---: |
| Median of 16 history navigations | 222 ms | 57 ms |
| Actual pageshow.persisted restoration | false | true |
| Private process-tree memory at end | 109.4 MiB | 108.0 MiB |
| HTTP requests | 3 | 3 |

This is approximately 74% less measured navigation time **in this synthetic
fixture**. HTTP caching already avoided repeat requests in both variants; the
benefit was retained-document restoration, not faster networking. Similar memory
in this small fixture is not evidence that caching real large documents costs
no RAM. A reverse-order repeat also restored documents and was faster, but
overlapped a test compile and is secondary evidence.

## 3. Implemented supported engine optimization

`BrowserWindow::configureNavigationCache` enables
`QWebEngineSettings::BackForwardCacheEnabled` after HTTP/HTTPS pages finish loading
in Balanced and Off modes. This calls Qt's supported Chromium integration rather
than patching browser UI speed indicators or injecting website scripts.

New-tab/local pages start with the default disabled. Enabling only after a web
load avoids retaining the departing internal new-tab document. Maximum Savings
disables new BFCache admission. Changing modes updates existing pages. Cached
entries already retained can remain until Chromium evicts them; switching modes
does not promise an immediate memory flush.

Chromium still decides eligibility, expiry, memory-pressure eviction and security
rules. The pinned source includes cache-size limits and a normal 600-second TTL;
feature parameters/platform policy can affect actual limits. Prism does not
override them or impose an unverified heap/process limit. Retained history
documents can increase RAM on real browsing sessions. Maximum Savings is the
option for avoiding new navigation-cache retention.

No page/profile pool, blanket cache clearing, disabled sandbox, disabled TLS,
disabled site isolation, fake Chrome identity or GPU-disable flag was added.

## 4. Underlying engine architecture investigated

Pinned source inspection covered:

- `src/core/web_engine_context.cpp`: one-time Chromium initialization, process
  arguments, in-process network/tracing services and Qt's in-process GPU path.
  Its GPU-disable path depends on the selected graphics implementation; it was
  not activated as an optimization. Rewriting these policies needs a custom
  engine build and security/graphics validation, not generic launch flags.
- `src/core/profile_adapter.cpp`, `profile_io_data_qt.cpp` and
  `api/qwebengineprofile.cpp`: named profiles default to disk HTTP cache;
  off-the-record profiles use memory cache. Automatic cache size is represented
  by zero. Changing paths/type can reset network context. Prism already has one
  shared persistent profile; repeatedly changing its configuration would add work.
- `src/core/web_contents_adapter.cpp`: Active/Frozen/Discarded transitions,
  visibility, safe recommendation limits, form/PDF state and capture protection.
  Frozen keeps renderer/DOM/JS memory; discarding reconstructs/reloads content.
- `src/core/web_contents_delegate_qt.cpp::IsBackForwardCacheSupported` directly
  gates Chromium BFCache on the public Qt setting; `web_engine_settings.cpp`
  sets its default to false. This supplied the implemented experiment.
- Chromium `content/browser/renderer_host/back_forward_cache_impl.cc`: eligibility,
  cache size, TTL, low-memory rejection and pressure eviction. Altering these
  policies would modify Chromium and needs a separately tested engine build.
- Chromium `third_party/blink/renderer/platform/scheduler/main_thread/page_scheduler_impl.cc`:
  background CPU budgets, wakeup throttling and audio exemptions already exist.
  Tightening budgets blindly can break applications; no such modification was kept.
- Qt's `examples/webenginewidgets/simplebrowser/tabwidget.cpp`: shared-profile
  page ownership and deferred deletion match Prism's existing close-tab approach.

GPU-backed WebGL continues to report Intel UHD Graphics 620 through ANGLE D3D11.
This proves that path is available, not that every video is hardware decoded.

## 5. Repositories, issues and attribution

All three requested repositories were studied:

1. [Qt WebEngine](https://github.com/qt/qtwebengine/tree/e081473af432e2b09d17caa3ca3bffd45992a0bd):
   files listed above, `.cmake.conf`, `configure.cmake`, public setting docs,
   `.gitmodules` and Chromium's pinned source.
2. [qutebrowser](https://github.com/qutebrowser/qutebrowser):
   `qutebrowser/browser/webengine/webenginesettings.py`, `webenginetab.py` and
   `webview.py`; profile initialization, cache defaults, lifecycle and shutdown.
   Keeping normal/private profiles separate and not copying arbitrary flags is
   appropriate; importing Python layers into Prism is not.
   [Issue 8420](https://github.com/qutebrowser/qutebrowser/issues/8420) tracks the
   public BFCache API. [Issue 8826](https://github.com/qutebrowser/qutebrowser/issues/8826)
   discusses discard lockups, reinforcing the decision not to add automatic discard.
   [Issue 8909](https://github.com/qutebrowser/qutebrowser/issues/8909) concerns
   Qt 6.11 graphics failures on another platform; it is not evidence that Prism
   has that bug or a reason to disable GPU acceleration here.
3. [kkoehne/qtwebengine-examples](https://github.com/kkoehne/qtwebengine-examples):
   `webenginewidgets/minimal/main.cpp`, `consoleOutput/main.cpp` and
   `photoBooth/main.cpp`. Useful for basic ownership/console diagnostics; the old
   photoBooth example auto-grants permissions and uses deprecated API, so that
   behavior was explicitly not adopted.

The supplied Qt 6.12 documentation URL was not accessible through the web tool.
The installed `C:/Qt/Docs/Qt-6.12.0/qtwebengine/qtwebengine-index.html`, pinned
source documentation and current official Qt WebEngine pages were used instead.

No upstream implementation text was copied into Prism. The external checkout
retains Qt's original licenses. qutebrowser is GPL-3.0-or-later; examples also
have license terms and are not a source of un-attributed copied code. Distribution
must continue to comply with Qt/Chromium licenses and notices:
[Qt WebEngine licensing](https://doc.qt.io/qt-6/qtwebengine-licensing.html).

## 6. Source revision and build feasibility

Experimental workspace: `C:/Users/user/Desktop/Prism-WebEngine-Lab`.
The official release tag `v6.140.0` resolves to Qt WebEngine commit
`e081473af432e2b09d17caa3ca3bffd45992a0bd`; its Chromium submodule is
`e2b413ea3e2dc3c533335e12297abfda95b26cfa`. The installed Qt 6.12 module reports
WebEngine 6.140.0. This is the matching upstream release reference; the exact
vendor-binary build commit cannot be independently proved from version metadata.
No binary/source-equivalence claim or custom-engine compatibility claim is made.

The shallow checkout excludes the enormous Chromium submodule. Observed resources:
15.75 GiB physical RAM, roughly 4.39 GiB free, 8 logical processors and 220.1 GiB
free disk. MSVC v143 19.44.35230, Windows SDK 10.0.26100.0, CMake 3.30.5,
Ninja 1.12.1, Node 24.19.0 and Python 3.14.6 exist. html5lib is missing; bison,
flex and gperf were not on PATH. Qt's engine requires C++20 even though Prism
remains a C++17 consumer. Optional Rust/bindgen tooling also needs configuration
checks for a chosen custom build; this would not mean rewriting Prism in Rust.

The pinned Chromium Windows guide specifies at least 8 GB RAM, recommends more
than 16 GB and requires at least 100 GB disk. Free disk passes that minimum but
must also hold build output/tools; only 4.39 GiB currently available RAM is poor
headroom. A full local engine build is impractical for this low-cost experiment.
No full-engine configure/build, compiler download, dependency installation or
installed-Qt modification was attempted. A proposed 32–64 GiB RAM workstation
with ample SSD space is a practical test host, not a hard upstream requirement.

## 7. Experimental work versus shipped changes

- **Built and tested separately:** the small supported-API `engine_probe` executable
  against the existing Qt libraries. Source and raw off/on JSON are under the lab.
- **Shipped:** the supported BFCache integration, version update and regression test.
- **Not built or modified:** Qt WebEngine/Chromium source binaries. No speculative
  source patch or maintained Chromium fork was created just to appear productive.
- **Investigated, not implemented:** lazy global/profile-service initialization,
  large JS-value marshalling changes, process-count limits, V8 heap limits and
  different Blink background budgets. These need profiling evidence, stronger
  build hardware and upstream engine tests before a targeted source patch.

## 8. Resource cleanup and safety validation

The after-change local run closed nine background tabs, drained deferred deletes
and waited two seconds. The process tree fell from eleven processes at ten tabs
to two processes with one tab. Remaining private memory was 227.7 MiB; allocator,
GPU and engine caches are not guaranteed to return to startup size immediately.
No working-set trimming, fake memory pressure or security reduction was used.

The new regression first failed on V0.2 because a JS value vanished on Back.
After the change it survives document restoration. Maximum Savings disables the
attribute. Navigation and existing sleep/wake tests passed together. A separate
focused source review found no actionable cache-policy issue.

## 9. Website compatibility and H.264

The final isolated-profile site run completed without load timeouts:

| Site | Result | Observed loadFinished time |
| --- | --- | ---: |
| Google Search | Search results title, 31 completed images; no unusual-traffic warning in this run | 2,302 ms |
| YouTube | JavaScript UI and consent/history content; zero completed content thumbnails | 5,800 ms |
| Wikipedia | Article title/content, seven completed images | 1,360 ms |
| Example Domain | HTTPS document/title | 539 ms |
| GitHub | Homepage title/content, 20 completed images | 4,476 ms |

These single-run network timings are not before/after speed comparisons. Neither
YouTube playback nor thumbnail-feed performance is proved fixed. The successful
Google result does not establish that its intermittent warning is fixed: the
new navigation-cache setting cannot explain a first search's server decision.
No Google challenge is bypassed and no automated identity is disguised. The
previous single-search unusual-traffic result is not explained by a confirmed
reload loop or profile duplication. Server-side reputation/session decisions
cannot be diagnosed definitively from Prism alone.

H.264 is still unavailable in the installed-engine probe; VP9 reports probably.
Official Qt documentation requires a build configured with
`-webengine-proprietary-codecs` for the corresponding proprietary codec support:
[Qt audio/video documentation](https://doc.qt.io/qt-6/qtwebengine-features.html#audio-and-video-codecs).
That is an engine build/configuration and distribution-licensing decision, not
a page setting or a random replacement DLL. Relevant codec/patent rights and
Qt/Chromium/FFmpeg license obligations must be addressed before distribution;
an open-source Qt license alone does not promise proprietary codec rights.
No codec binary was downloaded or patched. DRM support is a separate question.

## 10. Remaining limits and V0.3 recommendations

The demonstrated improvement applies to eligible history navigation. First-load
network time, startup, overall RAM and scrolling have no demonstrated broad gain.
Large retained documents can increase memory; native Chromium eviction remains
enabled. Maximum Savings prevents new cached navigation documents but does not
promise to flush old entries instantly. Sites with active features or restrictions
may be ineligible and fall back to normal navigation.

Keep Qt WebEngine. For V0.3, first collect longer multi-run traces of real sessions,
per-process CPU/identity counters, GPU/renderer work and native memory pressure.
Use a stronger isolated workstation only if those traces identify a specific
upstream allocation/startup bottleneck worth rebuilding. Do not maintain a fork
without repeatable benefits and engine regression/security tests. H.264 should
be evaluated as a separately licensed build variant, not a speed tweak.

Existing limitations remain: uncertain protection for arbitrary background jobs
(use Keep Awake), no automatic discard, URL-only closed-tab restore, no restartable
download manager, limited permissions and no tested DRM/device calls. macOS,
Linux, clean-PC deployment and true cold starts remain untested.

## 11. Reproduction and launch

Run `build.cmd`, then `test.cmd network`. Qt's bin directory must be on PATH for
direct benchmark commands. The preserved V0.2 extended benchmark and V0.2.1
benchmark use the same local workload; their versioned JSON files retain outliers
rather than silently deleting them. No Chrome-win claim is made or implied.

The lab's `probe/build.cmd` builds only its small executable, not WebEngine.
See the lab README for exact off/on commands, source provenance and feasibility.

Double-click `launch.cmd` to run the deployed V0.2.1 after deployment validation.
The installed Qt and the existing user profile are retained.

## 12. Final validation

Release build succeeded. Both CTest suites passed: zero failures, 40.02 seconds
total. The new cache-state test was observed failing before implementation, then
passed alongside navigation and sleep/wake coverage. Existing tests cover search,
titles, moved-tab recency, themes, shortcuts, redirects, cookies, HTTP authentication,
permission denial and actual download file contents. Real HTTPS loading passed.

`deploy.cmd` succeeded. Built and deployed Prism.exe SHA-256 matched:
`0EABAB2562C4035723F51F8BE699AC70FD4CFBC91F756BBACC22A5774000BDFD`.
`launch.cmd` was used to open the deployed executable after the user closed V0.2.
No installed Qt library was replaced. Detailed logs: `build/v021-build.txt`,
`build/v021-tests.txt`, `build/v021-targeted.txt`, `build/v021-cache-red.txt`,
`build/v021-websites.json` and `build/v021-deploy.txt`.
