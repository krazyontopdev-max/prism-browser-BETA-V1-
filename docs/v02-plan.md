# Prism V0.2

Upgrade the existing browser. Preserve its profile, default user agent, TLS checks,
GPU defaults and V0.1 features. No engine replacement or Chromium build.

1. Record a V0.1 baseline with the same Release benchmark harness used for V0.2:
   fresh process, synthetic local pages, process-tree memory/CPU, tab handlers,
   GPU renderer, codec probes and optional public-site smoke tests.
2. Remove repeated icon painting and duplicate selection updates. Keep existing
   pages and a shared persistent profile; do not inject scripts into websites.
3. Add Balanced (5 min), Maximum Savings (1 min), Off. Use Qt's Frozen state,
   never discard pages automatically. Respect recommendedState, audio, downloads,
   permissions, DevTools and per-tab Keep awake. One coarse timer, stopped in Off.
4. Add asynchronous save/permission prompts, download feedback, fullscreen and
   HTTP authentication. Preserve HTTPS rejection and normal popup navigation.
5. Build/test each stage, run identical benchmarks, verify deployment and report
   measured improvements, regressions and limits. No claim against Chrome without
   a controlled comparison. Do not access the user's live Chrome profile.
