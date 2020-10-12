auto Disc::CDDA::readBlock(uint portion) -> void {
  uint address = 4 + (portion << 1);

  for(uint id : range(2)) {
    uint8 header = adpcm.data[address];
    block[id].shift  = header.bit(0,3) > 12 ? 9 : header.bit(0,3);
    block[id].filter = header.bit(4,5);
    address++;
  }
  address = 16 + portion;

  for(uint nibble : range(28)) {
    uint8 value = adpcm.data[address];
    for(uint id : range(2)) {
      block[id].brr[nibble] = value >> (id << 2) & 15;
      address += 2;
    }
  }
}

auto Disc::CDDA::decodeBlock() -> void {
  static const i32 positive[4] = {0, 60, 115,  98};
  static const i32 negative[4] = {0,  0, -52, -55};

  for(uint id : range(2)) {
    uint lastID = adpcm.stereo ? id << 1 : 4;

    i16 lastSamples[2] = {adpcm.lastSamples[lastID + 0], adpcm.lastSamples[lastID + 1]};
    for(uint nibble : range(28)) {
      i32 sample = i16(block[id].brr[nibble] << 12) >> block[id].shift;
      sample += lastSamples[0] * positive[block[id].filter] >> 6;
      sample += lastSamples[1] * negative[block[id].filter] >> 6;
      lastSamples[1] = lastSamples[0];
      lastSamples[0] = sclamp<16>(sample);
      adpcm.currentSamples[id * 28 + nibble] = lastSamples[0];
    }
    adpcm.lastSamples[lastID + 0] = lastSamples[0];
    adpcm.lastSamples[lastID + 1] = lastSamples[1];
  }
}
