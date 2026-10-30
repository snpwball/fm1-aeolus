export async function seedStopBanks(firmware, saved, hash) {
  if (!firmware.stopBanks) return saved;
  const mapped = new Map(saved.map(s => [s.at, s]));
  for (const bank of firmware.stopBanks) {
    if (bank.slot !== 0 && bank.slot !== 1) throw Error('Invalid stop bank slot');
    const response = await fetch(bank.file);
    if (!response.ok) throw Error('Stop bank unavailable');
    const bytes = new Uint8Array(await response.arrayBuffer());
    if (bytes.length !== 0x14000 || await hash(bytes) !== bank.sha256) throw Error('Stop bank checksum mismatch');
    for (let off = 0; off < bytes.length; off += 4096) {
      const at = 0xc2000 + bank.slot * 0x14000 + off;
      mapped.set(at, {at, data: bytes.slice(off, off + 4096)});
    }
  }
  return [...mapped.values()];
}
