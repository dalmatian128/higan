auto MemoryCard::serialize(serializer& s) -> void {
  s.array(ram.data, ram.size);
}
