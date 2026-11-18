# Private codec-engine build

This is the approved experimental route, not an accepted public V1.0 build.
Stock Qt remains installed and unchanged. A successful compile does not settle
codec redistribution rights or prove playback, calling or screen sharing.

## Matching sources and tools

- Installed Qt SDK: 6.12.0, MSVC x64; compiler toolset v143 14.44.
- Qt WebEngine release: 6.140.0, upstream commit
  `e081473af432e2b09d17caa3ca3bffd45992a0bd`.
- Chromium integration submodule:
  `e2b413ea3e2dc3c533335e12297abfda95b26cfa`.
- Required tools include CMake, Ninja, Python with html5lib, Node.js, Perl,
  bison, flex, gperf and matching MSVC ATL headers/libraries. WebEngine internally
  requires C++20; the Prism application remains C++17.
- `prepare-source-bundle.ps1` verifies official source archives against upstream
  SHA-256 files. WebEngine has its own versioned archive, separate from Qt 6.12.

This machine's isolated workspace is `C:\Users\user\Desktop\Prism-WebEngine-Lab`;
its build directory is `C:\PrismCodecBuild`. Build-only ATL payloads are extracted
privately and checked against the installed Visual Studio catalog. Their manifest
records the official Microsoft URLs and hashes. They are not shipped in Prism.

## Configuration and isolation

The lab's `env-codecs.cmd` records the exact local configuration. It uses
`-webengine-proprietary-codecs`, Release, and a private
`CMAKE_INSTALL_PREFIX` ending in `Prism-WebEngine-Lab\codec-install`.
QtPdf's separate module and the unused WebEngineQuick wrapper are disabled;
the Widgets browser and its existing security settings are preserved.
For a direct Ninja resume, set `NODEJS_EXECUTABLE` to the Node executable
recorded in CMake's `Nodejs_EXECUTABLE` entry. Qt's usual CMake wrapper supplies
that variable, and DevTools generation requires it even when Node is on PATH.

`run-codec-build.ps1 -Phase Build` keeps Windows awake during compilation and
stops only its worker if free disk falls below 20 GiB. Set a positive
`-MinAvailableRamMiB` to opt into a RAM and commit reserve. The successful
resume used eight Ninja jobs and a zero reserve. Lower parallelism if the machine
starts paging heavily. The worker restores normal sleep behavior on exit.
`-Phase Install` verifies the
private prefix before copying anything. Never install this build into `C:\Qt`.
The user's ThrottleStop configuration and existing power plan are unchanged.

The sole source patch, `0001-skip-unused-picker-thumbnails.patch`, skips thumbnails
that Qt's names-only source model does not consume. It aims to fix a proven
Windows picker-teardown deadlock. It does not disable actual capture, consent,
GPU, sandboxing, TLS validation or isolation. Runtime verification is still required.

## Candidate acceptance

Configure Prism in a separate app build using the private WebEngineCore,
WebEngineWidgets and WebEngineCoreTools CMake packages. The lab's
`candidate-app.cmd` records those paths. Screen sharing is enabled only for this
candidate with `PRISM_ENABLE_SCREEN_SHARING=ON`; stock production defaults off.

Deploy matching DLLs, QtWebEngineProcess, resources and locales together. Do not
mix a new engine DLL with stock snapshots or resource packs. The accepted preview
and its normal profile are not the test target.

Copy the supplied synthetic `media` fixtures beside `prism_media_probe.exe`.
Set `PRISM_REQUIRE_PROPRIETARY_CODECS=1` and run `prism_media_probe playback`.
Every required format must play, with advancing clocks and decoded video frames.
Then run local WebRTC loopback and the four existing regression suites.

Explicit capture testing uses `PRISM_CAPTURE_TESTS=1` and
`prism_window_tests screenSharingRequiresSourceConsent sharesOnlySelectedTestWindow`.
The actual capture fixture selects only its own test window. Physical-device
testing is separately opt-in with `PRISM_DEVICE_PROBE=1`; authenticated calls and
screen sharing must be recorded honestly as manual acceptance.

The preparation-only worker builds and stages the private candidate, then stops
before tests, device access or installer execution. Its `prepared-not-validated`
status is not a pass. See `v08-release-work.md` and `v1-acceptance.md` for evidence
and remaining licensing, media and clean-machine release gates.
