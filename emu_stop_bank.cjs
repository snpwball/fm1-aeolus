const fs = require('fs');
module.exports = function restoreStopBank(x) {
  if (process.env.ORGAN_SKIP_BANK === '1') return;
  const records = [];
  for (let slot = 0; slot < 2; slot++) {
    const image = fs.readFileSync(`build/organ-bank-${slot}.bin`);
    for (let at = 0; at < image.length; at += 4096) {
      const record = Buffer.alloc(4100, 255);
      record.writeUInt32LE(0xc2000 + slot * 0x14000 + at);
      image.copy(record, 4, at, at + 4096);
      records.push(record);
    }
  }
  const data = Buffer.concat(records), ptr = x.fm1_alloc(data.length);
  new Uint8Array(x.memory.buffer, ptr, data.length).set(data);
  if (x.fm1_flash_restore(ptr, data.length)) throw Error('Stop bank restore failed');
};
