auto Peripheral::MemoryCard::chipSelect(u8 data) -> bool {
  if(data == 0x81) {
    if(self.io.slotNumber == 0 && memoryCardSlot1.device) {
      io.mode = IO::Mode::MemoryCardAccess;
    }
    if(self.io.slotNumber == 1 && memoryCardSlot2.device) {
      io.mode = IO::Mode::MemoryCardAccess;
    }
    io.command = 0;
    return true;
  }
  return false;
}

auto Peripheral::MemoryCard::transmit(u8 data) -> bool {
  io.data = data;
  auto mode = io.mode;
  switch(io.mode) {
  case IO::Mode::MemoryCardAccess:
    if(data == 'R') io.mode = IO::Mode::MemoryCardCommand;
    if(data == 'W') io.mode = IO::Mode::MemoryCardCommand;
    io.command = data;
    break;
  case IO::Mode::MemoryCardCommand:
    if(data == 0x00) io.mode = IO::Mode::MemoryCardIDLower;
    break;
  case IO::Mode::MemoryCardIDLower:
    if(data == 0x00) io.mode = IO::Mode::MemoryCardIDUpper;
    break;
  case IO::Mode::MemoryCardIDUpper:
    io.mode = IO::Mode::MemoryCardCommandAddressMSB;
    break;
  case IO::Mode::MemoryCardCommandAddressMSB:
    io.mode = IO::Mode::MemoryCardCommandAddressLSB;
    break;
  case IO::Mode::MemoryCardCommandAddressLSB:
    if(io.command == 'R' && data == 0x00) io.mode = IO::Mode::MemoryCardAckLower;
    if(io.command == 'W')                 io.mode = IO::Mode::MemoryCardData;
    io.offset = 0;
    break;
  case IO::Mode::MemoryCardAckLower:
    if(data == 0x00) io.mode = IO::Mode::MemoryCardAckUpper;
    break;
  case IO::Mode::MemoryCardAckUpper:
    if(io.command == 'R' && data == 0x00) io.mode = IO::Mode::MemoryCardComfirmAddressMSB;
    if(io.command == 'W' && data == 0x00) io.mode = IO::Mode::MemoryCardEndByte;
    break;
  case IO::Mode::MemoryCardComfirmAddressMSB:
    if(data == 0x00) io.mode = IO::Mode::MemoryCardComfirmAddressLSB;
    break;
  case IO::Mode::MemoryCardComfirmAddressLSB:
    if(data == 0x00) io.mode = IO::Mode::MemoryCardData;
    break;
  case IO::Mode::MemoryCardData:
    if(io.command == 'R' && data == 0x00) {
      if(++io.offset < 128) mode = IO::Mode::Idle;
      else io.mode = IO::Mode::MemoryCardChecksum;
    }
    if(io.command == 'W') {
      if(++io.offset < 128) mode = IO::Mode::Idle;
      else io.mode = IO::Mode::MemoryCardChecksum;
    }
    break;
  case IO::Mode::MemoryCardChecksum:
    if(io.command == 'R' && data == 0x00) io.mode = IO::Mode::MemoryCardEndByte;
    if(io.command == 'W')                 io.mode = IO::Mode::MemoryCardAckLower;
    break;
  }
  return mode != io.mode;
}

auto Peripheral::MemoryCard::receive() -> bool {
  if(io.command ==  0 ) access();
  if(io.command == 'R') read();
  if(io.command == 'W') write();
  return io.mode == IO::Mode::MemoryCardEndByte;
}

auto Peripheral::MemoryCard::access() -> void {
  if(io.mode == IO::Mode::MemoryCardAccess) {
    self.io.receiveSize = 0;
  }
}

