#include <ps1/ps1.hpp>

namespace higan::PlayStation {

Peripheral peripheral;
#include "io.cpp"
#include "serialization.cpp"
#include "controller.cpp"
#include "memorycard.cpp"

auto Peripheral::load(Node::Object parent) -> void {
  node = parent->append<Node::Component>("Peripheral");
}

auto Peripheral::unload() -> void {
  node.reset();
}

auto Peripheral::main() -> void {
  if(io.transmitSize && (io.transmitEnable || io.transmitStarted) && !io.acknowledgeLine) {
    int32 counter = 0;
    u8 data = io.transmitData;
    io.transmitSize--;
    io.transmitStarted = 1;
    switch(io.device) {
    case IO::Device::Null:
      if(controller.chipSelect(data)) {
        counter = 340;
        io.device = IO::Device::Controller;
      }
      if(memoryCard.chipSelect(data)) {
        counter = 170;
        io.device = IO::Device::MemoryCard;
      }
      break;
    case IO::Device::Controller:
      if(controller.transmit(data)) {
        counter = 340;
      }
      break;
    case IO::Device::MemoryCard:
      if(memoryCard.transmit(data)) {
        counter = 170;
      }
      break;
    }
    if(counter) {
      io.transmitFinished = 0;
      io.counter = counter;
    } else {
      debug(unimplemented, "Peripheral slot(", hex(io.slotNumber, 1L), ") = ", hex(data, 2L));
      io.device = IO::Device::Null;
    }
  }

  if(io.counter > 0) {
    io.counter -= 8;
    if(io.counter == 164 || io.counter == 82) io.transmitFinished = 1;
    if(io.counter <= 0) {
      io.counter = 0;
      io.acknowledgeLine = 1;
      if(io.acknowledgeInterruptEnable) {
        io.interrupt = 1;
        interrupt.raise(Interrupt::Peripheral);
      }
      switch(io.device) {
      case IO::Device::Controller:
        if(controller.receive()) io.device = IO::Device::Null;
        break;
      case IO::Device::MemoryCard:
        if(memoryCard.receive()) io.device = IO::Device::Null;
        break;
      }
    }
  }

  io.baudrateTimer -= 8;
  if(io.baudrateTimer <= 0) {
    io.baudrateTimer = io.baudrateReloadValue;
  }

  step(8);
}

auto Peripheral::step(uint clocks) -> void {
  Thread::clock += clocks;
}

auto Peripheral::power(bool reset) -> void {
  Thread::reset();
  io = {};
}

}
