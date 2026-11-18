# Prism V0.1

Build the user's requested C++17 Qt Widgets browser in this folder, using only
the installed Qt 6.12.0 WebEngine and MSVC v143 x64 (19.44).

## Design

A QMainWindow owns a movable QTabWidget of QWebEngineViews sharing a named
QWebEngineProfile. A compact toolbar contains navigation, an address/search
field, and a theme menu. The bundled new-tab HTML runs in the same engine, so
Home and links participate in normal browser history. No third-party assets,
frameworks, Chromium downloads, or advanced browser systems.

Address input resolves explicit HTTP/HTTPS URLs, local files, hostnames and
Google queries. Invalid or unsupported explicit URLs show an inline error.
Qt's error pages remain enabled; a small retry banner provides recovery.
Tab identity, rather than tab position, tracks recency after tab moves.
Recently closed URLs can be reopened during the current run.

## Work and verification

- [x] Check Qt and compiler; configure CMake with v143 x64 and Ninja.
- [x] Add URL interpretation tests; observe failures, then implement resolution.
- [x] Build the browser core; test local HTTP navigation, tabs, close/reopen,
      last-used selection, moved tabs, titles, invalid input and errors with Qt Test.
- [x] Confirm a real HTTPS website renders before styling.
- [x] Polish the tabs, controls and bundled start page; add System/Light/Dark
      themes and keyboard shortcuts. Theme changes must not reload websites.
- [x] Build Release, run all tests, deploy the installed Qt runtime locally,
      launch the deployed executable and inspect the desktop preview.
- [x] Write beginner launch/build instructions and concrete test results.

Review edge cases: URL schemes and query escaping; closed background tabs;
tab reordering; cancelled loads versus failures; final-tab closure; certificates
(never bypass validation); failed pages recovering via retry/back; theme changes
while a page is loading. Cross-platform source is desirable, but only Windows
is built and tested for this release.
