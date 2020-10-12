struct Peripheral : Thread {
  Node::Component node;

  //peripheral.cpp
  auto load(Node::Object) -> void;
  auto unload() -> void;

  auto main() -> void;
  auto step(uint clocks) -> void;
  auto power(bool reset) -> void;

  //io.cpp
  auto readByte(u32 address) -> u32;
  auto readHalf(u32 address) -> u32;
  auto readWord(u32 address) -> u32;
  auto writeByte(u32 address, u32 data) -> void;
  auto writeHalf(u32 address, u32 data) -> void;
  auto writeWord(u32 address, u32 data) -> void;

  //serialization.cpp
  auto serialize(serializer&) -> void;

  struct IO {
    //JOY_RX_DATA
    uint64 receiveData;
     uint8 receiveSize;

    //JOY_TX_DATA
    uint64 transmitData;
     uint8 transmitSize;

    //JOY_STAT
     uint1 transmitStarted = 1;
     uint1 transmitFinished = 1;
     uint1 receiveParityError;
     uint1 acknowledgeLine;
     uint1 interrupt;
    uint21 baudrateTimer;

    //JOY_MODE
     uint2 baudrateReloadFactor;
     uint2 characterLength;
     uint1 parityEnable;
     uint1 parityType;
     uint2 unknownMode_6_7;
     uint1 clockOutputPolarity;
     uint7 unknownMode_9_15;

    //JOY_CTRL
     uint1 transmitEnable;
     uint1 joyOutput;
     uint1 receiveEnable;
     uint1 unknownCtrl_3;
     uint1 acknowledge;
     uint1 unknownCtrl_5;
     uint1 reset;
     uint1 unknownCtrl_7;
     uint2 receiveInterruptMode;
     uint1 transmitInterruptEnable;
     uint1 receiveInterruptEnable;
     uint1 acknowledgeInterruptEnable;
     uint1 slotNumber;
     uint2 unknownCtrl_14_15;

    //JOY_BAUD
    uint16 baudrateReloadValue;

    //internal
    enum class Device : uint {
      Null,
      Controller = 0x01,
      MemoryCard = 0x81,
    } device = Device::Null;

     int32 counter;
  } io;

  //controller.cpp
  struct Controller {
    Peripheral& self;
    Controller(Peripheral& self) : self(self) {}

    auto chipSelect(u8 data) -> bool;
    auto transmit(u8 data) -> bool;
    auto receive() -> bool;

    auto access() -> void;
    auto button() -> void;

    struct IO {
      enum class Mode : uint {
        Idle,
        ControllerAccess,
        ControllerIDLower,
        ControllerIDUpper,
        ControllerDataLower,
        ControllerDataUpper,
      } mode = Mode::ControllerAccess;

       uint8 command;
       uint8 data;
    } io;
  } controller{*this};

  //memorycard.cpp
  struct MemoryCard {
    Peripheral& self;
    MemoryCard(Peripheral& self) : self(self) {}

    auto chipSelect(u8 data) -> bool;
    auto transmit(u8 data) -> bool;
    auto receive() -> bool;

    auto access() -> void;
    auto read() -> void;
    auto write() -> void;

    struct IO {
      enum class Mode : uint {
        Idle,
        MemoryCardAccess,
        MemoryCardCommand,
        MemoryCardIDLower,
        MemoryCardIDUpper,
        MemoryCardCommandAddressMSB,
        MemoryCardCommandAddressLSB,
        MemoryCardAckLower,
        MemoryCardAckUpper,
        MemoryCardComfirmAddressMSB,
        MemoryCardComfirmAddressLSB,
        MemoryCardData,
        MemoryCardChecksum,
        MemoryCardEndByte,
      } mode = Mode::Idle;

       uint8 command;
       uint8 data;
       uint8 dataPre;
       uint8 checksum;
      uint16 sector;
       int32 offset;
       uint8 eCode;
    } io;
  } memoryCard{*this};
};

extern Peripheral peripheral;
