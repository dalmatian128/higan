#include <ps1/ps1.hpp>

namespace higan::PlayStation {

#include "slot.cpp"
#include "serialization.cpp"

MemoryCard::MemoryCard(const string& name) {
  if(Location::suffix(name) != ".rom") {
    this->name = "flash.rom";
  } else {
    this->name = name;
  }
}

auto MemoryCard::allocate(Node::Port parent) -> Node::Peripheral {
  return node = parent->append<Node::Peripheral>(name);
}

auto MemoryCard::connect() -> void {
  ram.allocate(128_KiB);
  fd = platform->open(node, name, File::Read);
  if(fd) {
    ram.load(fd);
  } else {
    format();
  }
  flagByte = 0x08;
}

auto MemoryCard::disconnect() -> void {
  if(!node) return;
  if((flagByte & 0x08) == 0) save();
  ram.reset();
  node = {};
}

auto MemoryCard::power() -> void {
}

auto MemoryCard::read(u32 address) -> u8 {
  return ram.readByte(address & ram.size - 1);
}

auto MemoryCard::write(u32 address, u8 data) -> void {
  flagByte &= ~0x08;
  return ram.writeByte(address & ram.size - 1, data);
}

auto MemoryCard::save() -> void {
  if(!node) return;

  fd = platform->open(node, name, File::Write);
  if(fd) {
    fd->write(ram.data, ram.size);
  }
}

auto MemoryCard::format(void) -> void {
  auto checksum = [&](uint sector) -> u8 {
    u8 sum = ram.readByte(sector++);
    while(sector < 127) {
      sum ^= ram.readByte(sector++);
    }
    return sum;
  };

  ram.fill(0xffffffff);

  //header (frame 0)
  ram.writeByte(  0, 'M');
  ram.writeByte(  1, 'C');
  for(uint i : range(2, 127)) ram.writeByte(i, 0);
  ram.writeByte(127, checksum(0));

  //directory (frame 1 - 15)
  for(uint frame : range(1, 16)) {
    uint sector = frame * 128;
    ram.writeWord(sector +   0, 0x000000a0);  //free (freshly formatted)
    ram.writeWord(sector +   4, 0x00000000);  //file size
    ram.writeHalf(sector +   8, 0xffff);      //pointer to next file
    for(uint i : range(10, 127)) ram.writeByte(sector + i, 0x00);
    ram.writeByte(sector + 127, checksum(sector));
  }

  //broken sector list (frame 16 - 35)
  for(uint frame : range(16, 36)) {
    uint sector = frame * 128;
    ram.writeWord(sector +   0, 0xffffffff);  //broken sector number (none)
    ram.writeWord(sector +   4, 0x00000000);  //garbage
    ram.writeHalf(sector +   8, 0xffff);      //pointer to next file
    for(uint i : range(10, 127)) ram.writeByte(sector + i, 0x00);
    ram.writeByte(sector + 127, checksum(sector));
  }

  // broken sector replacement data (frame 36 - 55)
  for(uint frame : range(36, 56)) {
    uint sector = frame * 128;
    for(uint i : range(128)) ram.writeByte(sector + i, 0x00);
  }

  // unused frames (frame 56 - 62)
  for(uint frame : range(56, 63)) {
    uint sector = frame * 128;
    for(uint i : range(128)) ram.writeByte(sector + i, 0x00);
  }

  // write test frame (frame 63)
  uint source =  0 * 128;
  uint target = 63 * 128;
  for(uint i : range(128)) {
    u8 data = ram.readByte(source + i);
    ram.writeByte(target + i, data);
  }
}

}
