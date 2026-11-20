const state = { clips: [] };

function uniqSorted(values) {
  return [...new Set(values.filter(Boolean))].sort((a,b) => a.localeCompare(b));
}

function optionList(select, values, label) {
  select.innerHTML = `<option value="">${label}</option>` + values.map(v => `<option value="${escapeHtml(v)}">${escapeHtml(v)}</option>`).join('');
}

function escapeHtml(value) {
  return String(value ?? '').replace(/[&<>'"]/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;',"'":'&#39;','"':'&quot;'}[c]));
}

function render() {
  const search = document.querySelector('#search').value.toLowerCase();
  const event = document.querySelector('#event').value;
  const map = document.querySelector('#map').value;
  const filtered = state.clips.filter(c => {
    const haystack = JSON.stringify(c).toLowerCase();
    return (!search || haystack.includes(search)) && (!event || c.event === event) && (!map || c.map === map);
  });

  const root = document.querySelector('#clips');
  if (!filtered.length) {
    root.innerHTML = '<div class="empty">No clips found in clips.json.</div>';
  } else {
    root.innerHTML = filtered.slice().reverse().map(c => `
      <article class="card">
        <h3>${escapeHtml(c.event || 'clip')}</h3>
        <div class="meta">
          ${escapeHtml(c.map || 'Map unknown')} Â· ${escapeHtml(c.mode || 'Mode unknown')}<br>
          ${escapeHtml(c.weapon || 'Weapon unknown')} Â· ${escapeHtml(c.created_at || '')}<br>
          ${escapeHtml(c.file || '')}
        </div>
        ${(c.tags || []).map(t => `<span class="tag">${escapeHtml(t)}</span>`).join('')}
      </article>
    `).join('');
  }

  document.querySelector('#stats').textContent = `${state.clips.length} clips`;
}

async function load() {
  try {
    const response = await fetch('clips.json', { cache: 'no-store' });
    if (!response.ok) throw new Error('clips.json not found');
    const data = await response.json();
    state.clips = Array.isArray(data.clips) ? data.clips : [];
    optionList(document.querySelector('#event'), uniqSorted(state.clips.map(c => c.event)), 'All events');
    optionList(document.querySelector('#map'), uniqSorted(state.clips.map(c => c.map)), 'All maps');
  } catch (error) {
    document.querySelector('#clips').innerHTML = `<div class="empty">Open the dashboard with a local HTTP server and copy/export your clips.json into the dashboard folder for this prototype.</div>`;
  }
  render();
}

document.querySelectorAll('#search,#event,#map').forEach(el => el.addEventListener('input', render));
load();
