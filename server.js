// Try Pico Doom multiplayer server. Zero dependencies (Node 18+). Serves the web game and relays WebSocket rooms.
// Rooms: 2-4 players. Modes: co-op (everyone must reach the portal) and PvP (first to N kills).
// Wagers are NON-CUSTODIAL: the server only does the arithmetic and tells losers who to pay; it never touches money.
const http = require('http'), crypto = require('crypto'), fs = require('fs'), path = require('path');
const PORT = process.env.PORT || 8080, MAXP = 4, GOALS = [5, 10, 15, 20, 25, 50, 100];
const rooms = new Map(); let nextId = 1;
const clean = (s, n, re) => String(s || '').replace(re || /[^\w $.-]/g, '').slice(0, n);
const money = v => Math.max(0, Math.min(1000, Math.round(+v || 0)));
const frame = str => { const b = Buffer.from(str), l = b.length; let h;
  if (l < 126) h = Buffer.from([0x81, l]); else if (l < 65536) { h = Buffer.alloc(4); h[0] = 0x81; h[1] = 126; h.writeUInt16BE(l, 2); }
  else { h = Buffer.alloc(10); h[0] = 0x81; h[1] = 127; h.writeBigUInt64BE(BigInt(l), 2); } return Buffer.concat([h, b]); };
const send = (q, o) => { if (!q.s.destroyed) q.s.write(frame(JSON.stringify(o))); };
const bc = (r, o, except) => { for (const q of r.p.values()) if (q !== except) send(q, o); };
function parse(q, chunk, onmsg) {
  q.buf = Buffer.concat([q.buf, chunk]);
  for (;;) { const b = q.buf; if (b.length < 2) return; let len = b[1] & 127, off = 2;
    if (len === 126) { if (b.length < 4) return; len = b.readUInt16BE(2); off = 4; } else if (len === 127) { q.s.destroy(); return; }
    const masked = b[1] & 128; if (masked) off += 4; if (len > 8192) { q.s.destroy(); return; } if (b.length < off + len) return;
    let d = b.subarray(off, off + len); if (masked) { const k = b.subarray(off - 4, off); d = Buffer.from(d.map((v, i) => v ^ k[i & 3])); }
    q.buf = b.subarray(off + len); const op = b[0] & 15;
    if (op === 8) { q.s.end(); return; } if (op === 9) { q.s.write(Buffer.from([0x8a, 0])); continue; } if (op === 1) onmsg(d.toString()); } }
const lobby = (r, q) => ({ t: 'lobby', room: r.code, you: q.id, host: r.host, mode: r.mode, pvp: r.mode === 'pvp' ? 1 : 0, goal: r.goal, hal: r.hal, entry: r.entry, n: r.p.size,
  players: [...r.p.values()].map(x => ({ id: x.id, slot: x.slot, name: x.name, tag: x.tag, extra: x.extra })) });
const board = r => [...r.p.values()].map(x => ({ id: x.id, slot: x.slot, name: x.name, k: x.k }));
function scores(r) { for (const q of r.p.values()) send(q, { t: 'sc', list: board(r), mine: q.k, goal: r.goal }); }
function win(r, w) { r.over = true; r.started = false;
  const stake = q => r.entry + q.extra, pot = [...r.p.values()].reduce((a, q) => a + stake(q), 0);
  for (const q of r.p.values()) send(q, { t: 'win', w: w.id, name: w.name, payto: w.tag, pot, owe: q === w ? 0 : stake(q), board: board(r), you: q.id }); }
function leave(q) { const r = q.room; if (!r) return; r.p.delete(q.id); bc(r, { t: 'left', id: q.id, slot: q.slot });
  if (!r.p.size) { rooms.delete(r.code); return; } if (r.host === q.id) r.host = r.p.values().next().value.id;
  if (!r.started) [...r.p.values()].forEach(x => send(x, lobby(r, x))); }