auto Peripheral::MemoryCard::read() -> void {
  if(io.mode == IO::Mode::MemoryCardCommand) {
    if(self.io.slotNumber == 0 && memoryCardSlot1.device) {
      self.io.receiveData = memoryCardSlot1.device->flagByte;
      self.io.receiveSize = 1;
    }
    if(self.io.slotNumber == 1 && memoryCardSlot2.device) {
      self.io.receiveData = memoryCardSlot2.device->flagByte;
      self.io.receiveSize = 1;
    }
  }
  if(io.mode == IO::Mode::MemoryCardIDLower) {
    self.io.receiveData = 0x5a;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardIDUpper) {
    self.io.receiveData = 0x5d;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardCommandAddressMSB) {
    io.sector.bit(8,15) = io.data;
    io.checksum = io.data;
    self.io.receiveData = io.dataPre;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardCommandAddressLSB) {
    io.sector.bit(0, 7) = io.data;
    io.checksum ^= io.data;
    self.io.receiveData = io.dataPre;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardAckLower) {
    self.io.receiveData = 0x5c;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardAckUpper) {
    self.io.receiveData = 0x5d;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardComfirmAddressMSB) {
    self.io.receiveData = io.sector < 1024 ? io.sector.bit(8,15) : 0xff;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardComfirmAddressLSB) {
    self.io.receiveData = io.sector < 1024 ? io.sector.bit(0, 7) : 0xff;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardData) {
    uint sector = io.sector * 128 + io.offset;
    if(self.io.slotNumber == 0 && memoryCardSlot1.device) {
      self.io.receiveData = memoryCardSlot1.device->read(sector);
    }
    if(self.io.slotNumber == 1 && memoryCardSlot2.device) {
      self.io.receiveData = memoryCardSlot2.device->read(sector);
    }
    self.io.receiveSize = 1;
    io.checksum ^= self.io.receiveData;
  }
  if(io.mode == IO::Mode::MemoryCardChecksum) {
    self.io.receiveData = io.checksum;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardEndByte) {
    self.io.receiveData = 0x47;
    self.io.receiveSize = 1;
  }
  io.dataPre = io.data;
}

auto Peripheral::MemoryCard::write() -> void {
  if(io.mode == IO::Mode::MemoryCardCommand) {
    if(self.io.slotNumber == 0 && memoryCardSlot1.device) {
      self.io.receiveData = memoryCardSlot1.device->flagByte;
      self.io.receiveSize = 1;
    }
    if(self.io.slotNumber == 1 && memoryCardSlot2.device) {
      self.io.receiveData = memoryCardSlot2.device->flagByte;
      self.io.receiveSize = 1;
    }
  }
  if(io.mode == IO::Mode::MemoryCardIDLower) {
    self.io.receiveData = 0x5a;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardIDUpper) {
    self.io.receiveData = 0x5d;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardCommandAddressMSB) {
    io.sector.bit(8,15) = io.data;
    io.checksum  = io.data;
    self.io.receiveData = io.dataPre;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardCommandAddressLSB) {
    io.sector.bit(0, 7) = io.data;
    io.checksum ^= io.data;
    self.io.receiveData = io.dataPre;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardData) {
    uint sector = io.sector * 128 + io.offset;
    if(self.io.slotNumber == 0 && memoryCardSlot1.device) {
      memoryCardSlot1.device->write(sector, io.data);
    }
    if(self.io.slotNumber == 1 && memoryCardSlot2.device) {
      memoryCardSlot2.device->write(sector, io.data);
    }
    io.checksum ^= io.data;
    self.io.receiveData = io.dataPre;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardChecksum) {
    self.io.receiveData = io.dataPre;
    self.io.receiveSize = 1;
    io.eCode = io.data == io.checksum ? 0x47 : 0x4e;
  }
  if(io.mode == IO::Mode::MemoryCardAckLower) {
    self.io.receiveData = 0x5c;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardAckUpper) {
    self.io.receiveData = 0x5d;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::MemoryCardEndByte) {
    if(io.sector < 1024) {
      self.io.receiveData = io.eCode;
      self.io.receiveSize = 1;
    } else {
      self.io.receiveData = 0xff; //bad sector
      self.io.receiveSize = 1;
    }
  }
  io.dataPre = io.data;
}
