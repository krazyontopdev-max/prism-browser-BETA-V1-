# Windows verification — 14 November 2026

## Environment

- Windows 11 x64.
- Qt 6.12.0, `C:\Qt\6.12.0\msvc2022_64`.
- Prebuilt Qt WebEngine extension 6.140.0 for Qt 6.12.0.
- MSVC v143 x64, compiler 19.44.35230, toolset 14.44.35207.
- CMake 3.30.5 and Ninja from the installed Qt tools.

The first configure failed because Qt WebChannel was missing. Qt's installed
Maintenance Tool added the matching prebuilt WebChannel and Positioning modules
(including the installer's associated examples/documentation). No compiler or
Chromium source was downloaded. Missing optional Vulkan headers produced a
configure notice; the Release build and rendered websites worked without them.

## Results

- Release build: passed, no C++ compiler warnings reported.
- Address tests: 22 passed, zero failures (includes setup and cleanup).
- Browser tests: 14 passed, zero failures/skips (includes setup and cleanup).
- The original missing-address behavior and tab-close behavior were observed
  failing before implementation.
- Regression tests caught IPv6 being mistaken for a file path, hostnames with
  ports being mistaken for schemes, and a focused address field retaining a
  draft after Home. All three fixes passed the final suite.
- Real `https://example.com` rendered its title and document text in WebEngine.
- Deployed Prism was also opened through the desktop and used to load that HTTPS
  website. The start page and dark interface were visually inspected.
- A separate source review found no further confirmed correctness issues.

Coverage includes titles; Back/Forward/Reload/Home; close/reopen; last-used tab
selection after moving tabs; closing a background/final tab; keyboard shortcuts;
invalid input retaining the current page; failed/stopped loads; theme changes
without reloading websites; start-page Google form encoding; and target-blank links.

Full logs from the final direct runs are `build\address-final.txt` and
`build\window-final.txt`. The first complete CTest run also passed both suites.
Build/deployment logs are `build\build-log.txt` and `build\deploy-log.txt`.

## Limits

Only this Windows machine was tested. macOS, Linux, a clean Windows machine,
and installer distribution were not tested. Reopened tabs restore the URL only.
There are no performance benchmarks or measured speed claims.
