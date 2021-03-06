#include "processor.hpp"

#include "core/globals.hpp"

namespace top1::audio {
namespace detail {

  void registerAudioBufferResize(std::function<void(uint)> eventHandler) {
    Globals::events.bufferSizeChanged.add(eventHandler);
  }

} // detail
} // top1::audio
