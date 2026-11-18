# Prism delivery ledger

Latest continuation: `docs/v08-release-work.md`. The owner installed Inno Setup
7.1.1 and the preview installer now compiles and installs locally. Historical
compiler-blocked entries below describe the earlier checkpoint, not current status.

Plan: `docs/v1-audit-and-roadmap.md`; supplied V0.5–V1.0 master prompt is the specification.
The user approved the design and routine follow-up decisions on 15 November 2026.

- Execution: inline, milestone order; no additional approval prompts for routine work.
- Ruling: work inside the existing Prism directory as requested. The surrounding
  home-directory Git repository is not a safe commit/worktree target. Use
  project-only source checkpoints; never stage the home directory.
- V0.5 implementation checkpoint: domain-based Shields, private windows, local
  history, cookies/permissions management and scheduled full website-data reset.
  Release build and four CTest suites passed (77.43 seconds); separate real HTTPS
  check passed. Real Google/YouTube checks remain to be recorded before final acceptance.
- Review fixes: reject nested junctions and use guarded deletion; byte-bound
  history; batch history deletion; non-persisting private file chooser; owned
  initial windows; reset-marker failure handling; count-validated list updates
  with a wall-clock timeout; bundle filter data in deployment.
- Ruling: the list contains three public-suffix rules. Exclude public-suffix
  entries to avoid blocking unrelated users of shared hosting. Bundled-list,
  boundary, invalid-feed and private-preference tests cover this behaviour.
- Source checkpoint will be stored under `build/checkpoints` before V0.6 work.
- Checkpoint: `build/checkpoints/v05-source.zip` preserves the passing implementation.
- Ruling: user time-boxed Google validation and directed progress to V0.6 unless
  a confirmed critical regression exists. Google reached a security challenge;
  no Shields cause was demonstrated. Treat as an external compatibility limit,
  preserve the failed diagnostic log, and do not repeat or bypass it. YouTube
  rendering/playback remains unverified. No confirmed critical regression remains.
- V0.6 in progress: searchable functional Settings and its prerequisite browser
  data features; preserve the V0.5 checkpoint and existing test assertions.
- V0.6 accepted: Release and four suites passed, 71.44 seconds. Settings screenshot
  reviewed; all fifteen sections function. Search, homepage, private settings,
  sessions (including encoded spaces), bookmarks and automatic completed downloads
  have regressions. `build/checkpoints/v06-source.zip` preserves this stage.
- V0.7 in progress: logical bookmark folders, transactional JSON import/export,
  history search, persistent recently closed URLs, tab context actions/mute and
  download records/management. Profile isolation was introduced in V0.6.
- V0.8 remains a multimedia acceptance gate. Blanket approval does not make
  missing codec binaries, redistribution rights or clean-machine tests available.
- V0.7 verified: the latest incremental Release build succeeded. All four CTest
  suites passed in 148.91 seconds: 75 test functions passed, none failed, three
  explicitly skipped diagnostics. Raw results are in `build/stable-preview-tests.txt`
  and `build/prism_*-results.txt`. A source checkpoint is `build/checkpoints/v07-source.zip`.
- V0.8 actual playback evidence: generated local VP9 and AV1 videos advanced
  0.246390/0.247966 seconds and decoded 5/6 frames; WAV advanced 0.203298 seconds.
  H.264/AAC MP4 and AAC audio returned HTML media error 4, with zero progress.
  See `build/media-results.json`, `tests/media_probe.cpp` and
  `tests/generate-media.cmd`. These muted fixture results do not prove audible
  output, live streaming, DRM, device capture or successful calls.
- Installed Qt explicitly disables proprietary codecs. Legitimate H.264/AAC
  support needs a compatible engine built with the proprietary-codec option and
  the applicable redistribution rights. No replacement engine was obtained,
  compiled or installed. The previous engine feasibility audit remains relevant:
  this 16 GiB/four-core computer is unsuitable for an ASAP Chromium rebuild.
- Experimental desktop source selection was removed after its focused test hung
  during teardown. No production screen-capture enabling or permission expansion
  remains. The diagnostic is skipped by default. Mic/camera permission handling
  remains, but physical calls, screen sharing and authenticated external flows
  are not accepted. Do not enable the experimental diagnostic for routine tests.
- V0.9/V1 packaging work: portable packaging script, app-local Qt configuration,
  signed installed Microsoft runtime redistribution, installer source, MIT source
  license, privacy policy, contributor guide and third-party notices are present.
  Automatic approval review rejected installing the Inno Setup compiler with
  "blocked by policy". No installer was built; no clean-machine installation,
  upgrade, uninstall or public-distribution license acceptance is claimed.
- Security and existing behavior: stock Qt libraries, sandbox, TLS validation,
  isolation, GPU, BFCache and sleeping policy were preserved. No broad memory
  benchmark was rerun or improvement claimed. The sleeping regression now waits
  for the internal page's actual eligibility instead of assuming it loaded first.
- Final acceptance is recorded in `docs/v1-acceptance.md`. Version remains 0.7.0
  preview because the master prompt explicitly makes unresolved multimedia and
  installer acceptance V1.0 blockers. Do not relabel this preview as public V1.0.
- Portable stage verified against the Release executable, installed stock engine,
  forty photographs and credits. `launch.cmd --private` launched it successfully
  with no Qt directory on the launching process's PATH; preview left running.
  The ZIP includes README, acceptance, privacy and dependency notices. This is
  local deployment evidence, not a clean-machine installer test.

## Overnight continuation — 15 November

- Latest evidence and current blockers: `docs/v08-release-work.md`, especially
  "Overnight continuation checkpoint". Earlier compiler-installation and
  source-preparation statements above describe older checkpoints.
- Inno Setup is installed; preview install/upgrade/uninstall and preservation of
  profile data have passed locally. Separate-machine validation is still open.
- Physical camera/mic and local WebRTC probes passed. The owner reports Meet
  camera/mic/two-way audio/video working, with screen sharing failing.
- Isolated matching codec-engine build configured; a missing ATL dependency was
  identified and privately prepared. Actual codec playback and the proposed Qt
  thumbnail/deadlock patch remain unaccepted until built and exercised.
- The stock preview keeps capture off. No installed Qt replacement, security
  reduction, memory-policy change or premature V1.0 relabeling was made.
- Unattended candidate validation launch was rejected by automatic approval
  review ("blocked by policy"); its script self-test is not browser acceptance.
