# Prism website deployment

Serve this directory as a static website. Keep the `downloads` directory beside
`index.html` to use the bundled installer. Its size is about 180 MiB, so choose
hosting that accepts files this large. The installer is intentionally excluded
from Git because ordinary GitHub repository uploads do not support this size.

Alternatively, upload the installer as a GitHub Release asset, then replace
`downloadUrl` in `config.js` with its actual direct download URL. Keep the source
repository URL in `githubUrl` separately. Do not invent either URL.

Update version, download URL and size together for each release. Update the
checksum file alongside the installer. Do not replace an installer while keeping
an old checksum. The current bundled installer is unsigned.

Fill in Gmail, Discord, Stripe, PayPal.me and GitHub in `config.js`. Empty links
remain disabled. Preview this website before deployment. Public codec rights and
third-party distribution compliance require resolution before public release.

Upload only the website directory to website hosting. For the browser source
repository, select the project source, documentation, licences and build scripts.
Exclude browser profiles, cookies, test profiles, crash dumps, private credentials,
build outputs and installer binaries. This workspace may inherit a Git repository
from a parent directory, so never stage the entire parent repository.
