# Prism and Chrome comparison — 18 November 2026

## Result

This controlled one-page test favored Chrome for loading speed and Prism for
process-tree Private Bytes. It supports neither a general speed ranking nor a
claim that Prism is faster on every PC.

| Measurement | Prism 1.0.0, Chromium 140 | Chrome 157 |
| --- | ---: | ---: |
| Median navigation load event | 308 ms | 97 ms |
| Observed load range | 254–989 ms | 89–113 ms |
| Median animation-frame interval | 16.7 ms | 8.3 ms |
| One-page process-tree Private Bytes | 263.8 MiB | 395.3 MiB |
| One-page process-tree working set | 481.9 MiB | 781.6 MiB |

## Method and limits

`node tests/paired-speed.mjs` launches separate visible browser instances with
private sessions and disposable test data. It alternates five navigation runs
per browser, using identical no-store localhost HTML and a touched 32 MiB
JavaScript allocation. Both report an active visible page and a 1180 × 700 CSS
viewport. Sandbox, GPU and site isolation are enabled with default policies.
Local DevTools ports are used for diagnostics. Only the spawned diagnostic
process trees are closed afterward. Existing user browsers are left open.

Load time is the page Navigation Timing loadEventEnd value. It excludes app
startup and does not measure visual completeness or internet loading. Memory
uses the same `tests/process-metrics.ps1` collector, summing each owned process
tree after five seconds settling. Both browser instances remain running during
the samples. Private Bytes are committed private memory, not physical RAM.
Working sets can contain shared pages. Browser services and engine versions
differ, and the fresh Chrome profile launched more helper processes.

Animation-frame intervals were measured on a static page. They show different
frame scheduling in this setup, not input latency or scrolling performance.
The test used one PC and one simple local fixture. Authenticated sites, extensions,
video, tab switching, cold startup and gaming were not compared. Existing
synthetic tab-savings results remain separate evidence.

Raw output: `build/paired-speed-results.json`. Five successful navigation samples
were recorded per browser, with visibility and viewport validation passing.

## Reported crash

Windows recorded an unhandled C++ exception in the installed 0.7.0 candidate
while the owner opened tabs. The event had no actionable source stack or dump.
The subsequent normal-environment window suite passed 34 cases with three skips.
This comparison completed without a crash, but the reported crash remains
unresolved. The separate sandboxed diagnostic failure had a different exception.

## Confirmed UI fix

The Google screenshot shows a rendered HTTP verification response accompanied
by Prism's generic connection-failure banner. A local HTTP 429 regression test
reproduced that message. Prism now reports the website's HTTP status and asks
the visitor to follow its instructions. Connection and certificate failures
retain their existing handling. The focused test failed before the change and
passed afterward, along with failed-load recovery, stopped navigation and tab
recency tests. The fix does not bypass or solve Google's challenge.

The first full window-suite run after the fix passed 34 cases, failed one
new-tab URL wait in the recency test, and skipped three optional checks. The
focused rerun passed all six cases including setup and cleanup. The final full
suite passed 35 cases, failed none and skipped three optional checks in 95.8
seconds. Evidence is in `build/http-message-final-suite.txt`.

The updated `dist/Prism-Browser-1.0.0-Setup.exe` compiled successfully. Deployment
to the existing installation was attempted. Windows Restart Manager could not
close the running 0.7.0 Prism process, so Setup returned 5 and rolled back.
After Prism closed normally, a second installation completed successfully.
The installed executable matched staging by SHA-256. The installed HTTP 429,
connection recovery and stopped-navigation checks passed (five cases including
setup and cleanup). The updated Prism Browser 1.0.0 was then launched using
the existing `prism-engine-candidate` profile. Browser data was preserved.

## Website update

The website now presents this comparison in a responsive table linked to this
method, alongside the existing privacy, Shields and sleeping-tab sections.
JavaScript syntax and local asset links passed checks. The Chrome preview
connector was unavailable, so visual browser QA remains incomplete. The site
has not been published and public URL placeholders remain unfilled.
