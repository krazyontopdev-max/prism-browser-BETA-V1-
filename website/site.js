const settings = window.PRISM;
document.querySelectorAll('[data-version]').forEach(el => el.textContent = settings.version);
document.querySelectorAll('[data-size]').forEach(el => el.textContent = settings.installerSize);
document.querySelectorAll('.download').forEach(el => {
  if (settings.downloadUrl) { el.href = settings.downloadUrl; el.removeAttribute('aria-disabled'); el.setAttribute('download', 'Prism-Browser-' + settings.version + '-Setup.exe'); }
  else { el.textContent = 'Download pending'; el.setAttribute('aria-disabled', 'true'); }
});
document.querySelectorAll('[data-url]').forEach(el => {
  const key = el.dataset.url;
  const value = settings[key];
  if (value) { el.href = key === 'email' ? `mailto:${value}` : value; el.removeAttribute('aria-disabled'); }
});
document.querySelectorAll('[data-pending]').forEach(el => { if (settings[el.dataset.pending]) el.hidden = true; });
if (settings.email && settings.discordUrl) document.querySelector('#contact-pending').hidden = true;
if (settings.stripeUrl && settings.paypalUrl) document.querySelector('#support-pending').hidden = true;
if (settings.discordUsername && !settings.discordUrl) {
  const discord = document.querySelector('[data-url="discordUrl"]');
  const text = document.createElement('span');
  text.textContent = `${settings.discordDisplayName || 'Discord'} · @${settings.discordUsername}`;
  discord.replaceWith(text);
}
if (settings.email && (settings.discordUrl || settings.discordUsername)) document.querySelector('#contact-pending').hidden = true;
