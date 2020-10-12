struct MemoryCardSlot {
  Node::Port port;
  unique_pointer<MemoryCard> device;

  //slot.cpp
  MemoryCardSlot(string name);
  auto load(Node::Object) -> void;
  auto unload() -> void;

  auto connect() -> void;
  auto disconnect() -> void;

  const string name;
};

extern MemoryCardSlot memoryCardSlot1;
extern MemoryCardSlot memoryCardSlot2;
