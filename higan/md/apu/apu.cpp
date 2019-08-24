#include <md/md.hpp>

namespace higan::MegaDrive {

APU apu;
#include "bus.cpp"
#include "serialization.cpp"

auto APU::load(Node::Object parent, Node::Object from) -> void {
  logger.attach(tracer);
  tracer->setSource("apu");
  tracer->setAddressBits(24);  //really 16; used for alignment with 68K CPUs

  logger.attach(onInterrupt);
  onInterrupt->setSource("apu");
  onInterrupt->setName("interrupt");
}

auto APU::unload() -> void {
  logger.detach(tracer);
  logger.detach(onInterrupt);
}

auto APU::main() -> void {
  if(!state.enabled) {
    return step(1);
  }

  if(state.nmiLine) {
    state.nmiLine = 0;  //edge-sensitive
    irq(0, 0x0066, 0xff);
  }

  if(state.intLine) {
    //level-sensitive
    irq(1, 0x0038, 0xff);
  }

  if(tracer->enabled() && tracer->address(r.pc)) {
    tracer->instruction(disassembleInstruction(r.pc), disassembleContext());
  }

  instruction();
}

auto APU::step(uint clocks) -> void {
  Thread::step(clocks);
  Thread::synchronize(cpu, vdp, psg, ym2612);
}

auto APU::serializing() const -> bool {
  return scheduler.serializing();
}

auto APU::setNMI(bool value) -> void {
  state.nmiLine = value;
}

auto APU::setINT(bool value) -> void {
  state.intLine = value;
}

auto APU::enable(bool value) -> void {
  //68K cannot disable the Z80 without bus access
  if(!bus->granted() && !value) return;
  if(state.enabled && !value) reset();
  state.enabled = value;
}

auto APU::power(bool reset) -> void {
  Z80::bus = this;
  Z80::power();
  bus->grant(false);
  Thread::create(system.frequency() / 15.0, {&APU::main, this});
  if(!reset) ram.allocate(8_KiB);
  state = {};
}

auto APU::reset() -> void {
  Z80::power();
  bus->grant(false);
  Thread::create(system.frequency() / 15.0, {&APU::main, this});
  state = {};
}

}
