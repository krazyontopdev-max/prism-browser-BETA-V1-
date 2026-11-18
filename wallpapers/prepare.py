"""Rebuild the reviewed, licensed wallpaper pack; requires Pillow and requests."""
import concurrent.futures
import hashlib
import html
import json
import re
import sys
import time
from collections import Counter
from io import BytesIO
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT.parent / 'build' / 'wallpaper-tools'))
import requests
from PIL import Image, ImageCms, ImageOps

HEADERS = {'User-Agent': 'PrismWallpaperAttribution/0.3.2 (desktop browser asset preparation)'}
LICENSES = {
    'CC0': ('CC0-1.0', 'https://creativecommons.org/publicdomain/zero/1.0/'),
    'CC BY-SA 4.0': ('CC-BY-SA-4.0', 'https://creativecommons.org/licenses/by-sa/4.0/'),
    'CC BY-SA 3.0': ('CC-BY-SA-3.0', 'https://creativecommons.org/licenses/by-sa/3.0/'),
    'CC BY 4.0': ('CC-BY-4.0', 'https://creativecommons.org/licenses/by/4.0/'),
    'CC BY 3.0': ('CC-BY-3.0', 'https://creativecommons.org/licenses/by/3.0/'),
    'CC BY 2.0': ('CC-BY-2.0', 'https://creativecommons.org/licenses/by/2.0/'),
}
CHANGES = 'Center-cropped to 16:9, resized to 2560 x 1440, converted to sRGB WebP. No generative edits.'

def plain(value):
    return ' '.join(html.unescape(re.sub(r'<[^>]+>', ' ', value)).split())

def download(url, destination):
    if destination.exists():
        return destination.read_bytes()
    for attempt in range(4):
        with requests.get(url, headers=HEADERS, timeout=(15, 75), stream=True) as response:
            if response.status_code in (429, 503):
                delay = min(60, max(5, int(response.headers.get('Retry-After', 10 * (attempt + 1)))))
                time.sleep(delay)
                continue
            response.raise_for_status()
            data = bytearray()
            for chunk in response.iter_content(65536):
                data.extend(chunk)
                if len(data) > 30 * 1024 * 1024:
                    raise ValueError('Source thumbnail exceeds 30 MiB')
        destination.write_bytes(data)
        return bytes(data)
    raise RuntimeError('Server requested repeated backoff: ' + url)

def prepare(source):
    stamp = hashlib.sha256(json.dumps(source, sort_keys=True, ensure_ascii=False).encode('utf8')).hexdigest()
    cache = DOWNLOADS / (source['id'] + '.prepared.json')
    if cache.exists():
        saved = json.loads(cache.read_text('utf8'))
        if saved['source'] == stamp:
            photo = saved['photo']
            path = ROOT / 'images' / (photo['sha256'] + '.webp')
            if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == photo['sha256']:
                print(source['id'], 'reusing verified WebP', flush=True)
                return photo
    evidence = source['evidence']
    info = evidence['imageinfo'][0]
    meta = info['extmetadata']
    license_id, license_url = LICENSES[meta['LicenseShortName']['value']]
    source_license = meta['LicenseUrl']['value'].replace('http:', 'https:').removesuffix('/deed.en').rstrip('/')
    assert source_license == license_url.rstrip('/'), 'License identifier and URL disagree'
    assert info['mime'] == 'image/jpeg'
    assert info['width'] >= 2560 and info['height'] >= 1440
    url = urlsplit(info['thumburl'])
    assert url.scheme == 'https' and url.hostname in ('thumb.wikimedia.org', 'upload.wikimedia.org')
    url = urlunsplit((url.scheme, url.netloc, url.path, '', ''))
    data = download(url, DOWNLOADS / (source['id'] + '.jpg'))
    image = Image.open(BytesIO(data))
    assert image.width >= 2560 and image.height >= 1440
    profile = image.info.get('icc_profile')
    image = ImageOps.exif_transpose(image).convert('RGB')
    if profile:
        image = ImageCms.profileToProfile(image, ImageCms.ImageCmsProfile(BytesIO(profile)), ImageCms.createProfile('sRGB'), outputMode='RGB')
    image = ImageOps.fit(image, (2560, 1440), method=Image.Resampling.LANCZOS)
    selected = None
    # Keep detail rather than padding simple images to an arbitrary file size.
    for quality in (95, 97, 99, 100, 93, 90, 87, 85, 82, 80):
        if quality > 95 and selected is None:
            continue
        output = BytesIO()
        image.save(output, 'WEBP', quality=quality, method=6)
        encoded = output.getvalue()
        if len(encoded) <= 2_000_000:
            if selected is None or (len(selected[0]) < 1_000_000 and len(encoded) > len(selected[0])):
                selected = encoded, quality
            if len(encoded) >= 1_000_000:
                break
    assert selected is not None
    encoded, quality = selected
    sha = hashlib.sha256(encoded).hexdigest()
    (ROOT / 'images' / (sha + '.webp')).write_bytes(encoded)
    artist = plain(meta['Artist']['value'])
    attribution = plain(meta.get('Attribution', {}).get('value', ''))
    credit = attribution or artist
    if source['id'] == 'prism-nature-04':
        credit = '© Tomas Castelazo, www.tomascastelazo.com / Wikimedia Commons'
    result = {key: source[key] for key in ('id', 'category', 'location')}
    result.update({
        'title': evidence['title'][5:], 'photographer': credit, 'artist': artist,
        'attribution': attribution or artist,
        'sourceUrl': info['descriptionurl'], 'originalUrl': info['url'],
        'license': license_id, 'licenseUrl': license_url,
        'sha256': sha, 'width': 2560, 'height': 1440, 'bytes': len(encoded),
        'changes': CHANGES, 'quality': quality,
        'sourceRevision': evidence['revisions'][0]['revid'],
        'sourcePhotoDate': plain(meta.get('DateTimeOriginal', {}).get('value', '')),
        'downloadUrl': url, 'downloadSha256': hashlib.sha256(data).hexdigest(),
    })
    creator_urls = []
    for href in re.findall(r'href="([^"]+)"', meta['Artist']['value']):
        href = html.unescape(href)
        if href.startswith('//'): href = 'https:' + href
        if urlsplit(href).scheme in ('http', 'https') and href not in creator_urls: creator_urls.append(href)
    result['creatorUrls'] = creator_urls
    cache.write_text(json.dumps({'source':stamp, 'photo':result}, ensure_ascii=False, indent=2), 'utf8')
    print(result['id'], len(encoded), 'bytes, quality', quality, flush=True)
    time.sleep(0.5)
    return result

