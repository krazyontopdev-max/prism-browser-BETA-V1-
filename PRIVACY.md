# Prism privacy

Prism stores browser profiles, history, bookmarks, settings, cookies and cached
content locally. Local profile IDs have separate storage. There is no Prism
telemetry, analytics, remote diagnostics or crash-upload service.

Websites still receive ordinary browsing requests, including the IP address and
browser identity. Search terms go to the chosen search provider. Favicons may be
requested from shortcut websites and their declared icon hosts. Ordinary windows
check the HaGeZi HTTPS list upstream at most daily; updates can also be requested
manually. Optional wallpaper catalogs use the URL the user configures. The forty
bundled photographs and local custom images work without a wallpaper server.
Custom image files are never uploaded by Prism.

Standard Shields blocks listed third-party domains. Strict blocks more listed
subresources and can break sites. Exceptions and Off are available. Domain
blocking cannot prevent all tracking or remove every ad; there is no fingerprinting
protection feature and no anonymity guarantee.

Private windows use off-the-record engine profiles and temporary native new-tab
storage. Private browsing history and preference changes are not saved into the
ordinary profile. Explicit downloads and exported bookmarks remain wherever the
user saves them. Websites, network operators, Windows and external applications
are outside that isolation. Closing a private window waits for its outstanding
local wallpaper import before deleting temporary native storage.

Cookie/cache clearing is distinct from full site-data deletion. Full website
storage deletion is explicitly confirmed and scheduled for the next ordinary
launch after every other Prism process closes. It signs users out. Native
bookmarks, wallpaper files and ordinary downloaded files are kept. Session
restoration keeps tab URLs, not unsaved forms or complete back/forward history.
