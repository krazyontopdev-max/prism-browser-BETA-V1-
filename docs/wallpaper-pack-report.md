# Offline wallpaper pack

- 40 genuine photographs: 10 coastlines, 10 nature, 10 cities, 10 mountains.
- Each is 2560 × 1440 WebP. Images total 58,595,002 bytes (55.9 MiB).
  Sizes range from 774,736 to 1,995,584 bytes; 38 are between 1 and 2 MB.
  Two night scenes remain smaller at maximum WebP quality, without padding.
- Individual Commons file-page licenses and revision records were checked and
  saved in wallpapers/sources.json. The collection uses CC0, CC BY 2.0/3.0/4.0,
  and CC BY-SA 3.0/4.0. Photographer credits, source links, modifications and full
  license terms ship in wallpapers/CREDITS.html and wallpapers/licenses.
- Sources were visually reviewed; source photo dates, available camera metadata
  and original processing notes are retained. No AI imagery or generative edits.
- Built-in images load directly from the packaged wallpapers folder. There are
  no wallpaper-server requests or first-run downloads for these photographs.
  Existing optional download catalogs, custom images, categories, shared hourly
  rotation, fixed modes and shuffle remain supported. Built-ins are not stored
  in or removed by the user-download cache.
- Release build passed. All three test suites passed (75.99 seconds), including
  the public HTTPS example.com check. New tests decode and hash-check every
  wallpaper, verify dimensions and size, exercise ten-image category rotation,
  test actual new-tab rendering/imports and cover custom/mixed modes.
- Build and deployment each contain exactly 40 matching image hashes, with
  credits, provenance and all six applicable license texts. Existing browser
  data and Qt WebEngine settings were preserved. No performance gain is claimed.

Launch with launch.cmd. Keep dist/Prism/wallpapers beside Prism.exe when packaging
or moving the app; retain its credits and licenses when redistributing images.
