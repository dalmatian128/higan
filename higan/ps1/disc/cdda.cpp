auto Disc::CDDA::load(Node::Object parent) -> void {
  stream = parent->append<Node::Stream>("CD-DA");
  stream->setChannels(2);
  stream->setFrequency(44100);
  //28 * 2 * 4 * 44100 / 18900
  sample.resampler[0].reset(37800, 44100, 1024);
  sample.resampler[1].reset(37800, 44100, 1024);
  adpcm.clock = 100352;
}

auto Disc::CDDA::unload() -> void {
  stream.reset();
}

auto Disc::CDDA::clockSector() -> void {
  if(self.fifo.adpcm.empty()) return;

  if(self.fifo.adpcm.size() == 2340) {
    for([[maybe_unused]] uint offset : range(7)) {
      self.fifo.adpcm.read();
    }

    uint8 data = self.fifo.adpcm.read();
    adpcm.stereo        = data.bit(0);
    adpcm.sampleRate    = data.bit(2);
    adpcm.bitsPerSample = data.bit(4);
    adpcm.emphasis      = data.bit(6);

    if(adpcm.bitsPerSample) debug(unimplemented, "XA-ADPCM 8bit/sample");

    double frequency = adpcm.sampleRate ? 18900 : 37800;
    for(uint ch : range(2)) {
      if(sample.resampler[ch].inputFrequency() != frequency) {
        sample.resampler[ch].setInputFrequency(frequency);
      }
    }
    adpcm.clock = (200704 >> adpcm.stereo) << adpcm.sampleRate;

    //skip copy of sub-header
    for([[maybe_unused]] uint offset : range(4)) {
      self.fifo.adpcm.read();
    }
  }

  if(self.fifo.adpcm.size() > 128) {
    for(uint offset : range(128)) {
      adpcm.data[offset] = self.fifo.adpcm.read();
    }
    for(uint portion : range(4)) {
      readBlock(portion);
      decodeBlock();
      //resample to 44100Hz
      for(uint ch : range(1 + adpcm.stereo)) {
        for(uint nibble : range(56 >> adpcm.stereo)) {
          sample.resampler[ch].write(adpcm.currentSamples[ch * 28 + nibble] / 32768.0);
        }
      }
    }
  }
}

auto Disc::CDDA::clockSample() -> void {
  sample.left  = 0;
  sample.right = 0;

  auto amplify = [&](double sample, u8 volume) -> double {
    return sample * volume / 256;
  };

  if(self.ssr.reading && self.ssr.playingCDDA) {
    i16 l = 0;
    i16 r = 0;

    l |= drive->sector.data[drive->sector.offset++] << 0;
    l |= drive->sector.data[drive->sector.offset++] << 8;
    r |= drive->sector.data[drive->sector.offset++] << 0;
    r |= drive->sector.data[drive->sector.offset++] << 8;

    sample.left  = l / 32768.0;
    sample.right = r / 32768.0;
  }

  if(drive->mode.xaADPCM) {
    double l = 0;
    double r = 0;

    if(adpcm.stereo) {
      if(sample.resampler[0].pending()) l = sample.resampler[0].read();
      if(sample.resampler[1].pending()) r = sample.resampler[1].read();
    } else {
      if(sample.resampler[0].pending()) l = sample.resampler[0].read();
      r = l;
    }

    if(!self.adpcm.mute) {
      sample.left  += amplify(l, volume[0]);
      sample.right += amplify(l, volume[1]);
      sample.left  += amplify(r, volume[2]);
      sample.right += amplify(r, volume[3]);
    }
  }

  stream->sample(sample.left, sample.right);
}
