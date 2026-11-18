# Third-party components

Prism application code is MIT licensed. That license does not replace these terms:

- Qt 6.12 / WebEngine 6.140, dynamically linked: LGPL-3.0/GPL/commercial terms
  as applicable to each module. https://www.qt.io/licensing/open-source-lgpl-obligations
  Matching source references: https://github.com/qt/qtbase/tree/v6.12.0 and
  https://github.com/qt/qtwebengine/tree/v6.140.0.
- Chromium and included dependencies retain their own licenses. View the engine's
  `chrome://credits` page for the generated third-party notices. Public release
  needs a verified corresponding-source/relinking compliance package; links alone
  are not a claim that this distribution requirement has been satisfied.
- The current deployment also contains Qt6Quick3DUtils. Its upstream module uses
  GPL-3.0-only/commercial terms, not LGPL-only terms. The Qt Quick 3D source archive
  and GPL-3.0 license text are included in the source/notice material. Its applicable
  distribution requirements remain part of the public-release review.
- The app-local Qt WebP plugin has notices and terms in `licenses/qt-webp`.
- HaGeZi Multi LIGHT: GPL-3.0; Public Suffix List: MPL-2.0. Sources, authors and
  full terms accompany the data in `filters`.
- Forty photographs: individual CC0/BY/BY-SA terms, attribution, original source
  records and transformations are in `wallpapers/CREDITS.html`, `sources.json`
  and `wallpapers/licenses`. Those terms remain with each photograph.
- App-local Microsoft VC143 runtime files, when packaged, are unmodified
  redistributables from the installed Visual Studio Build Tools redistributable
  directory. Microsoft terms apply, not MIT:
  https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files

The stock preview uses the unmodified installed Qt engine. A separately marked
private codec candidate may contain the locally built WebEngine and its supplied
source patch; consult its ENGINE-CANDIDATE.txt and accompanying build records.
Neither package injects codec DLLs or supplies a DRM module. Installed Qt is
unchanged. Public codec rights, corresponding-source/relinking compliance and
clean-machine acceptance remain release gates; a private test is not clearance
for public redistribution.
