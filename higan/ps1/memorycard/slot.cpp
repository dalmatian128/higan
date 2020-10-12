MemoryCardSlot memoryCardSlot1{"MemoryCard Slot 1"};
MemoryCardSlot memoryCardSlot2{"MemoryCard Slot 2"};

MemoryCardSlot::MemoryCardSlot(string name) : name(name) {
}

auto MemoryCardSlot::load(Node::Object parent) -> void {
  port = parent->append<Node::Port>(name);
  port->setFamily("PlayStation");
  port->setType("MemoryCard");
  port->setHotSwappable(true);
  port->setAllocate([&](string name) -> Node::Peripheral {
    if(name.size() > 0) device = new MemoryCard(name);
    if(device) return device->allocate(port);
    return {};
  });
  port->setConnect([&] { return connect(); });
  port->setDisconnect([&] { return disconnect(); });
}

auto MemoryCardSlot::unload() -> void {
  disconnect();
  port = {};
}

auto MemoryCardSlot::connect() -> void {
  if(device) {
    device->connect();
  }
}

auto MemoryCardSlot::disconnect() -> void {
  if(device) {
    device->disconnect();
    device = nullptr;
  }
}
