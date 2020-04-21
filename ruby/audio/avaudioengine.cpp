#include <IOKit/audio/IOAudioTypes.h>
#include <AVFoundation/AVFoundation.h>

struct AudioAVAudioEngine : AudioDriver {
  AudioAVAudioEngine& self = *this;
  AudioAVAudioEngine(Audio& super) : AudioDriver(super) {}
  ~AudioAVAudioEngine() { terminate(); }

  auto create() -> bool override {
    return super.setLatency(20);
  }

  auto driver() -> string override { return "AVAudioEngine"; }
  auto ready() -> bool override { return _ready; }

  auto hasDevices() -> vector<string> override {
    vector<string> devices;
    NSDictionary *dictionary = getOutputDevices();
    for(NSString *key in dictionary) {
      devices.append([key UTF8String]);
    }
    return devices;
  }

  auto hasBlocking() -> bool override {
    return true;
  }

  auto hasChannels() -> vector<uint> override {
    return {1, 2};
  }

  auto hasFrequencies() -> vector<uint> override {
    AudioObjectID deviceID;
    if(self.device == "Default") {
      deviceID = getDefaultOutputDevice();
    } else {
      NSDictionary *dictionary = getOutputDevices();
      deviceID = findOutputDevice(self.device, dictionary);
    }
    NSArray<NSNumber*> *sampleRates = getOutputDeviceSampleRates(deviceID);

    vector<uint> frequencies;
    for(NSNumber* rate in sampleRates) frequencies.append([rate unsignedIntValue]);
    return frequencies;
//    return {32040, 44100, 48000, 96000};
  }

  auto hasLatencies() -> vector<uint> override {
    return {20, 40, 60, 80, 100};
  }

  auto setDevice(string device) -> bool override { return initialize(); }
  auto setBlocking(bool blocking) -> bool override { return true; }
  auto setChannels(u32 channels) -> bool override { return initialize(); }
  auto setFrequency(u32 frequency) -> bool override { return initialize(); }
  auto setLatency(u32 latency) -> bool override { return initialize(); }

  auto clear() -> void override {
    double samples[2] = {};
    while(_frameCount > 0) {
      output(samples);
    }
//    [_engine pause];
  }

  auto level() -> double override {
    if (_engine == nil) return super.level();
    return [[_engine mainMixerNode] outputVolume];
  }

  auto output(const double samples[]) -> void override {
    if(_frameCount == 0) {
      dispatch_time_t timeout = self.blocking ? DISPATCH_TIME_FOREVER : DISPATCH_TIME_NOW;
      auto ret = dispatch_semaphore_wait(_frameBoundarySemaphore, timeout);
      if(ret != 0) return;
      _currentFrameIndex = (_currentFrameIndex + 1) % _kMaxInflightBuffers;
    }

    AVAudioPCMBuffer *buffer = _dynamicPCMBuffers[_currentFrameIndex];
    for(auto ch : range([[buffer format] channelCount])) {
      float *channelData = [buffer floatChannelData][ch];
      channelData[_frameCount] = samples[ch];
    }
    if(++_frameCount < [buffer frameLength]) return;

    __block dispatch_semaphore_t semaphore = _frameBoundarySemaphore;
    if([_engine isRunning]) {
      [_player scheduleBuffer:buffer completionHandler:^(void) {
        dispatch_semaphore_signal(semaphore);
        semaphore = nil;
      }];
      if([_player isPlaying] != YES) {
        [_player play];
      }
    } else {
      dispatch_time_t when = dispatch_time(DISPATCH_TIME_NOW, self.latency * 1000 * 1000);
      dispatch_after(when, _dummyAudioQueue, ^{
        dispatch_semaphore_signal(semaphore);
        semaphore = nil;
      });
    }

    _frameCount = 0;
  }

private:
  auto initialize() -> bool {
    terminate();

    if(self.frequency == 0 || self.channels == 0 || self.latency == 0)
      return false;

    @autoreleasepool {
      AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:self.frequency channels:self.channels];

      AVAudioFrameCount frameLength = self.frequency * self.latency / 1000;
      AVAudioFrameCount frameCapacity = sizeof(float) * frameLength * self.channels;
      NSMutableArray *mutableDynamicPCMBuffers = [NSMutableArray arrayWithCapacity:_kMaxInflightBuffers];

      for(auto i = 0; i < _kMaxInflightBuffers; i++) {
        AVAudioPCMBuffer *dynamicPCMBuffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:format frameCapacity:frameCapacity];
        [dynamicPCMBuffer setFrameLength:frameLength];
        [mutableDynamicPCMBuffers addObject:dynamicPCMBuffer];
      }

      _dynamicPCMBuffers = [mutableDynamicPCMBuffers copy];
      _currentFrameIndex = _kMaxInflightBuffers - 1;
      _frameCount = 0;

