//computes the distance between the current LBA and seeking LBA
auto Disc::Drive::distance() const -> uint {
  return 0;
}

auto Disc::Drive::clockSector() -> void {
  auto adpcmSector = [&]() -> bool {
    uint offset = 16;
    uint8 file    = sector.data[offset++];
    uint8 channel = sector.data[offset++];
    uint8 subMode = sector.data[offset++];

    uint1 audio    = subMode.bit(2);
    uint1 realtime = subMode.bit(6);
    if(audio == 1 && realtime == 1) {
      if(mode.xaFilter == 1) {
        if(file == self.adpcm.xaFilterFile && channel == self.adpcm.xaFilterChannel) {
          return true;
        }
      } else {
        return true;
      }
    }
    return false;
  };

  if(self.ssr.reading) {
    self.fd->seek(2448 * (abs(session->leadIn.lba) + lba.current));
    self.fd->read(sector.data, 2448);
    sector.offset = 0;

  //print("* CDC sector:", hex(lba.current, 8L), " mode:", hex(sector.data[15], 2L), " subMode:", hex(sector.data[18], 2L), "\n");

    if(!self.ssr.playingCDDA) {
      if(mode.xaADPCM == 1 && adpcmSector()) {
        self.fifo.adpcm.flush();
        for(uint offset : range(2340)) {
          self.fifo.adpcm.write(sector.data[12 + offset]);
        }
        lba.current++;
        return;
      }

      if(mode.sectorSize == 0) {
        self.fifo.data.flush();
        for(uint offset : range(2048)) {
          self.fifo.data.write(sector.data[24 + offset]);
        }
      }

      if(mode.sectorSize == 1) {
        self.fifo.data.flush();
        for(uint offset : range(2340)) {
          self.fifo.data.write(sector.data[12 + offset]);
        }
      }

      self.fifo.response.flush();
      self.fifo.response.write(self.status());

      self.irq.ready.flag = 1;
      self.irq.poll();

      lba.current++;
    } else if(cdda->playMode == Disc::CDDA::PlayMode::Normal) {
      lba.current++;
    } else if(cdda->playMode == Disc::CDDA::PlayMode::FastForward) {
      int end = 0;
      if(auto trackID = session->inTrack(lba.current)) {
        if(auto track = session->track(*trackID)) {
          if(auto index = track->index(track->lastIndex)) {
            end = index->end;
          }
        }
      }

      lba.current = min(end, lba.current + 10);
    } else if(cdda->playMode == Disc::CDDA::PlayMode::Rewind) {
      int start = 0;
      if(auto trackID = session->inTrack(lba.current)) {
        if(auto track = session->track(*trackID)) {
          if(auto index = track->index(1)) {
            start = index->lba;
          }
        }
      }

      lba.current = max(start, lba.current - 10);
    }
  }
}
