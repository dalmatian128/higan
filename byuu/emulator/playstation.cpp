#include <ps1/interface/interface.hpp>

struct PlayStation : Emulator {
  PlayStation();
  auto load() -> bool override;
  auto open(higan::Node::Object, string name, vfs::file::mode mode, bool required) -> shared_pointer<vfs::file> override;
  auto input(higan::Node::Input) -> void override;

  uint regionID = 0;
};

PlayStation::PlayStation() {
  interface = new higan::PlayStation::PlayStationInterface;
  medium = icarus::medium("PlayStation");
  manufacturer = "Sony";
  name = "PlayStation";

  firmware.append({"BIOS", "US"});      //NTSC-U
  firmware.append({"BIOS", "Japan"});   //NTSC-J
  firmware.append({"BIOS", "Europe"});  //PAL
}

auto PlayStation::load() -> bool {
  regionID = 0;  //default to NTSC-U region (for audio CDs)
  if(auto manifest = medium->manifest(game.location)) {
    auto document = BML::unserialize(manifest);
    auto region = document["game/region"].string();
    //if statements below are ordered by lowest to highest priority
    if(region == "PAL"   ) regionID = 2;
    if(region == "NTSC-J") regionID = 1;
    if(region == "NTSC-U") regionID = 0;
  }

  if(!file::exists(firmware[regionID].location)) {
    errorFirmwareRequired(firmware[regionID]);
    return false;
  }

  if(auto region = root->find<higan::Node::String>("Region")) {
    region->setValue("NTSC-U → NTSC-J → PAL");
  }

  if(auto fastBoot = root->find<higan::Node::Boolean>("Fast Boot")) {
    fastBoot->setValue(settings.general.fastBoot);
  }

  if(auto port = root->find<higan::Node::Port>("PlayStation/Disc Tray")) {
    port->allocate();
    port->connect();
  }

  if(auto port = root->find<higan::Node::Port>("Controller Port 1")) {
    port->allocate("Gamepad");
    port->connect();
  }

  if(auto port = root->find<higan::Node::Port>("MemoryCard Slot 1")) {
    port->allocate("flash1.rom");
    port->connect();
  }

  if(auto port = root->find<higan::Node::Port>("MemoryCard Slot 2")) {
    port->allocate("flash2.rom");
    port->connect();
  }

  return true;
}

auto PlayStation::open(higan::Node::Object node, string name, vfs::file::mode mode, bool required) -> shared_pointer<vfs::file> {
  if(name == "bios.rom") {
    return Emulator::loadFirmware(firmware[regionID]);
  }

  if(name == "manifest.bml") {
    if(game.manifest = medium->manifest(game.location)) {
      return vfs::memory::open(game.manifest.data<uint8_t>(), game.manifest.size());
    }
    return Emulator::manifest(game.location);
  }

  if(name == "cd.rom") {
    if(game.location.iendsWith(".zip")) {
      MessageDialog().setText(
        "Sorry, compressed CD-ROM images are not currently supported.\n"
        "Please extract the image prior to loading it."
      ).setAlignment(presentation).error();
      return {};
    }

    if(auto result = vfs::cdrom::open(game.location)) return result;

    MessageDialog().setText(
      "Failed to load CD-ROM image."
    ).setAlignment(presentation).error();
  }

  if(name == "program.exe") {
    return vfs::memory::open(game.image.data(), game.image.size());
  }

  if(name == "flash1.rom") {
    auto location = locate(game.location, "_1.mcd", settings.paths.saves);
    if(auto result = vfs::disk::open(location, mode)) return result;
  }

  if(name == "flash2.rom") {
    auto location = locate(game.location, "_2.mcd", settings.paths.saves);
    if(auto result = vfs::disk::open(location, mode)) return result;
  }

  return {};
}

auto PlayStation::input(higan::Node::Input node) -> void {
  auto name = node->name();
  maybe<InputMapping&> mapping;
  if(name == "Up"      ) mapping = virtualPad.up;
  if(name == "Down"    ) mapping = virtualPad.down;
  if(name == "Left"    ) mapping = virtualPad.left;
  if(name == "Right"   ) mapping = virtualPad.right;
  if(name == "Cross"   ) mapping = virtualPad.a;
  if(name == "Circle"  ) mapping = virtualPad.b;
  if(name == "Square"  ) mapping = virtualPad.x;
  if(name == "Triangle") mapping = virtualPad.y;
  if(name == "L1"      ) mapping = virtualPad.lb;
  if(name == "L2"      ) mapping = virtualPad.lt;
  if(name == "R1"      ) mapping = virtualPad.rb;
  if(name == "R2"      ) mapping = virtualPad.rt;
  if(name == "Select"  ) mapping = virtualPad.back;
  if(name == "Start"   ) mapping = virtualPad.start;
  if(name == "LX-axis" ) mapping = virtualPad.lStickAxisX;
  if(name == "LY-axis" ) mapping = virtualPad.lStickAxisY;
  if(name == "RX-axis" ) mapping = virtualPad.rStickAxisX;
  if(name == "RY-axis" ) mapping = virtualPad.rStickAxisY;
  if(name == "L-thumb" );
  if(name == "R-thumb" );

  if(mapping) {
    auto value = mapping->value();
    if(auto axis = node->cast<higan::Node::Axis>()) {
      axis->setValue(value);
    }
    if(auto button = node->cast<higan::Node::Button>()) {
      button->setValue(value);
    }
  }
}
