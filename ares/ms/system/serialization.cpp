auto System::serialize(bool synchronize) -> serializer {
  if(synchronize) scheduler.enter(Scheduler::Mode::Synchronize);
  serializer s;

  u32  signature = SerializerSignature;
  char version[16] = {};
  char description[512] = {};
  memory::copy(&version, (const char*)SerializerVersion, SerializerVersion.size());

  s(signature);
  s(synchronize);
  s(version);
  s(description);

  serialize(s, synchronize);
  return s;
}

auto System::unserialize(serializer& s) -> bool {
  u32  signature = 0;
  bool synchronize = true;
  char version[16] = {};
  char description[512] = {};

  s(signature);
  s(synchronize);
  s(version);
  s(description);

  if(signature != SerializerSignature) return false;
  if(string{version} != SerializerVersion) return false;

  if(synchronize) power();
  serialize(s, synchronize);
  return true;
}

auto System::serialize(serializer& s, bool synchronize) -> void {
  scheduler.setSynchronize(synchronize);
  s(cartridge);
  s(cpu);
  s(vdp);
  s(psg);
  if(MasterSystem::Model::MasterSystem()) {
    s(controllerPort1);
    s(controllerPort2);
  }
  if(MasterSystem::Region::NTSCJ()) {
    if(MasterSystem::Model::MarkIII()) {
      s(expansionPort);
    }
    if(MasterSystem::Model::MasterSystemI()) {
      s(opll);
    }
    if(MasterSystem::Model::MasterSystemII()) {
      s(opll);
    }
  }
}