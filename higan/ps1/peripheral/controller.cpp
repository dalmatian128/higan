auto Peripheral::Controller::chipSelect(u8 data) -> bool {
  if(data == 0x01) {
    io.mode = IO::Mode::ControllerAccess;
    io.command = 0;
    return true;
  }
  return false;
}

auto Peripheral::Controller::transmit(u8 data) -> bool {
  auto mode = io.mode;
  switch(io.mode) {
  case IO::Mode::ControllerAccess:
    if(data == 0x42) io.mode = IO::Mode::ControllerIDLower;
    io.command = data;
    break;
  case IO::Mode::ControllerIDLower:
    if(data == 0x00) io.mode = IO::Mode::ControllerIDUpper;
    break;
  case IO::Mode::ControllerIDUpper:
    io.mode = IO::Mode::ControllerDataLower;
    break;
  case IO::Mode::ControllerDataLower:
    io.mode = IO::Mode::ControllerDataUpper;
    break;
  }
  return mode != io.mode;
}

auto Peripheral::Controller::receive() -> bool {
  if(io.command ==  0 ) access();
  if(io.command == 'B') button();
  return io.mode == IO::Mode::ControllerDataUpper;
}

auto Peripheral::Controller::access() -> void {
  if(io.mode == IO::Mode::ControllerAccess) {
    self.io.receiveData = 0xff;
    self.io.receiveSize = 1;
  }
}

auto Peripheral::Controller::button() -> void {
  if(io.mode == IO::Mode::ControllerIDLower) {
    self.io.receiveData = 0x41;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::ControllerIDUpper) {
    self.io.receiveData = 0x5a;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::ControllerDataLower) {
    maybe<u64> receive;
    if(self.io.slotNumber == 0 && controllerPort1.device) {
      receive = controllerPort1.device->read();
    }
    if(self.io.slotNumber == 1 && controllerPort2.device) {
      receive = controllerPort2.device->read();
    }
    self.io.receiveData = receive ? receive() >> 16 : 0xff;
    self.io.receiveSize = 1;
  }
  if(io.mode == IO::Mode::ControllerDataUpper) {
    maybe<u64> receive;
    if(self.io.slotNumber == 0 && controllerPort1.device) {
      receive = controllerPort1.device->read();
    }
    if(self.io.slotNumber == 1 && controllerPort2.device) {
      receive = controllerPort2.device->read();
    }
    self.io.receiveData = receive ? receive() >> 24 : 0xff;
    self.io.receiveSize = 1;
  }
}
