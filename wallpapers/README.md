# Prism wallpaper pack

The browser includes **40 real photographic wallpapers**, ten each in coastlines,
nature, cities and mountains. Every shipped image is a 2560 × 1440 WebP, under
2,000,000 bytes. No generated imagery or generative editing was used. Camera,
photographer, original photo date, file-page revision and license evidence are
retained in `sources.json`; the photographs were selected from Wikimedia Commons
featured photographs and visually reviewed. Original panorama/HDR processing
information is retained in the source records.

Open [CREDITS.html](CREDITS.html) for each original title, photographer, requested
attribution, source link, reviewed revision, license and modifications. Full legal
terms are included in `licenses`. Images are CC0 or Creative Commons BY/BY-SA
2.0, 3.0 or 4.0, as stated individually. Redistribution and adaptation are allowed
under those terms. Share-alike derivatives remain under the same license as the
original; these image licenses do not change the browser code's license. Keep
credits, notices and license terms with redistributed copies. No endorsement by
the photographers is implied.

## Offline use and packaging

`catalog.json`, `images`, `licenses`, `CREDITS.html` and `sources.json` are copied
to `build/wallpapers` by CMake and `dist/Prism/wallpapers` by `deploy.cmd`. Keep
this folder beside Prism.exe when packaging the browser. Wallpaper selection
uses local files directly: no server, first-run download, user-cache seeding,
internet connection or copy into the user profile is required. Only the selected
photo is decoded by the page. Built-in files are separate from the bounded
download cache and cannot be pruned or removed by custom-photo controls.

The default rotation follows the UTC hour and saved shuffle offset, shared by
all new tabs. Fixed selection, category filtering, local custom photos, mixed
rotation and no wallpaper retain their existing behavior and settings. User
browser data and original custom images are unchanged.

## Rebuilding the photo assets

Normal C++ builds use the checked-in photographs and never fetch assets.
`prepare.py` is an optional maintainer tool, requiring Python, Pillow and requests,
to reproduce the pack from the reviewed `sources.json`. It downloads official
Wikimedia thumbnails only during asset preparation, keeps originals in the
ignored build directory, normalizes sRGB, center-crops/resizes and encodes WebP.
It verifies individual license names/URLs, resolution, size and unique hashes.
Simple images are not padded to reach an arbitrary size target.

## Optional additional downloadable catalog

The existing HTTPS catalog system still works independently of browser updates.
No endpoint is configured by default; built-in photos never depend on it.
`catalog.template.json` is only a publishing example, not the shipped catalog.
For additional assets, verify redistribution rights and provide URL, SHA-256,
category, location, photographer, original source and license URL. The remote
validator currently accepts CC0-1.0, CC-BY-4.0 and CC-BY-SA-4.0. Host catalog and
photographs on the same HTTPS origin, with at most 2 MiB per WebP and 3840 × 2160.

Enter that URL in New-tab Settings and check for updates. Only the selected
remote photo is downloaded, with checksum/size validation and a 128 MiB cache.
Remote catalogs are merged with the offline collection; removing/changing a
remote endpoint never removes the packaged collection. Local custom images
remain private and are never uploaded. Cached site shortcut favicons are
independent of the wallpaper pack.
