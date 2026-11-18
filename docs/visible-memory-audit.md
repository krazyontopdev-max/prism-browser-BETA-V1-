# Visible Chrome memory audit — 2026-11-15

No production browser code, memory policy, engine configuration or user data was
changed. The Release benchmark and shared process collector were extended for a
matched comparison. The Release build passed. Focused checks for repeated
discard/wake/page deletion, BFCache document retention, hidden-page updates and
custom-wallpaper rotation all passed.

## Comparison result: invalid, no browser ranking

The controlled workload used the same local HTTP URLs and response bytes, fresh
off-the-record sessions, visible windows, 1/5/10 loaded tabs, a matched 1180 × 703
CSS viewport, 250 ms after each load, five seconds settling and three seconds
idle sampling. Both browsers used tests/process-metrics.ps1 to sum Private Bytes
across their own root PID and descendants, with PID/creation-time CPU identities.
Sandboxing, site isolation and hardware acceleration were not changed.

Validation caught multiple Chrome pages reporting document.hidden=false while
Prism had one selected tab. One correction explicitly supplied Chrome's private
browser context and background-tab creation. That run also failed tab-state /
viewport validation. Testing stopped after that correction. The precise reason
for the remaining mismatch was not captured, so it is not attributed to a
specific Chrome feature. No RAM or CPU ranking is supported by this experiment.
The earlier pilot also had Chrome process churn, making its CPU comparison
unreliable. Raw pilot data is marked invalid in build/visible-comparison.json;
build/visible-comparison.log records the failed validation.

QtTest instrumentation in Prism's harness, different native browser UI, and
Chrome 157 versus Qt's Chromium 140 remain comparison limitations even after
matching tab states. This controlled local fixture is not a substitute for a
matched authenticated real-site workload. No before/after RAM improvement is
claimed because no production optimization was justified or implemented.

## Source and existing evidence

- BrowserWindow creates one shared QWebEngineProfile for its tabs. Each view owns
  its page; closing a tab schedules view deletion, and window destruction removes
  pages before the profile. Repeated page-object deletion tests pass. No strong
  QObject ownership cycle or duplicate profile per tab was found.
- QWebChannel and NewTabBridge objects are allocated for every tab, including
  web-only tabs. Those are avoidable native objects, but their memory contribution
  has not been measured. No evidence connects them to hundreds of MiB in renderer
  processes. Adding lifecycle complexity without a measured benefit is unjustified.
- Balanced freezing and BFCache intentionally retain document/JS state. Maximum
  Savings already releases eligible untouched pages without weakening input,
  permission, media, download or active-page protection. Cache clearing or broader
  discard would change the requested performance or sleeping behaviour.
- The wallpaper store retains JSON metadata and local file paths, not forty
  decoded QImages. Hidden pages skip catalog delivery; only a changed selected
  image URL is assigned. Previously visited new-tab renderers can retain their
  decoded image/compositor resources. Cross-renderer sharing or forcing eviction
  has no demonstrated safe app-level fix here.
- The focused retention trace attributed the analogous renderer to TikTok before
  closure. Its private memory was 171 MiB at five seconds, 117 MiB at thirty, and
  the process exited before sixty. Total private commit declined from 802 to
  493 MiB while a live YouTube page and audio/video services remained. This is
  delayed resource recovery, not proof of a persistent leak or exact cache owner.

The strongest existing safe memory reduction remains Maximum Savings for unused
eligible tabs. A new C++ allocation change or cache policy needs measured ownership
and benefit first. No speculative flags, forced process limits or engine rebuild
were used. Private Bytes means committed private memory, not unique physical RAM;
working-set sums can count shared pages multiple times.
