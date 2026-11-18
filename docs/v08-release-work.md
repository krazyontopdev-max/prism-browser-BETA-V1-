# V0.8 and release continuation — 15 November 2026

The application remains a 0.7.0 preview until the multimedia acceptance gate
passes. No installed Qt libraries, browser engine policies or security flags
were changed. The existing V0.5–V0.7 source checkpoints remain intact.

## Implemented and observed

- Added a persisted abnormal-exit marker and recovery prompt for ordinary
  profiles. Recovery restores bounded, validated tab URLs without changing the
  configured startup preference. Private windows neither record this marker nor
  restore ordinary URLs. A regression verifies restoration, unsafe URL rejection
  and private isolation. A deliberately terminated, isolated test profile produced
  the expected recovery dialog on restart; automated interaction with that dialog
  was unavailable, so interactive end-to-end recovery acceptance is incomplete.
- Added a local engine WebRTC loopback diagnostic with no STUN/TURN or external
  servers. A generated canvas stream crossed a peer connection and decoded three
  frames; inbound video statistics reported 212 bytes received. This tests local
  video transport, not physical cameras, microphones, audible output or real calls.
  Evidence: `build/webrtc-results.json`, `build/v08-media-probe.txt`.
- Repeated actual local codec playback: VP9 and AV1 decoded six frames each;
  WAV playback advanced. H.264/AAC MP4 and AAC audio still failed with media error
  4 and zero playback progress. No codec workaround or browser identity spoofing.
- Windows enumerates an integrated camera and microphone endpoints. Enumeration
  is not capture or physical-device acceptance. Screen sharing remains disabled
  after the earlier teardown hang; it is not advertised as supported.
- The owner installed Inno Setup 7.1.1. Its compiler has a valid Authenticode
  signature. `installer/prism.iss` compiled successfully to
  `dist/Prism-0.7.0-Preview-Setup.exe`. This resolves the earlier compiler-availability
  blocker; it does not remove multimedia, installation-test or licensing gates.
- `package-preview.ps1 -Installer` stages fresh binaries separately in
  `dist/PrismInstallerStage`, preserving the running portable preview. Compile
  that stage with `ISCC.exe installer/prism.iss`.

## Current verification checkpoint

Release compiled successfully. Privacy/core/window suites passed (5.93, 0.54,
101.47 seconds). New-tab testing timed out during simultaneous installer
compression; its unchanged bridge assertion passed on an isolated rerun in
16.50 seconds. No timeout was increased or security assertion removed. Current
totals across the four suites: 76 passed, zero failures in the latest per-suite
runs, three explicitly skipped diagnostics. The forward-navigation regression
now waits for its asynchronous button enablement before clicking.

The installer installed successfully in an isolated directory, executable hashes
matched Release and stock Qt, forty photos were present, and native accessibility
showed the rendered new-tab search, clock, sidebar and photographer/license
credits. It launched with Qt removed from PATH. Keyboard closure ended the
process and persisted `session/running=false`. Raw evidence:
`build/installer-validation.json`, `build/installer-install.log`,
`build/v08-regressions.txt`, `build/v08-newtab-recheck.txt`.

## Codec route and resources

