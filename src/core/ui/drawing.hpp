#pragma once

#include <fmt/format.h>
#include <plog/Log.h>

#include "util/typedefs.hpp"
#include "filesystem.hpp"

#include "core/ui/canvas.hpp"

namespace top1::ui::drawing {

  const uint WIDTH = 320;
  const uint HEIGHT = 240;

  namespace Colours {

    const Colour Black      = Colour(0x000000);

    const MainColour White  = {0xFFFFFF, 0x646464};
    const MainColour Yellow = {0xFAB700, 0x646464};
    const MainColour Red    = {0xE6332A, 0x6E0C0C};
    const MainColour Green  = {0x5ECF3E, 0x0C6E0C};
    const MainColour Blue   = {0x009EE3, 0x0C0C6E};

    const Colour Gray60     = 0x999999;
    const Colour Gray70     = 0xB2B2B2;

  } // Colours

  namespace Fonts {
    inline Font Light;
    inline Font Norm;
    inline Font Bold;

    inline fs::path font_dir {"data/fonts"};
  }

  inline void initUtils(Canvas &canvas) {
    Fonts::Light = Font(canvas, "TOP-1 Light", Fonts::font_dir / "TOP-1" / "TOP-1.ttf");
    if (!Fonts::Light.valid()) {
      LOGE << "Invalid font: " << Fonts::Light.name;
    }
    Fonts::Norm = Font(canvas, "TOP-1 Regular", Fonts::font_dir / "TOP-1" / "TOP-1.ttf");
    if (!Fonts::Norm.valid()) {
      LOGE << "Invalid font: " << Fonts::Norm.name;
    }
    Fonts::Bold = Font(canvas, "TOP-1 Bold", Fonts::font_dir / "TOP-1" / "TOP-1.ttf");
    if (!Fonts::Bold.valid()) {
      LOGE << "Invalid font: " << Fonts::Bold.name;
    }
  }

} // ui::drawing
