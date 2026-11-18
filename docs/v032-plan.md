# V0.3.2 implementation

Preserve the existing browser and profile. Use the attached mockup only as a
layout reference; its photograph has no supplied redistribution license.

- One native NewTabStore manages preferences, shortcuts, hourly selection,
  catalog updates, image cache and native imports for all tabs in this window.
- A per-page WebChannel bridge is attached only to the exact internal start URL.
  Every method also validates that URL. Remote navigation detaches the channel.
- Internal HTML/CSS/JS renders the sidebar, search, clock, credits, controls and
  settings. No website script injection or per-tab wallpaper-fetch loops.
- A configurable HTTPS catalog supports licensed, hashed WebP downloads on demand.
  Supply 40 explicitly unpublished catalog slots (10 per category); do not invent
  photos, licenses or hosting. Custom photos are copied/transcoded locally.
- Test settings, validation, shared selection, custom imports, page controls and
  remote-page isolation; run all existing Release tests and verify deployment.
