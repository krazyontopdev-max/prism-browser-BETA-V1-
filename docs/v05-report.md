# Prism V0.5 implementation checkpoint

Added native domain-based Shields using HaGeZi Multi LIGHT, Standard/Strict/Off,
per-site exceptions, offline rule/PSL data, attributed licenses and daily/manual
HTTPS updates. Updates validate size, syntax, provenance headers and declared
entry count, have inactivity and wall-clock deadlines, and retain the previous
snapshot on failure. Public suffixes are excluded. This is not cosmetic or
same-host video-ad filtering and does not claim uBlock/Brave equivalence.

Added isolated off-the-record private windows (Ctrl+Shift+N), temporary native
new-tab data and non-persisting private file choosers. Private history/preferences
do not overwrite ordinary data. Saved downloads remain on disk. Outstanding
photo imports finish before temporary storage is removed.

Privacy and site data provides cookie review/selection deletion, permission reset,
local visit history with selection deletion/clearing, cookie/cache clearing and
an explicitly confirmed next-launch full default-profile website-data reset.
Reset validates containment and refuses links/junctions before guarded deletion.
Bookmarks, wallpapers and downloaded files are outside that reset. No existing
user data was cleared by development or tests.

Release built successfully. All four CTest suites passed in 77.43 seconds.
Focused tests cover domain boundaries, update rejection, private cookie isolation,
private picker persistence, history limits, and a nested-junction external sentinel.
The targeted bugs were observed failing before their fixes. Existing navigation,
downloads, permissions, BFCache, sleeping, image and new-tab regressions passed.
An initial network-enabled suite failed without captured test output; the local
suite passed, and a separate fresh `realHttpsWebsite` test rendered Example Domain
successfully. No causal explanation for the earlier network failure is claimed.

The time-boxed Google check reached an unusual-traffic/security challenge and
reported `solveSimpleChallenge is not defined`; a Shields cause was not established.
Per the user's direction, this is recorded as an external compatibility limitation,
with no challenge bypass or security change. YouTube began loading but the diagnostic
did not obtain body text at its sampling point; playback/complete rendering was not
verified. Raw results remain in `build/v05-live-sites.txt`. This opt-in external
diagnostic is separate from the deterministic suites and the example.com HTTPS check.
Final deployment verification remains pending.
H.264 remains unavailable in the unchanged installed engine; actual video/calling
acceptance belongs to the unresolved V0.8 gate. No speed/RAM improvement is claimed.

Evidence: `build/v05-final-build.log`, `build/v05-final-suite.txt`,
`build/v05-https-diagnostic.txt`, `build/v05-private-tests.txt`,
`build/v05-privacy-red.txt`, `build/v05-review-red.txt`.