      _engine = [[AVAudioEngine alloc] init];
      _player = [[AVAudioPlayerNode alloc] init];
      [_engine attachNode:_player];

      NSDictionary *dictionary = getOutputDevices();
      AudioObjectID deviceID = findOutputDevice(self.device, dictionary);
      if(deviceID == kAudioObjectUnknown) {
        deviceID = getDefaultOutputDevice();
      }
      AVAudioOutputNode *output = [_engine outputNode];
      NSError *error;
      [[output AUAudioUnit] setDeviceID:deviceID error:&error];
      if(error != nil) {
        NSLog(@"setDeviceID %@", error.localizedFailureReason);
        return false;
      }

      AVAudioMixerNode *mainMixer = [_engine mainMixerNode];
      [_engine connect:_player to:mainMixer format:format];

      _prepared = false;
      _observer = [[NSNotificationCenter defaultCenter] addObserverForName:AVAudioEngineConfigurationChangeNotification object:_engine queue:[NSOperationQueue mainQueue] usingBlock:^(NSNotification *note) {
        dispatch_async(dispatch_get_main_queue(), ^{
          if(_prepared) {
            _prepared = false;
            self.device = "Default";
            initialize();
          } else {
            NSError *error;
            [_engine startAndReturnError:&error];
            if(error != nil) {
              NSLog(@"startAndReturnError %@", error.localizedFailureReason);
              return;
            }
            _prepared = true;
          }
        });
      }];

      if(@available(macOS 11.0, *)) {
        [_engine startAndReturnError:&error];
        if(error != nil) {
          NSLog(@"startAndReturnError %@", error.localizedFailureReason);
          return _ready;
        }
        _prepared = true;
      }

