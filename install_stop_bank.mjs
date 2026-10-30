export function packEditorBytes(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i += 7) {
    const group = bytes.subarray(i, i + 7);
    out.push(group.reduce((mask, b, j) => mask | ((b >> 7) << j), 0));
    for (const b of group) out.push(b & 127);
  }
  return out;
}

export async function editorRequest(link, cmd, args = [], accept = () => true, attempts = 3) {
  for (let attempt = 0; attempt < attempts; attempt++) {
    if (!link.send([240, 125, 70, 76, cmd, ...args, 247])) throw Error('Keyboard disconnected');
    const until = Date.now() + (cmd === 13 || cmd === 82 ? 10000 : 1000);
    while (Date.now() < until) {
      const p = await link.read(Math.min(250, until - Date.now()));
      if (!p) { if (link.lost) throw Error('Keyboard disconnected'); continue; }
      if (p[0] !== 240 || p[1] !== 125 || p[2] !== 70 || p[3] !== 76 || p[4] !== cmd || p.at(-1) !== 247) continue;
      const reply = p.slice(5, -1);
      if (accept(reply)) return reply;
    }
  }
  throw Error(`No reply to stop bank command ${cmd}`);
}

export async function stopBankStatus(link, manifest, attempts = 3) {
  const p = await editorRequest(link, 82, [], () => true, attempts);
  if (p.length !== 36 || p[0] !== 1 || p[35] !== 0) throw Error('Unsupported stop bank protocol');
  const id = String.fromCharCode(...p.slice(19, 35));
  if (id !== manifest.build_id) throw Error('The installed firmware does not match this build');
  for (let slot = 0; slot < 2; slot++) {
    const start = 3 + slot * 8;
    const crc = p.slice(start, start + 5).reduce((v, b, j) => v + b * 2 ** (7 * j), 0);
    const length = p[start + 5] + p[start + 6] * 128 + p[start + 7] * 16384;
    if (crc !== manifest.banks[slot].crc32 || length !== manifest.banks[slot].data_size) throw Error('Stop bank does not match firmware');
  }
  return { ready: p[1] === 1, valid: p[2] };
}

export async function installStopBanks(link, manifest, images, report = () => {}) {
  let status = await stopBankStatus(link, manifest);
  for (let slot = 0; slot < 2; slot++) {
    if (status.valid & (1 << slot)) { report(slot, 100); continue; }
    const bytes = images[slot];
    if (bytes.length !== 0x14000) throw Error('Invalid stop bank size');
    const begin = await editorRequest(link, 11, [slot], p => p.length === 2 && p[0] === slot);
    if (begin[1]) throw Error(`Cannot prepare stop bank ${slot + 1}`);
    for (let off = 512; off < bytes.length; off += 256) {
      const address = [slot, off & 127, (off >> 7) & 127, (off >> 14) & 127];
      const p = await editorRequest(link, 12, [...address, ...packEditorBytes(bytes.subarray(off, off + 256))],
        p => p.length === 5 && address.every((v, i) => p[i] === v));
      if (p[4]) throw Error(`Stop bank ${slot + 1} write failed at ${off}: ${p[4]}`);
      report(slot, Math.floor((off + 256 - 512) * 99 / (bytes.length - 512)));
    }
    const end = await editorRequest(link, 13, [slot, ...packEditorBytes(bytes.subarray(0, 480))], p => p.length === 2 && p[0] === slot);
    if (end[1]) throw Error(`Stop bank ${slot + 1} verification failed: ${end[1]}`);
    status = await stopBankStatus(link, manifest);
    if (!(status.valid & (1 << slot))) throw Error(`Stop bank ${slot + 1} readback failed`);
    report(slot, 100);
  }
  if (!status.ready) throw Error('Stop banks are incomplete');
}