function handle(q, m) {
  if (m.t === 'join') {
    if (q.room) return; let r;
    if (m.host) { let code; do code = Math.random().toString(36).slice(2, 6).toUpperCase(); while (rooms.has(code));
      r = { code, mode: m.mode === 'pvp' ? 'pvp' : 'coop', goal: GOALS.includes(+m.goal) ? +m.goal : 10, hal: m.hal ? 1 : 0, entry: money(m.entry), p: new Map(), host: q.id, started: false }; rooms.set(code, r); }
    else { r = rooms.get(clean(m.room, 4, /[^A-Za-z0-9]/g).toUpperCase());
      if (!r) return send(q, { t: 'err', m: 'ROOM NOT FOUND' }); if (r.started) return send(q, { t: 'err', m: 'GAME ALREADY STARTED' }); if (r.p.size >= MAXP) return send(q, { t: 'err', m: 'ROOM FULL (4 MAX)' }); }
    const used = new Set([...r.p.values()].map(x => x.slot)); q.slot = [0, 1, 2, 3].find(i => !used.has(i));
    Object.assign(q, { room: r, name: clean(m.name, 10) || 'PLAYER', tag: clean(m.tag, 20, /[^\w$]/g).replace('$', ''), extra: money(m.extra), k: 0, exited: false });
    r.p.set(q.id, q); for (const x of r.p.values()) send(x, lobby(r, x)); return; }
  const r = q.room; if (!r) return;
  switch (m.t) {
    case 'start': if (r.host !== q.id || r.p.size < 2 && !r.solo) { if (r.host === q.id) r.solo = 1; else return; }
      r.started = true; r.over = false; for (const x of r.p.values()) { x.k = 0; x.exited = false; }
      bc(r, { t: 'start', mode: r.mode, pvp: r.mode === 'pvp' ? 1 : 0, goal: r.goal, hal: r.hal }); scores(r); break;
    case 's': bc(r, { t: 's', id: q.id, slot: q.slot, name: q.name, x: m.x, y: m.y, a: m.a, hp: m.hp }, q); break;
    case 'm': bc(r, { t: 'm', i: m.i, d: m.d }, q); break;
    case 'hit': if (r.mode === 'pvp') { const to = r.p.get(m.to); if (to && to !== q) send(to, { t: 'hit', from: q.id, d: Math.min(200, Math.max(0, +m.d || 0)) }); } break;
    case 'killed': case 'k': { if (r.mode !== 'pvp' || !r.started || r.over) break;
      const by = m.t === 'k' ? q : r.p.get(m.by); if (!by || (m.t === 'killed' && by === q)) break;
      by.k++; scores(r); if (by.k >= r.goal) win(r, by); break; }
    case 'exit': if (r.mode !== 'coop' || !r.started) break; q.exited = true;
      { const n = [...r.p.values()].filter(x => x.exited).length; for (const x of r.p.values()) send(x, { t: 'exitn', n, total: r.p.size });
        if (n === r.p.size) { for (const x of r.p.values()) x.exited = false; for (const x of r.p.values()) send(x, { t: 'done' }); } } break;
  } }
const mime = { '.html': 'text/html; charset=utf-8', '.js': 'text/javascript', '.png': 'image/png' };
const server = http.createServer((req, res) => {
  if (req.url === '/health') { res.end('ok'); return; }
  const f = path.join(__dirname, 'public', req.url === '/' ? 'index.html' : path.normalize(req.url).replace(/^(\.\.[\/\\])+/, ''));
  fs.readFile(f, (e, d) => { if (e) { res.statusCode = 404; res.end('not found'); } else { res.setHeader('Content-Type', mime[path.extname(f)] || 'application/octet-stream'); res.end(d); } }); });
server.on('upgrade', (req, s) => {
  const key = req.headers['sec-websocket-key']; if (!key) return s.destroy();
  s.write('HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ' +
    crypto.createHash('sha1').update(key + '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64') + '\r\n\r\n');
  const q = { id: nextId++, s, buf: Buffer.alloc(0), room: null, n: 0, t0: Date.now() }; s.setNoDelay(true);
  s.on('data', c => parse(q, c, txt => { if (Date.now() - q.t0 > 1000) { q.t0 = Date.now(); q.n = 0; } if (++q.n > 80) return; let m; try { m = JSON.parse(txt); } catch (e) { return; } if (m && typeof m === 'object') handle(q, m); }));
  s.on('close', () => leave(q)); s.on('error', () => s.destroy()); });
server.listen(PORT, () => console.log('Try Pico Doom server on :' + PORT));
