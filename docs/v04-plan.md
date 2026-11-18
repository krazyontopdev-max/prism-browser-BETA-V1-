# V0.4 performance and polish

Preserve the profile, WebEngine security/GPU defaults, BFCache admission policy,
new-tab layout and the forty offline wallpapers. No engine replacement.

- [x] Improve the existing benchmark harness first, while production code is
  unchanged. Preserve that binary; measure identical isolated profiles, fixed
  wallpaper, synthetic 1/5/10 tabs and a five-site live workload before/after.
  Include every descendant PID, private commit, working-set sums, stable-PID CPU
  deltas and switch-to-rAF acknowledgement (not claimed as display latency).
- [x] Target confirmed repeated work in BrowserWindow, NewTabStore/Bridge and
  start.js. Only the active new-tab page should receive/render wallpaper updates.
- [x] Keep Balanced freezing and BFCache. In Maximum Savings, first freeze, then
  discard long-idle HTTP(S) pages only when Qt recommends Discarded; retain audio,
  downloads, permissions, form protection and Keep Awake. Discarded pages reload
  when selected. Never discard internal new tabs or the active page.
- [x] Resolve favicons from actual site metadata/cache with bounded first-party
  discovery, including SVG/PNG and icon URLs observed during browsing. No proxy
  services or fabricated site logos. Use a neutral fallback if unavailable.
- [x] Refine tab metrics and compact-window CSS without changing the layout.
- [x] Add focused regressions for resource release/wake/forms, hidden new-tab
  work, icons and compact layouts. Build Release and run all existing suites.
- [x] Repeat the same measurements; test YouTube, TikTok, Claude and DeepSeek
  honestly, including blocked/login/codec limitations. Verify deployment and
  report measured gains separately from noisy results and untested coverage.

Qt's matching source in Prism-WebEngine-Lab confirms recommendedState protects
audible/loading/capture/related-window pages, and after freezing also form input
and PDFs. Freezing retains the document; discarding releases it and reloads on
wake. Arbitrary application state cannot be universally detected. Maximum
Savings must describe this tradeoff; Keep Awake protects work the engine cannot
recognize. The source is inspected only, not modified or rebuilt.

