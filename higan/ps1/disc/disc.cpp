#include <ps1/ps1.hpp>

namespace higan::PlayStation {

Disc disc;
#include "drive.cpp"
#include "cdda.cpp"
#include "adpcm.cpp"
#include "io.cpp"
#include "command.cpp"
#include "irq.cpp"
#include "serialization.cpp"

auto Disc::load(Node::Object parent) -> void {
  node = parent->append<Node::Component>("PlayStation");

  tray = node->append<Node::Port>("Disc Tray");
  tray->setFamily("PlayStation");
  tray->setType("Compact Disc");
  tray->setHotSwappable(true);
  tray->setAllocate([&](auto name) { return allocate(tray); });
  tray->setConnect([&] { return connect(); });
  tray->setDisconnect([&] { return disconnect(); });

  fifo.parameter.resize(16);
  fifo.response.resize(16);
  fifo.data.resize(2340);
  fifo.adpcm.resize(2340);

  //subclass simulation
  drive.session = session;
  drive.cdda = cdda;
  cdda.drive = drive;

  cdda.load(node);
}

auto Disc::unload() -> void {
  cdda.unload();

  fifo.parameter.reset();
  fifo.response.reset();
  fifo.data.reset();
  fifo.adpcm.reset();

  disconnect();
  tray.reset();
  node.reset();
}

auto Disc::allocate(Node::Port parent) -> Node::Peripheral {
  return cd = parent->append<Node::Peripheral>("PlayStation");
}

auto Disc::connect() -> void {
  cd->setManifest([&] { return information.manifest; });

  information = {};
  if(auto fp = platform->open(cd, "manifest.bml", File::Read, File::Required)) {
    information.manifest = fp->reads();
  }

  auto document = BML::unserialize(information.manifest);
  information.name = document["game/label"].string();
  information.region = document["game/region"].string();
  information.executable = (bool)document["game/executable"];

  if(!executable()) {
    fd = platform->open(cd, "cd.rom", File::Read, File::Required);
    if(!fd) return disconnect();

    //read disc TOC (table of contents)
    uint sectors = fd->size() / 2448;
    vector<uint8_t> subchannel;
    subchannel.resize(sectors * 96);
    for(uint sector : range(sectors)) {
      fd->seek(sector * 2448 + 2352);
      fd->read(subchannel.data() + sector * 96, 96);
    }
    session.decode(subchannel, 96);
  }
}

auto Disc::disconnect() -> void {
  fd.reset();
  cd.reset();
}

auto Disc::main() -> void {
  counter.sector += 128;
  if(counter.sector >= 451584 >> drive.mode.speed) {
    //75hz (single speed) or 150hz (double speed)
    counter.sector -= 451584 >> drive.mode.speed;
    drive.clockSector();
  }

  counter.adpcm += 128;
  if(counter.adpcm >= cdda.adpcm.clock) {
    //337.5hz (stereo) or 168.75hz (mono)
    counter.adpcm -= cdda.adpcm.clock;
    cdda.clockSector();
  }

  counter.cdda += 128;
  if(counter.cdda >= 768) {
    //44100hz
    counter.cdda -= 768;
    cdda.clockSample();
  }

  if(event.counter > 0) {
    event.counter -= 128;
    if(event.counter <= 0) {
      event.counter = 0;
      command(event.command);
    }
  }

  if(drive.mode.report && ssr.playingCDDA) {
    counter.report -= 128;
    if(counter.report <= 0) {
      counter.report += 33'868'800 / 75;

      maybe<uint8_t> trackID = {};
      maybe<uint8_t> indexID = {};
      int lba = drive.lba.current;
      uint8_t flag = (lba % 75) & 0x10 ? 1 << 7 : 0;
      if(trackID = session.inTrack(lba)) {
        if(auto track = session.track(*trackID)) {
          if(indexID = track->inIndex(lba)) {
            if(auto index = track->index(*indexID)) {
              if(flag) lba -= index->lba;
            }
          }
        }
      }
      auto [minute, second, frame] = CD::MSF::fromLBA(lba);

      fifo.response.flush();
      fifo.response.write(status());
      fifo.response.write(CD::BCD::encode(*trackID));
      fifo.response.write(CD::BCD::encode(*indexID));
      fifo.response.write(0x00 | CD::BCD::encode(minute));
      fifo.response.write(flag | CD::BCD::encode(second));
      fifo.response.write(0x00 | CD::BCD::encode(frame));
      fifo.response.write(0x00);  //peak lo
      fifo.response.write(0x00);  //peak hi

      irq.ready.flag = 1;
      irq.poll();
    }
  }

  step(128);
}

auto Disc::step(uint clocks) -> void {
  Thread::clock += clocks;
}

auto Disc::power(bool reset) -> void {
  Thread::reset();
  event = {};
  io = {};
}

}
