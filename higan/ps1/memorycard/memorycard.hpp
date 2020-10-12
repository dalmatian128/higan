struct MemoryCard {
  Node::Peripheral node;
  Shared::File fd;

  //memorycard.cpp
  MemoryCard(const string& name);

  auto allocate(Node::Port) -> Node::Peripheral;
  auto connect() -> void;
  auto disconnect() -> void;

  auto power() -> void;

  auto read(u32 address) -> u8;
  auto write(u32 address, u8 data) -> void;
  auto save() -> void;
  auto format() -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;

  Memory::Writable ram;
  string name;
  u8 flagByte;
};

#include "slot.hpp"
