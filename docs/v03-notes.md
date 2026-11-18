# Prism V0.3 — The Glow-Up

Refined the existing Widgets interface and bundled new-tab page: purple surfaces,
subtle static gradients, larger wordmark, cleaner tab spacing, accent-edged active
tabs, softer controls and short CSS entrance/hover transitions. Reduced-motion
preferences disable those transitions. No animation loops or new dependencies.

Appearance now offers five accent presets and a custom color picker. The accent
is validated and stored under appearance/accent. Live updates affect browser
chrome and internal start pages only, without reloading websites or losing drafts.

Profile identity, storage locations, WebEngine settings, navigation cache, sleeping
modes, downloads, permissions, authentication and other browser behavior are retained.
The new regression verifies accent changes preserve a start-page draft, perform
no reload, and persist the chosen color. Build/test logs are build/v03-build.txt
and build/v03-tests.txt. Launch with launch.cmd after deployment.

Validation: Release build succeeded; both existing CTest suites, including the
new accent regression and real HTTPS test, passed in 43.78 seconds with zero
failures. Final resource rebuild log: build/v03-final-build.txt.
