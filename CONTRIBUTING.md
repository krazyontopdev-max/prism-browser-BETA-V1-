# Contributing

Use C++17 and installed Qt 6.12 Widgets/WebEngine with compatible MSVC x64.
Run `build.cmd --incremental`, then `test.cmd`. Qt Test output is in
`build/prism_*-results.txt`. `test.cmd network` additionally checks example.com;
external compatibility diagnostics require `PRISM_COMPATIBILITY_CHECKS=1` and
may fail on third-party challenges. Do not bypass those challenges.

Keep patches focused. Preserve sandboxing, TLS validation, site isolation, GPU,
BFCache, sleeping protections and private/profile isolation. Add meaningful
regressions for data parsing, persistence and native/website boundaries. Never
use a new Chromium flag as a substitute for identifying a bug.

Retain all third-party notices and photo attribution. Do not modify installed
Qt or redistribute codecs without resolving the required rights and build
maintenance. Never claim faster performance without comparable measurements.
