'use strict';
(() => {
    const $ = id => document.getElementById(id);
    let bridge = null;
    let pendingSearch = null;
    let current = null;
    let clockTimer = null;
    let editing = -1;
    let wallpaper = '';
    let sidebarKey = '';
    let listsKey = '';
    let clockFormat = null;
    let clock24 = null;
    let clockKey = '';
    const dateFormat = new Intl.DateTimeFormat(undefined, { weekday: 'long', day: 'numeric', month: 'long' });
    const element = (tag, text) => { const node = document.createElement(tag); if (text !== undefined) node.textContent = text; return node; };
    const fallbackIcon = () => {
        const icon = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
        icon.setAttribute('viewBox', '0 0 24 24'); icon.setAttribute('aria-hidden', 'true');
        for (const [tag, attributes] of [['circle', {cx:12, cy:12, r:9}], ['path', {d:'M3 12h18M12 3c5 5 5 13 0 18-5-5-5-13 0-18'}]]) {
            const shape = document.createElementNS('http://www.w3.org/2000/svg', tag);
            for (const [name, value] of Object.entries(attributes)) shape.setAttribute(name, value);
            icon.append(shape);
        }
        return icon;
    };
    const call = (method, ...args) => { if (bridge) bridge[method](...args); };

    function tick() {
        if (document.hidden) { clearTimeout(clockTimer); clockTimer = null; return; }
        const now = new Date();
        const format = current ? Boolean(current.options.clock24) : true;
        if (format !== clock24) {
            clock24 = format;
            clockFormat = new Intl.DateTimeFormat(undefined, { hour: '2-digit', minute: '2-digit', hour12: !format });
        }
        const key = `${Math.floor(now.getTime() / 60000)}:${format}`;
        if (clockKey !== key) {
            clockKey = key;
            const parts = clockFormat.formatToParts(now);
            $('clock').textContent = parts.filter(p => p.type !== 'dayPeriod').map(p => p.value).join('').trim();
            $('clock').dateTime = now.toISOString();
            $('period').textContent = parts.find(p => p.type === 'dayPeriod')?.value || '';
            $('date').textContent = dateFormat.format(now);
        }
        if (clockTimer === null) clockTimer = setTimeout(() => { clockTimer = null; tick(); }, 60000 - now.getTime() % 60000 + 20);
    }
    document.addEventListener('visibilitychange', () => { if (!document.hidden && bridge) bridge.getState(render); tick(); });
    tick();

    function render(state) {
        if (!state.options) return;
        current = state;
        if (document.hidden) { clearTimeout(clockTimer); clockTimer = null; return; }
        const photo = state.wallpaper || {};
        const image = photo.image || '';
        if (image !== wallpaper) {
            wallpaper = image;
            $('wallpaper').style.backgroundImage = image ? `url(${JSON.stringify(image)})` : 'none';
            document.body.classList.toggle('has-wallpaper', Boolean(image));
        }
        $('credit').textContent = image ? `${photo.location || photo.name} · ${photo.photographer}`
            : state.options.mode === 'none' ? 'No wallpaper' : state.status || 'No photo available. Choose a local image in Settings.';
        $('license').hidden = !image || !photo.licenseUrl;
        if (photo.licenseUrl) { $('license').href = photo.licenseUrl; $('license').textContent = photo.license + (photo.changes ? ' · cropped/resized' : ''); }
        $('credit').title = [photo.title, photo.attribution, photo.changes, photo.sourceUrl].filter(Boolean).join('\n');
        if (image && photo.sourceUrl) $('credit').href = photo.sourceUrl;
        else $('credit').removeAttribute('href');
        $('clock24').checked = Boolean(state.options.clock24);
        $('mode').value = state.options.mode;
        for (const box of $('categories').querySelectorAll('input')) box.checked = (state.options.categories || []).includes(box.value);
        if (document.activeElement !== $('catalogUrl')) $('catalogUrl').value = state.options.catalogUrl || '';
        $('status').textContent = state.status || '';
        $('assetsNote').textContent = state.assetsPending ? 'No built-in photographs found. Restore the wallpapers folder beside Prism.exe or import local images.' : `${state.builtinCount || 0} bundled photographs work offline. Rotation changes hourly, shared across all new tabs.`;
        $('import').disabled = Boolean(state.importing);
        $('import').textContent = state.importing ? 'Importing…' : 'Import local photos…';
        const key = JSON.stringify(state.shortcuts);
        if (key !== sidebarKey) {
            sidebarKey = key;
            $('shortcuts').replaceChildren(...state.shortcuts.map(site => {
                const link = element('a'); link.href = site.url; link.title = site.name; link.setAttribute('aria-label', site.name);
                if (site.icon) { const icon = element('img'); icon.src = site.icon; icon.alt = ''; icon.onerror = () => { link.replaceChildren(fallbackIcon()); link.title = `${site.name} — favicon unavailable`; }; link.append(icon); }
                else { link.append(fallbackIcon()); link.title += ' — favicon unavailable'; }
                return link;
            }));
        }
        const listKey = JSON.stringify([state.shortcuts.map(site => [site.name, site.url]), state.custom.map(photo => [photo.id, photo.name]), state.catalog.map(photo => [photo.id, photo.location, photo.photographer]), state.options.mode, state.options.fixedId, state.options.categories]);
        if (listKey !== listsKey) { listsKey = listKey; renderLists(state); }
        tick();
    }
    function renderLists(state) {
        const entries = state.options.mode === 'customFixed' ? state.custom : state.catalog.filter(photo => (state.options.categories || []).includes(photo.category));
        $('fixed').replaceChildren(...entries.map(photo => { const option = element('option', photo.name || `${photo.location} · ${photo.photographer}`); option.value = photo.id; return option; }));
        if (!entries.length) { const option = element('option', 'No photos available'); option.value = ''; $('fixed').append(option); }
        $('fixed').value = entries.some(photo => photo.id === state.options.fixedId) ? state.options.fixedId : entries[0]?.id || '';
        $('fixed').disabled = !['builtinFixed', 'customFixed'].includes(state.options.mode);
        $('shortcutList').replaceChildren(...state.shortcuts.map((site, index) => {
            const row = element('div'); row.className = 'list-row'; row.append(element('span', `${site.name} · ${site.url}`));
            const edit = element('button', 'Edit'); edit.onclick = () => openEditor(index);
            const remove = element('button', 'Remove'); remove.onclick = () => call('removeShortcut', index);
            row.append(edit, remove); return row;
        }));
        $('customList').replaceChildren(...state.custom.map(photo => {
            const row = element('div'); row.className = 'list-row'; row.append(element('span', photo.name));
            const remove = element('button', 'Remove copy'); remove.onclick = () => call('removeCustom', photo.id);
            row.append(remove); return row;
        }));
    }
    function openSettings() { if (!$('options').open) $('options').showModal(); }
    function openEditor(index = -1) {
        editing = index;
        const site = current?.shortcuts[index];
        $('editorTitle').textContent = site ? 'Edit shortcut' : 'Add shortcut';
        $('shortcutName').value = site?.name || '';
        $('shortcutUrl').value = site?.url || '';
        if (!$('shortcutEditor').open) $('shortcutEditor').showModal();
        $('shortcutName').focus();
    }
    for (const category of ['coastlines', 'nature', 'cities', 'mountains']) {
        const label = element('label'); label.className = 'check';
        const checkbox = element('input'); checkbox.type = 'checkbox'; checkbox.value = category;
        checkbox.onchange = () => call('setOptions', { categories: [...$('categories').querySelectorAll('input:checked')].map(box => box.value) });
        label.append(checkbox, document.createTextNode(category)); $('categories').append(label);
    }
    $('home').onclick = () => $('search').focus();
    $('search').form.onsubmit = event => {
        event.preventDefault();
        if (bridge) call('search', $('search').value);
        else pendingSearch = $('search').value;
    };
    $('browserSettings').onclick = () => call('openBrowserSettings');
    $('settings').onclick = openSettings;
    $('wallpaperControl').onclick = openSettings;
    $('closeSettings').onclick = () => $('options').close();
    $('add').onclick = () => openEditor();
    $('addInSettings').onclick = () => openEditor();
    $('closeEditor').onclick = () => $('shortcutEditor').close();
    $('shortcutForm').onsubmit = event => { event.preventDefault(); call('addShortcut', $('shortcutName').value, $('shortcutUrl').value, editing); $('shortcutEditor').close(); };
    $('clock24').onchange = () => call('setOptions', { clock24: $('clock24').checked });
    $('mode').onchange = () => call('setOptions', { mode: $('mode').value });
    $('fixed').onchange = () => call('setOptions', { fixedId: $('fixed').value });
    $('import').onclick = () => call('chooseImages');
    $('saveCatalog').onclick = () => call('setOptions', { catalogUrl: $('catalogUrl').value });
    $('refreshCatalog').onclick = () => call('refreshCatalog');
    $('shuffle').onclick = () => call('shuffle');
    $('fullscreen').onclick = () => call('toggleFullscreen');
    if (typeof qt !== 'undefined' && typeof QWebChannel !== 'undefined') {
        new QWebChannel(qt.webChannelTransport, channel => {
            bridge = channel.objects.prism;
            bridge.stateChanged.connect(render);
            bridge.getState(render);
            if (pendingSearch !== null) { call('search', pendingSearch); pendingSearch = null; }
        });
    } else $('status').textContent = 'Native new-tab controls are unavailable.';
})();