if __name__ == '__main__':
    sources = json.loads((ROOT / 'sources.json').read_text('utf8'))['photos']
    assert Counter(p['category'] for p in sources) == {'coastlines':10, 'nature':10, 'cities':10, 'mountains':10}
    DOWNLOADS = ROOT.parent / 'build' / 'wallpaper-downloads'
    DOWNLOADS.mkdir(exist_ok=True)
    (ROOT / 'images').mkdir(exist_ok=True)
    (ROOT / 'licenses').mkdir(exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futures = {pool.submit(prepare, source):source['id'] for source in sources}
        results = {}
        errors = []
        for future in concurrent.futures.as_completed(futures):
            try:
                results[futures[future]] = future.result()
            except Exception as error:
                errors.append((futures[future], str(error)))
                print('FAILED', futures[future], str(error), flush=True)
        if errors:
            raise RuntimeError(str(errors))
        photos = [results[source['id']] for source in sources]
    assert len({p['sha256'] for p in photos}) == 40
    (ROOT / 'catalog.json').write_text(json.dumps({'version':1,'photos':photos},ensure_ascii=False,indent=2),'utf8')
    for license_id, license_url in {p['license']:p['licenseUrl'] for p in photos}.items():
        download(license_url + 'legalcode.en', ROOT / 'licenses' / (license_id + '.html'))
    credits = ['<!doctype html><meta charset="utf-8"><title>Prism wallpaper credits</title>',
        '<style>body{font:16px/1.6 system-ui;max-width:1000px;margin:40px auto;padding:0 24px}article{border-top:1px solid #ccc;padding:20px 0}h2{font-size:20px}a{overflow-wrap:anywhere}</style>',
        '<h1>Prism wallpaper credits</h1><p>40 real photographs, ten per category. Each photo remains under its stated Creative Commons license. These images may be redistributed and adapted under those terms; no additional restrictions apply to these photographs. No photographer endorses Prism.</p>',
        '<p>Edits by Prism: '+html.escape(CHANGES)+' Share-alike derivatives are offered under the same license as their original photograph. The application code is separate from these image licenses.</p>']
    for photo in photos:
        esc = lambda key: html.escape(str(photo[key]), quote=True)
        creators = ' · '.join(f'<a href="{html.escape(url, quote=True)}">Photographer website/profile</a>' for url in photo['creatorUrls'])
        credits.append(f'<article id="{esc("id")}"><h2>{esc("location")}</h2><p><b>Title:</b> {esc("title")}<br><b>Photographer:</b> {esc("photographer")}<br><b>Author metadata:</b> {esc("artist")}<br><b>Attribution supplied:</b> {esc("attribution")}<br><b>Photo date:</b> {esc("sourcePhotoDate")}<br><b>Changes:</b> {esc("changes")}</p><p><a href="{esc("sourceUrl")}">Original source and photographer links</a> · <a href="https://commons.wikimedia.org/w/index.php?oldid={esc("sourceRevision")}">Reviewed file-page revision</a><br><a href="licenses/{esc("license")}.html">{esc("license")} — offline legal terms</a> · <a href="{esc("licenseUrl")}">Canonical license</a></p><p>Bundled file: images/{esc("sha256")}.webp · {esc("bytes")} bytes</p></article>')
        credits.append('<p>' + creators + '</p>')
    (ROOT / 'CREDITS.html').write_text('\n'.join(credits),'utf8')
    print('TOTAL',sum(p['bytes'] for p in photos),'bytes',flush=True)
