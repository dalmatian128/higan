#pragma once

#include <nall/queue.hpp>
#include <nall/serializer.hpp>

namespace nall::DSP::Resampler {

struct FIR {
  auto inputFrequency() const -> f64 { return _inputFrequency; }
  auto outputFrequency() const -> f64 { return _outputFrequency; }

  auto reset(f64 inputFrequency, f64 outputFrequency = 0, u32 queueSize = 0) -> void;
  auto setInputFrequency(f64 inputFrequency) -> void;
  auto pending() const -> bool;
  auto full() const -> bool;
  auto flush() -> void;
  auto read() -> f64;
  auto write(f64 sample) -> void;
  auto serialize(serializer&) -> void;

private:
  auto euclidean(u32 x, u32 y) -> u32;

  f64 _inputFrequency;
  f64 _outputFrequency;

  u32 _divider;
  queue<f64> _samples;

  struct Sinc {
    auto factor() const -> u32 { return _factor; }

    auto reset(u32 factor, f64 gain = 1.0) -> void;
    auto process(f64 sample) -> f64;
    auto flush() -> void;
    auto serialize(serializer&) -> void;

  private:
    u32 _factor;
    u32 _taps;
    f64 _filter[32];
    f64 _ringBuffer[32];
    u8 _ringPosition;
  } _interpolation, _decimation;
};

inline auto FIR::reset(f64 inputFrequency, f64 outputFrequency, u32 queueSize) -> void {
  _inputFrequency = inputFrequency;
  _outputFrequency = outputFrequency ? outputFrequency : _inputFrequency;

  _samples.resize(queueSize ? queueSize : _outputFrequency * 0.02);  //default to 20ms max queue size

  _divider = euclidean(_outputFrequency, _inputFrequency);

  u32 interpolationFactor = _outputFrequency / _divider;
  _interpolation.reset(interpolationFactor, interpolationFactor);

  u32 decimationFactor = _inputFrequency / _divider;
  _decimation.reset(decimationFactor);
}

inline auto FIR::setInputFrequency(f64 inputFrequency) -> void {
  _inputFrequency = inputFrequency;
  _divider = euclidean(_outputFrequency, _inputFrequency);

  u32 interpolationFactor = _outputFrequency / _divider;
  if(interpolationFactor != _interpolation.factor()) {
    _interpolation.reset(interpolationFactor, interpolationFactor);
  }

  u32 decimationFactor = _inputFrequency / _divider;
  _decimation.reset(decimationFactor);
}

inline auto FIR::pending() const -> bool {
  return _samples.pending();
}

inline auto FIR::full() const -> bool {
  return _samples.full();
}

inline auto FIR::flush() -> void {
  _samples.flush();
  _interpolation.flush();
  _decimation.flush();
}

inline auto FIR::read() -> f64 {
  f64 sample = 0;
  for(u32 n : range(_decimation.factor())) {
    f64 s = 0;
    if(!_samples.empty()) s = _samples.read();
    if(n == 0) sample = _decimation.process(s);
    if(n != 0)          _decimation.process(s);
  }
  return sample;
}

inline auto FIR::write(f64 sample) -> void {
  for(u32 n : range(_interpolation.factor())) {
    f64 s;
    if(n == 0) s = _interpolation.process(sample);
    if(n != 0) s = _interpolation.process(0);
    if(!_samples.full()) _samples.write(s);
  }
}

inline auto FIR::serialize(serializer& s) -> void {
  s(_inputFrequency);
  s(_outputFrequency);
  s(_interpolation);
  s(_decimation);
}

inline auto FIR::euclidean(u32 x, u32 y) -> u32 {
  if(x == y) return 1;
  while(y != 0) {
    u32 r = x % y;
    x = y;
    y = r;
  }
  return x;
};

inline auto FIR::Sinc::reset(u32 factor, f64 gain) -> void
{
  _factor = factor;
  _taps = 4 * factor + 1;

  flush();

  //create sinc filter table
  f64 amplitude = gain / factor;
  s32 shift = (_taps - 1) / 2;
  for(s32 n : range(_taps)) {
    f64 x = Math::Pi * (n - shift) / factor;
    _filter[n] = x == 0 ? amplitude : amplitude * sin(x) / x;
  }
}

inline auto FIR::Sinc::process(f64 sample) -> f64
{
  _ringBuffer[_ringPosition] = sample;

  f64 sum = 0;
  for(auto n : range(_taps)) {
    u8 p = (_ringPosition - n) & 31;
    sum += _ringBuffer[p] * _filter[n];
  }

  _ringPosition = (_ringPosition + 1) & 31;

  sum = min(sum,  1.0);
  sum = max(sum, -1.0);
  return sum;
}

inline auto FIR::Sinc::flush() -> void {
  memory::fill<f64>(_ringBuffer, 32, 0);
  _ringPosition = 0;
}

inline auto FIR::Sinc::serialize(serializer& s) -> void {
  s(_factor);
  s(_taps);
  s(array_span<f64>{_filter, 32});
}

}