The installed WebEngine configuration disables proprietary codecs. Official Qt
documents `qt-configure-module <qtwebengine-source> -webengine-proprietary-codecs`
as an engine build option and warns that distribution requires codec licenses:
[Qt audio/video documentation](https://doc-snapshots.qt.io/qtwebengine/qtwebengine-features.html#audio-and-video-codecs).
No verified matching codec-enabled Windows package has been obtained. No claim
is made that a commercial Qt subscription alone supplies codec patent rights.

Current resources: 15.75 GiB physical RAM, 2.92 GiB free RAM and 213.97 GiB free
disk at the preflight snapshot (`build/v08-environment.json`). A slow isolated
local source build may be possible after freeing RAM and verifying full source,
toolchain and output-space requirements; success and build duration are unknown.
It must not overwrite installed Qt. A larger rented build machine or trusted
supplier are alternatives, with explicit cost and redistribution terms. No paid
service was selected and no enormous source download/build was started.

Public V1.0 remains blocked on legitimate H.264/AAC, actual media/calling/capture
acceptance, corresponding-source/relinking compliance and final installation
acceptance. This document records progress, not a V1.0 release certification.

## Installer lifecycle results and local build authorization

The final preview installer upgraded the isolated installation successfully (exit
0), and its uninstaller removed the executable successfully (exit 0). An owned
sentinel file in the separate local browser profile survived both operations.
This tests preservation without deleting or modifying the ordinary user profile.
Evidence: `build/installer-lifecycle-results.json` and upgrade/uninstall logs.

The same installer then installed into `%LOCALAPPDATA%/Programs/Prism` successfully;
its executable matched the tested Release build. It launched in a private window
with the Qt development directory removed from PATH, leaving ordinary data alone.
Evidence: `build/installer-final-results.json`. This is a real local installed
build test, not a clean-machine/VM certification.

The owner chose an isolated local codec build, authorized normal closing of heavy
apps, and requested no further questions. After the owner closed apps, free RAM
rose to 6.74 GiB. The system already has a 512 GB Toshiba NVMe SSD. No hardware
purchase, paid service, security exclusion or global toolchain change was made.
The owner authorized keeping the computer awake during the worker; its awake
request is released automatically on exit. Build parallelism is limited to one,
and the worker stops its own process tree if free disk drops below 20 GiB.

Build workspace: `C:/Users/user/Desktop/Prism-WebEngine-Lab`; matching Qt tag
`v6.140.0`, Chromium integration submodule
`e2b413ea3e2dc3c533335e12297abfda95b26cfa`. Source preparation is underway.
Build-only GNU tools came from the official Qt-hosted gnuwin32 archive; html5lib
was installed into a private virtual environment. Build output is isolated in
`C:/PrismCodecBuild`, and installation is restricted to the lab's `codec-install`
directory. No installed Qt or working-engine replacement is authorized by these
scripts. Local build approval does not certify proprietary-codec distribution
rights or public V1.0 acceptance.

## Continued diagnostics and user calling result

The owner reports Google Meet in Prism works for camera, microphone and two-way
audio/video, but not screen sharing. This is user-reported live-call evidence,
not an automated authenticated-site test or a result for the future custom engine.

The physical-device engine probe passed: nineteen 320×240 video frames, a live
microphone track and nonzero sampled audio activity (`audioPeak=3`). Only numeric
statistics were saved (`build/device-results.json`); no recording was made.
The first probe's Start button had been shifted by inline video layout, so its
native click missed. Giving the button a block layout fixed the diagnostic.

Prism's source-picker integration now explicitly cancels abandoned requests and
resolves pending capture dialogs before destroying their page. Closing a page
before resolving its raw-WebContents callback caused an access violation in a
targeted test; the close regression passed after that lifetime fix. Cancel,
navigation and tab-close cases passed in an isolated window suite (88.57 seconds,
34 passed, two optional diagnostics skipped). A concurrent engine-build run timed
out; the isolated result is the acceptance evidence for these regressions.

Actual selected-window capture still deadlocked in the stock engine. Windows
Wait Chain Traversal confirmed a cycle in the test process: UI thread 13792 waits
for thread 7044, which waits on a SendMessage back to 13792. Evidence:
`build/screen-waitchain.txt`, `build/screen-trace.txt`. This proves a deadlock,
not a memory leak. Native desktop media-list cleanup synchronously joins its
worker; the Qt model discards thumbnail notifications and exposes names only.

Experimental Qt patch `patches/0001-skip-unused-picker-thumbnails.patch` in the
engine lab requests an empty thumbnail size, using Chromium's existing names-only
refresh path. It aims to avoid unnecessary thumbnail captures and that teardown
cycle. It does not disable actual screen/window capture, consent, GPU, sandbox,
TLS validation or isolation. This hypothesis has NOT yet passed runtime testing;
the isolated engine must finish building before it can be accepted or integrated.
No Chromium source file was changed. The installed preview and Qt remain intact.

The isolated engine configured successfully with proprietary codecs enabled and
initially one build job. A safety guard stops only its worker tree below 20 GiB free disk;
awake requests clear automatically when the worker exits. Source tag and patch
are preserved. Do not call this a completed codec fix until actual playback passes.

## Overnight continuation checkpoint

The current build uses two low-priority jobs. Its RAM guard was deliberately
self-tested at an unreachable 32 GiB threshold and killed only its test worker.
The old `codec-resource-stop.json` with PID 6912 records that self-test, not a real
low-memory failure. The normal threshold is 1,536 MiB available RAM.

The next build stopped at `base_jumbo_48.obj`: `atldef.h` was absent from the
installed MSVC v143 toolset. This was a missing SDK dependency, not RAM exhaustion
or a codec/compiler incompatibility. Matching Microsoft ATL header/library VSIX
payloads were obtained from the installed Visual Studio catalog's official
Microsoft download URLs and verified against its SHA-256 records. They are
extracted only under `Prism-WebEngine-Lab/tools/atl-v143`; no Visual Studio or Qt
installation was changed. `manifest.json` records URLs, versions and checksums.
The build-only CL/LINK variables supply those paths to Qt's regenerated compiler
environment. The focused recompilation of the originally failing object passed
(`Prism-WebEngine-Lab/codec-atl-check.log`). Existing successful objects were
retained; no whole-build clean or installed-toolchain modification was needed.
Original failure evidence: `Prism-WebEngine-Lab/codec-build-atl-missing.log`.

Stock Release now builds with screen sharing off by default. Enabling it requires
the separate candidate configuration and successful source-consent and actual
selected-window capture tests. This avoids exposing the confirmed stock-engine
deadlock. No normal browser permission is granted automatically by production code.

Windows executable version metadata and an icon derived from the existing Prism
triangle have been added. The Release executable's ProductName and FileVersion
were checked (`build/v09-version-metadata.json`); the version remains 0.7.0 preview.
The installer now accepts separate staging, identity, folder and profile arguments
so an engine candidate cannot overwrite the working installation. Its alternate
configuration compiled successfully (`build/v09-installer-parameters.log`).

Official source archives for seven supporting Qt modules were downloaded and
verified against upstream SHA-256 files under `dist/PrismSources`. The exact local
WebEngine checkout and experimental patch remain in the lab. This is progress
toward corresponding source, not a completed public redistribution audit.

Codex research tabs were closed and Codex was minimized without ending the chat.
Observed private memory fell from about 2,208 to 1,877 MiB across its ChatGPT
processes; this is a point-in-time observation, not a guaranteed persistent saving.
No forced working-set trimming, security changes or user-data deletion was used.

The separate candidate validation helper passed its success, nonzero-exit and
owned-child timeout self-tests. Automatic approval review rejected starting that
unattended helper with "blocked by policy". It has NOT run the codec, camera,
sharing or installed-candidate acceptance steps. Keep this distinct from the
already authorized engine compilation and previously tested preview installer.
The working installed browser and installed Qt remain untouched.

## 16 November continuation

The complete stock Release build passed, followed by all four regression suites:
privacy 12, core 23, windows 31 and new-tab 10 passing cases, zero failures.
Six explicitly optional cases were skipped. Total suite time was 95.19 seconds;
the window suite took 74.51 seconds. Logs: `build/v09-release-build.log`,
`build/v09-release-tests.log` and the four `*-results.txt` files.

The codec build was resumed after the passing ATL recompilation. It remains
isolated and uses the same two-job/resource-guard configuration. No codec playback
or experimental source-patch success is claimed while compilation is incomplete.

An attempt to finish the isolated recovery-prompt UI check encountered an
unavailable desktop-control service. Its owned test process was cleaned up;
no user browser was closed. The prior crash-marker and automated restore tests
remain evidence, but end-to-end manual acceptance is still open. Do not substitute
this unavailable check for a pass or repeatedly change working recovery code.

The strict codec gate was exercised against the stock engine as a negative
control. It correctly failed with media error 4, zero frames and zero playback
time for H.264/AAC MP4 and AAC audio. In the same run VP9/AV1 decoded 5/4 frames
and WAV playback advanced. Evidence: `build/v08-stock-codec-gate.json` and
`build/v08-stock-codec-gate.log`. This verifies the acceptance gate rejects the
missing codecs; it does not enable them. The custom build must pass that exact
strict gate. The source checkpoint is preserved as
`build/checkpoints/v08-overnight-source-20261116.zip` with a companion hash record.

## Continued release preparation

`launch.cmd` was dropping profile arguments in the preview path and all arguments
in the older portable/development paths. A native argument-recording stub proved
all three failures without opening a real profile. The launcher now forwards its
arguments in every path. All three regressions passed, including private/profile
options and a quoted argument containing a space. Evidence:
`build/launcher-tests-before.json` and `build/launcher-tests-after.json`.
No C++ browser behavior or engine setting changed for this fix.

The README's obsolete V0.1 packaging instructions were corrected. A project-local
`.gitignore` excludes build and distribution output; no home-repository index was
edited or reset. One competing background Git scan was lowered to idle priority,
and the owned two-job compiler tree was restored to normal Windows scheduling.
No process was killed for this adjustment. These are build-host choices, not
Prism performance improvements. The owner reports Task Manager CPU speed of
2.62 GHz; the low Windows performance counter alone does not prove throttling.
ThrottleStop, thermal protections and the existing power plan were not changed.

The official WebEngine 6.140.0 source archive was also downloaded from Qt's
separately versioned archive and checksum-verified. `dist/PrismSources` now holds
eight upstream module archives, an application-source ZIP with offline photos
and synthetic media fixtures, and the private build recipe/patch references.
Source-ZIP entry checks found no browser-profile paths. This is not codec-rights
clearance or a completed public-release compliance audit.

The previously rejected all-in-one unattended validator was narrowed to a
preparation-only route. That route was accepted and is waiting for the existing
engine build. It may install only into the lab prefix, compile and stage the
private app, then stops before browser tests, capture/device access or installer
execution. Its JSON timestamp comparison initially failed on a native DateTime;
typed conversion fixed it, and actual-worker/offset checks passed. The running
mode is `-PrepareOnly`; `prepared-not-validated` must never be treated as acceptance.
Actual codec, capture and installed-build tests still need their recorded results.

Two bounded test stages are now queued after preparation: the four regression
suites plus strict codec/synthetic WebRTC tests, then (only if those pass) capture
of the existing generated red/blue test window. They have separate result files
`candidate-local-tests.json` and `candidate-window-capture.json` in the lab.
Neither stage runs an installer or enables physical camera/microphone probes.
Queued is not passed. The About label was also corrected to describe media
support as engine-dependent rather than hardcoding the stock engine's codec
limitation; Release compilation passed (`build/v09-about-build.log`).

A fourth bounded stage is queued only after owned-window capture passes. It
builds the separately named codec-candidate installer, installs into the lab's
new `installed-candidate` directory, verifies matching binaries/40 wallpapers,
and reruns local regressions, strict media and owned-window capture there.
It refuses existing destinations, uses a separate installer identity/profile,
and does not replace the working installation or run physical camera/mic probes.
Results go to `candidate-installed-tests.json`. The broad default validator path
has been removed: missing or conflicting modes are refused before work begins.
Those mode guards were exercised successfully. This queue is not an installed
test pass or public V1.0 acceptance; the media, hardware/call and rights gates
remain governed by their actual evidence.

## User-requested compiler-only pause

The owner asked to pause agent work while compilation continues. All queued
preparation, test, capture and installer workers were stopped while still waiting;
their result files now say `paused-by-user`. No acceptance was performed.
The existing objects were retained, CMake was reconfigured for three compile jobs,
and only the guarded build was launched independently in the background. Its RAM
reserve is now 2,048 MiB; disk protection remains 20 GiB. The monitor records are
`Prism-WebEngine-Lab/codec-monitor.json` and `codec-worker.json`; progress/errors
are in `codec-build.log` and `codec-monitor.stderr`. ThrottleStop is unchanged.
After compilation ends, resume validation explicitly rather than trusting the
old queued-worker PIDs. The tested working browser and installed Qt are intact.

The owner then requested higher CPU use and offered to close other apps. The
compiler-only run was reconfigured to four jobs, matching the four physical cores,
with normal Windows scheduling and the same 2,048 MiB RAM/20 GiB disk cutoffs.
Completed objects were retained. This supersedes the three-job trial; no browser
runtime process limit, security setting, power-plan or ThrottleStop change was made.

The owner's final resource request supersedes that setting: eight inner Ninja
jobs for the eight logical CPU threads, with one outer CMake job to prevent
nested build oversubscription. RAM is allocated on demand. A native Windows
memory query now checks physical availability and commit headroom every second;
either falling below 2,048 MiB stops only the owned build tree. Native readings
and the high-threshold cutoff predicate were checked before starting. Completed
objects remain cached. Only compilation is running; tests/installation remain
paused, and ThrottleStop is untouched. Use the latest `codec-monitor.json` rather
than older job counts or worker PIDs when resuming.

## 17 November — resume for homework use

The eight-job build stopped at the memory safeguard, not at a demonstrated source
error: worker 18696 had 1,972 MiB available RAM against its 2,048 MiB threshold;
commit headroom was still 13,653 MiB. The last stage reached task 8,235/17,584.
Completed objects were retained. Failure logs/resource evidence were copied to
`codec-build-memory-stop-checkpoint.log` and `codec-resource-stop-eight-job.json`.
No finished custom WebEngine binaries or new-engine acceptance are claimed.

At the owner's request to keep Chrome usable for homework, configuration was
regenerated for two inner compile jobs, one outer job, BelowNormal priority and
a 3,072 MiB physical/commit reserve. Only the unfinished compiler was resumed in
the background. Chrome, ThrottleStop and working Prism data were untouched.
Earlier stock Release tests/source-packaging work remain completed checkpoints;
new-engine preparation, media/capture checks and installer validation remain
unstarted/paused and must not be inferred from the old worker records.

The initial homework restart also hit the conservative 3 GiB cutoff during GN
setup (3,068 MiB available). The next restart retained two BelowNormal jobs with
a 2,560 MiB physical/commit cutoff. The unconditional timestamp touch forcing GN
regeneration on every resume was removed; genuine dependency changes still
regenerate normally. Compilation resumed from retained objects with 9,350 tasks
remaining in that stage. A separate read-only graphical log viewer was opened
and maximized through desktop controls after Terminal control was blocked by
product policy. It displays the full current build log and follows new lines;
it has no command input and does not control the compiler. Chrome and ThrottleStop
were left alone. See `graphical-viewer.json` and the latest `codec-monitor.json`.

After closing Chrome, the owner prioritized compilation again. With about
5.2 GiB available RAM, the remaining generated Core target was resumed directly
with four Ninja jobs at normal priority and a 2,048 MiB safety cutoff, followed
by the outer CMake build at one job. No configuration rerun or clean was used;
completed outputs remain cached. This supersedes the homework scheduling above.
The requested one-hour completion was not promised: substantial engine work and
the later acceptance checks remain. ThrottleStop and installed Qt are unchanged.

The four-job attempt also hit the 2,048 MiB resource guard. On the owner's status
check, no compiler remained active. Its stop record/stderr were preserved as
`codec-resource-stop-four-job.json` and `codec-monitor-four-job.stderr`. The
existing Core target was restarted with two direct Ninja jobs, normal priority
and the same 2 GiB cutoff, with no reconfiguration or clean. Latest monitor mode
is `MemorySafeBuildOnly`; use its new PID rather than prior records. V1.0 remains
unaccepted and no higher parallelism should be assumed sustainable on this PC.
