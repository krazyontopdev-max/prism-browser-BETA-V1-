# Prism V0.3.2

Implemented the reference layout: black browser chrome, slim shortcut rail,
temporary black wordmark, Google search, live 12/24-hour clock/date, photo credit,
wallpaper settings, shuffle and fullscreen. No gradients or animations.

The shared native wallpaper service supports fixed/rotating built-in and local
images, mixed rotation, categories and no wallpaper. Rotation follows the UTC
hour across new tabs. Downloads are on demand with HTTPS, same-origin redirects,
SHA-256 verification, size limits and a bounded local cache. Local photo imports
run on a worker and never upload or modify the originals. Hidden pages stop
their clock and defer wallpaper rendering.

The exact Qt 6.12.0 official WebP image plugin was added app-locally because the
installed image-format plugins lacked it. Installed Qt files and WebEngine
settings were not changed. Plugin provenance/licenses are in third_party/qt-webp.

Release build succeeded. Both existing test suites passed, including the public
HTTPS example.com check. New-tab tests passed for actual HTML/bridge controls,
native image picker/import, WebP display, categories, editable shortcuts, clock
settings, responsive width, fullscreen requests, persistence, shared selection,
catalog validation and remote-page bridge rejection. Deployment matches the
built executable; launch.cmd launched the deployed app for visual inspection.

Wallpaper follow-up: forty licensed photographs are now packaged locally, ten
per category, and work offline without hosting. See wallpaper-pack-report.md
and wallpapers/CREDITS.html for verification and credits. The optional hosted
catalog has not been tested against a production service. Favicons come from each site's own
/favicon.ico and are cached; unavailable icons use an explicit letter fallback.

Existing browser profile/data, downloads, permissions, authentication, tab
sleeping and Keep Awake remain in place. No performance gain is claimed.