      _frameBoundarySemaphore = dispatch_semaphore_create(_kMaxInflightBuffers);
      _dummyAudioQueue = dispatch_queue_create("dev.ares.dummyAudioQueue", DISPATCH_QUEUE_SERIAL);
    }

    _ready = true;

    return _ready;
  }

  auto terminate() -> void {
    _ready = false;

    @autoreleasepool {
      if(_frameBoundarySemaphore != nil) {
        NSUInteger waitInflightBuffers = _kMaxInflightBuffers;
        if(_frameCount > 0) waitInflightBuffers--;
        for(auto i = 0; i < waitInflightBuffers; i++)
          dispatch_semaphore_wait(_frameBoundarySemaphore, DISPATCH_TIME_FOREVER);
        for(auto i = 0; i < _kMaxInflightBuffers; i++)
          dispatch_semaphore_signal(_frameBoundarySemaphore);

        _dummyAudioQueue = nil;
        _frameBoundarySemaphore = nil;
      }

      if(_engine != nil) {
        [[NSNotificationCenter defaultCenter] removeObserver:_observer];
        [_engine stop];

        [_engine disconnectNodeOutput:_player];
        [_engine detachNode:_player];

        _observer = nil;
        _player = nil;
        _engine = nil;

        _dynamicPCMBuffers = nil;
      }
    }
  }

  auto getOutputDevices() -> NSDictionary * {
    AudioObjectPropertyAddress propertyAddress = {
      .mSelector = kAudioHardwarePropertyDevices,
      .mScope    = kAudioObjectPropertyScopeGlobal,
      .mElement  = kAudioObjectPropertyElementMaster
    };

    UInt32 dataSize = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize);
    if(err != noErr || dataSize == 0) {
      return nil;
    }

    UInt32 deviceCount = dataSize / sizeof(AudioObjectID);
    AudioObjectID *deviceIDs = new AudioObjectID[deviceCount];
    if(deviceIDs == nil) {
      return nil;
    }
    err = AudioObjectGetPropertyData(kAudioObjectSystemObject, &propertyAddress, 0, NULL, &dataSize, deviceIDs);
    if(err != noErr) {
      delete[] deviceIDs;
      return nil;
    }

    NSMutableDictionary *mutableAudioDevices = [NSMutableDictionary dictionaryWithCapacity:deviceCount];

    for(auto i : range(deviceCount)) {
      UInt32 terminalType = isOutputDevice(deviceIDs[i]);
      switch (terminalType) {
      case 0:
        continue;
      case kAudioStreamTerminalTypeHeadphones:
        break;
      default:
        break;
      }
      NSString *deviceName = getOutputDeviceName(deviceIDs[i]);
      if(deviceName == nil)
        continue;
      [mutableAudioDevices setObject:[NSNumber numberWithUnsignedInt:deviceIDs[i]] forKey:deviceName];
    }

    delete[] deviceIDs;
    deviceIDs = nil;

    return [mutableAudioDevices copy];
  }

  auto getDefaultOutputDevice() -> AudioObjectID {
    AudioObjectPropertyAddress propertyAddress = {
      .mSelector = kAudioHardwarePropertyDefaultOutputDevice,
      .mScope    = kAudioObjectPropertyScopeGlobal,
      .mElement  = kAudioObjectPropertyElementMaster
    };
    AudioObjectID deviceID;

    UInt32 dataSize = sizeof(deviceID);
    OSStatus err = AudioObjectGetPropertyData(AudioObjectID(kAudioObjectSystemObject), &propertyAddress, 0, nil, &dataSize, &deviceID);
    if(err != noErr || deviceID == kAudioObjectUnknown) {
      return kAudioObjectUnknown;
    }

    return deviceID;
  }

  auto getOutputDeviceName(AudioObjectID deviceID) -> NSString * {
    AudioObjectPropertyAddress propertyAddress = {
      .mSelector = kAudioObjectPropertyName,
      .mScope    = kAudioObjectPropertyScopeGlobal,
      .mElement  = kAudioObjectPropertyElementMaster
    };
    CFStringRef deviceName = nil;

    UInt32 dataSize = sizeof(deviceName);
    OSStatus err = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &dataSize, &deviceName);
    if(err != noErr || deviceName == nil) {
      return nil;
    }

    return (__bridge_transfer NSString *)deviceName;
  }

  auto getOutputDeviceSampleRates(AudioObjectID deviceID) -> NSArray<NSNumber*> * {
    AudioObjectPropertyAddress propertyAddress = {
      .mSelector = kAudioDevicePropertyAvailableNominalSampleRates,
      .mScope    = kAudioObjectPropertyScopeGlobal,
      .mElement  = kAudioObjectPropertyElementMaster
    };

    UInt32 dataSize = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, NULL, &dataSize);
    if(err != noErr || dataSize == 0) {
      return nil;
    }

    UInt32 rateCount = dataSize / sizeof(AudioValueRange);
    AudioValueRange *sampleRates = new AudioValueRange[rateCount];
    if(sampleRates == nil) {
      return nil;
    }
    err = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &dataSize, sampleRates);
    if(err != noErr) {
      delete[] sampleRates;
      return nil;
    }

    NSMutableArray *mutableSampleRates = [NSMutableArray arrayWithCapacity:rateCount];
    for(auto i : range(rateCount)) {
      NSNumber *rate = [NSNumber numberWithDouble:sampleRates[i].mMinimum];
      [mutableSampleRates addObject:rate];
    }

    delete[] sampleRates;
    sampleRates = nil;

    return [mutableSampleRates copy];
  }

  auto isOutputDevice(AudioObjectID deviceID) -> UInt32 {
    AudioObjectPropertyAddress propertyAddress = {
      .mSelector = kAudioDevicePropertyStreams,
      .mScope    = kAudioObjectPropertyScopeOutput,
      .mElement  = kAudioObjectPropertyElementMaster
    };

    UInt32 dataSize = 0;
    OSStatus err = AudioObjectGetPropertyDataSize(deviceID, &propertyAddress, 0, NULL, &dataSize);
    if(err != noErr || dataSize == 0) {
      return 0;
    }

    UInt32 count = dataSize / sizeof(AudioObjectID);
    AudioObjectID *streamIDs = new AudioObjectID[count];
    if(streamIDs == nil) {
      return 0;
    }
    err = AudioObjectGetPropertyData(deviceID, &propertyAddress, 0, NULL, &dataSize, streamIDs);
    if(err != noErr) {
      return 0;
    }

    propertyAddress.mSelector = kAudioStreamPropertyTerminalType;
    propertyAddress.mScope    = kAudioObjectPropertyScopeGlobal;
    UInt32 terminalType = 0;
    for(auto i : range(count)) {
      dataSize = sizeof(UInt32);
      err = AudioObjectGetPropertyData(streamIDs[i], &propertyAddress, 0, NULL, &dataSize, &terminalType);
      if(err != noErr) {
        continue;
      }
    }

    delete[] streamIDs;
    streamIDs = nil;

    return terminalType;
  }

  auto findOutputDevice(string &deviceName, NSDictionary *dictionary) -> AudioObjectID {
    NSString *target = [NSString stringWithUTF8String:deviceName];
    id value = [dictionary objectForKey:target];
    return value != nil ? [value unsignedIntValue] : kAudioObjectUnknown;
  }

  bool _ready = false;
  bool _prepared = false;

  AVAudioEngine *_engine = nil;
  AVAudioPlayerNode *_player = nil;

  AVAudioFrameCount _frameCount = 0;

  static constexpr NSUInteger _kMaxInflightBuffers = 3;
  dispatch_semaphore_t _frameBoundarySemaphore = nil;
  NSArray <AVAudioPCMBuffer *> *_dynamicPCMBuffers = nil;
  NSUInteger _currentFrameIndex = 0;

  dispatch_queue_t _dummyAudioQueue = nil;

  id _observer = nil;
};
