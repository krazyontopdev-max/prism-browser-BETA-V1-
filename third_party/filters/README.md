# Prism Shields filter data

Downloaded unchanged on 15 November 2026:

- `light.txt`: HaGeZi Multi LIGHT domains, version 2026.1115.0834.15,
  https://raw.githubusercontent.com/hagezi/dns-blocklists/main/wildcard/light-onlydomains.txt
  by HaGeZi and the contributors identified upstream. GPL-3.0; full terms in
  `GPL-3.0.txt`. Sources and issue reports: https://github.com/hagezi/dns-blocklists
- `public_suffix_list.dat`: the Public Suffix List, by its contributors,
  https://publicsuffix.org/list/public_suffix_list.dat
  MPL-2.0; full terms in `MPL-2.0.txt`. Source and contributor history:
  https://github.com/publicsuffix/list

These are independent data files, not a claim of equivalence to a full content
blocker. Prism uses whole-domain matching. It does not implement cosmetic rules
or guarantee removal of same-host advertising/video ads. Keep these data files,
attribution and license texts together when distributing the application.

The bundled snapshots work offline. Ordinary windows check HaGeZi's HTTPS
upstream at most daily and retain a validated cached snapshot on failure.
Private windows do not update or persist changes. No Prism-operated server is used.
