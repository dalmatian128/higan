auto GPU::readGP0() -> u32 {
  uint32 data;

  if(io.mode == Mode::CopyFromVRAM) {
    for(uint loop : range(2)) {
      uint10 x = io.copy.x + io.copy.px;
       uint9 y = io.copy.y + io.copy.py;
      data = vram.readHalf((y * 1024 + x) * 2) << 16 | data >> 16;
      if(++io.copy.px >= io.copy.width) {
        io.copy.px = 0;
        if(++io.copy.py >= io.copy.height) {
          io.copy.py = 0;
          io.mode = Mode::Normal;
          return data;
        }
      }
    }
    return data;
  }

  return data;
}

auto GPU::writeGP0(u32 value) -> void {
  auto forceMask = [&]() -> u16 {
    return io.forceMaskBit && !io.colorDepth ? 0x8000 : 0;
  };

  auto checkMask = [&](u32 address) -> bool {
    if(io.checkMaskBit) {
      u16 data = vram.readHalf(address);
      if(data & 0x8000) return true;
    }
    return false;
  };

  if(io.mode == Mode::CopyToVRAM) {
    u16 mask = forceMask();
    for(uint loop : range(2)) {
      uint10 x = io.copy.x + io.copy.px;
       uint9 y = io.copy.y + io.copy.py;
      u32 address = (y * 1024 + x) * 2;
      if(!checkMask(address)) vram.writeHalf(address, value | mask);
      value >>= 16;
      if(++io.copy.px >= io.copy.width) {
        io.copy.px = 0;
        if(++io.copy.py >= io.copy.height) {
          io.copy.py = 0;
          io.mode = Mode::Normal;
          return;
        }
      }
    }
    return;
  }

  auto setPalette = [&](u32 data) -> void {
    io.texturePaletteX = data >> 16 &  0x3f;
    io.texturePaletteY = data >> 22 & 0x1ff;
  };

  auto setPage = [&](u32 data) -> void {
    io.texturePageBaseX = data >> 16 & 15;
    io.texturePageBaseY = data >> 20 &  1;
    io.semiTransparency = data >> 21 &  3;
    io.textureDepth     = data >> 23 &  3;
    io.textureDisable   = data >> 27 &  1;
  };

  auto& queue = this->queue.gp0;

   uint8 command = value >> 24;
  uint24 data    = value >>  0;

  if(queue.empty()) {
    queue.command = command;
  //print("* GP0(", hex(command, 2L), ") = ", hex(value, 6L), "\n");
  } else {
    command = queue.command;
  //print("* GP0(", hex(command, 2L), ") = ", hex(value, 8L), " [", queue.length, "]\n");
  }

  //unknown
  if(command == 0x00) {
    return;
  }

  //clear cache
  if(command == 0x01) {
    return;
  }

  //fill rectangle
  if(command == 0x02) {
    if(queue.write(value) < 3) return;
    uint16 color  = Color::to16(queue.data[0].bit(0,23));
    uint16 x      = (queue.data[1].bit( 0,15) & 1023) & ~15;
    uint16 y      = (queue.data[1].bit(16,31) & 511);
    uint16 width  = (queue.data[2].bit( 0,15) & 1023) + 15 & ~15;
    uint16 height = (queue.data[2].bit(16,31) & 511);
    for(uint py : range(height)) {
      for(uint px : range(width)) {
        vram.writeHalf(((y + py) * 1024 + (x + px)) * 2, color);
      }
    }
    return queue.reset();
  }

  //interrupt request
  case 0x1f: {
    io.interrupt = 1;
    interrupt.raise(Interrupt::GPU);
    return;
  }

  //monochrome triangle
  if(command == 0x20 || command == 0x21 || command == 0x22 || command == 0x23) {
    if(queue.write(value) < 4) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[0]).setPoint(queue.data[2]);
    auto v2 = Vertex().setColor(queue.data[0]).setPoint(queue.data[3]);
    if(command == 0x20 || command == 0x21) renderTriangle<Render::Color>(v0, v1, v2);
    if(command == 0x22 || command == 0x23) renderTriangle<Render::Color | Render::Alpha>(v0, v1, v2);
    return queue.reset();
  }

  //textured triangle
  if(command == 0x24 || command == 0x25 || command == 0x26 || command == 0x27) {
    if(queue.write(value) < 7) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto v1 = Vertex().setColor(queue.data[0]).setPoint(queue.data[3]).setTexel(queue.data[4]);
    auto v2 = Vertex().setColor(queue.data[0]).setPoint(queue.data[5]).setTexel(queue.data[6]);
    setPalette(queue.data[2]);
    setPage(queue.data[4]);
    if(command == 0x24) renderTriangle<Render::Texel | Render::ModulateColor>(v0, v1, v2);
    if(command == 0x25) renderTriangle<Render::Texel>(v0, v1, v2);
    if(command == 0x26) renderTriangle<Render::Texel | Render::Alpha | Render::ModulateColor>(v0, v1, v2);
    if(command == 0x27) renderTriangle<Render::Texel | Render::Alpha>(v0, v1, v2);
    return queue.reset();
  }

  //monochrome quadrilateral
  if(command == 0x28 || command == 0x29 || command == 0x2a || command == 0x2b) {
    if(queue.write(value) < 5) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[0]).setPoint(queue.data[2]);
    auto v2 = Vertex().setColor(queue.data[0]).setPoint(queue.data[3]);
    auto v3 = Vertex().setColor(queue.data[0]).setPoint(queue.data[4]);
    if(command == 0x28 || command == 0x29) renderQuadrilateral<Render::Color>(v0, v1, v2, v3);
    if(command == 0x2a || command == 0x2b) renderQuadrilateral<Render::Color | Render::Alpha>(v0, v1, v2, v3);
    return queue.reset();
  }

  //textured quadrilateral
  if(command == 0x2c || command == 0x2d || command == 0x2e || command == 0x2f) {
    if(queue.write(value) < 9) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto v1 = Vertex().setColor(queue.data[0]).setPoint(queue.data[3]).setTexel(queue.data[4]);
    auto v2 = Vertex().setColor(queue.data[0]).setPoint(queue.data[5]).setTexel(queue.data[6]);
    auto v3 = Vertex().setColor(queue.data[0]).setPoint(queue.data[7]).setTexel(queue.data[8]);
    setPalette(queue.data[2]);
    setPage(queue.data[4]);
    if(command == 0x2c) renderQuadrilateral<Render::Texel | Render::ModulateColor>(v0, v1, v2, v3);
    if(command == 0x2d) renderQuadrilateral<Render::Texel>(v0, v1, v2, v3);
    if(command == 0x2e) renderQuadrilateral<Render::Texel | Render::Alpha | Render::ModulateColor>(v0, v1, v2, v3);
    if(command == 0x2f) renderQuadrilateral<Render::Texel | Render::Alpha>(v0, v1, v2, v3);
    return queue.reset();
  }

  //shaded triangle
  if(command == 0x30 || command == 0x31 || command == 0x32 || command == 0x33) {
    if(queue.write(value) < 6) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[2]).setPoint(queue.data[3]);
    auto v2 = Vertex().setColor(queue.data[4]).setPoint(queue.data[5]);
    if(command == 0x30 || command == 0x31) renderTriangle<Render::Shade>(v0, v1, v2);
    if(command == 0x32 || command == 0x33) renderTriangle<Render::Shade | Render::Alpha>(v0, v1, v2);
    return queue.reset();
  }

  //shaded textured triangle
  if(command == 0x34 || command == 0x35 || command == 0x36 || command == 0x37) {
    if(queue.write(value) < 9) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto v1 = Vertex().setColor(queue.data[3]).setPoint(queue.data[4]).setTexel(queue.data[5]);
    auto v2 = Vertex().setColor(queue.data[6]).setPoint(queue.data[7]).setTexel(queue.data[8]);
    setPalette(queue.data[2]);
    setPage(queue.data[5]);
    if(command == 0x34 || command == 0x35) renderTriangle<Render::Texel | Render::ModulateShade>(v0, v1, v2);
    if(command == 0x36 || command == 0x37) renderTriangle<Render::Texel | Render::Alpha | Render::ModulateShade>(v0, v1, v2);
    return queue.reset();
  }

  //shaded quadrilateral
  if(command == 0x38 || command == 0x39 || command == 0x3a || command == 0x3b) {
    if(queue.write(value) < 8) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[2]).setPoint(queue.data[3]);
    auto v2 = Vertex().setColor(queue.data[4]).setPoint(queue.data[5]);
    auto v3 = Vertex().setColor(queue.data[6]).setPoint(queue.data[7]);
    if(command == 0x38 || command == 0x39) renderQuadrilateral<Render::Shade>(v0, v1, v2, v3);
    if(command == 0x3a || command == 0x3b) renderQuadrilateral<Render::Shade | Render::Alpha>(v0, v1, v2, v3);
    return queue.reset();
  }

  //shaded textured quadrilateral
  if(command == 0x3c || command == 0x3d || command == 0x3e || command == 0x3f) {
    if(queue.write(value) < 12) return;
    auto v0 = Vertex().setColor(queue.data[ 0]).setPoint(queue.data[ 1]).setTexel(queue.data[ 2]);
    auto v1 = Vertex().setColor(queue.data[ 3]).setPoint(queue.data[ 4]).setTexel(queue.data[ 5]);
    auto v2 = Vertex().setColor(queue.data[ 6]).setPoint(queue.data[ 7]).setTexel(queue.data[ 8]);
    auto v3 = Vertex().setColor(queue.data[ 9]).setPoint(queue.data[10]).setTexel(queue.data[11]);
    setPalette(queue.data[2]);
    setPage(queue.data[5]);
    if(command == 0x3c || command == 0x3d) renderQuadrilateral<Render::Texel | Render::ModulateShade>(v0, v1, v2, v3);
    if(command == 0x3e || command == 0x3f) renderQuadrilateral<Render::Texel | Render::Alpha | Render::ModulateShade>(v0, v1, v2, v3);
    return queue.reset();
  }

  //monochrome line
  if(command == 0x40 || command == 0x41 || command == 0x42 || command == 0x43) {
    if(queue.write(value) < 3) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[0]).setPoint(queue.data[2]);
    if(command == 0x40 || command == 0x41) renderSolidLine<Render::Color>(v0, v1);
    if(command == 0x42 || command == 0x43) renderSolidLine<Render::Color | Render::Alpha>(v0, v1);
    return queue.reset();
  }

  //monochrome polyline
  if(command == 0x48 || command == 0x4a || command == 0x4c || command == 0x4e) {
    if(value == 0x5555'5555 || value == 0x5000'5000) return queue.reset();
    if(queue.write(value) < 3) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[0]).setPoint(queue.data[2]);
    if(command == 0x48 || command == 0x4c) renderSolidLine<Render::Color>(v0, v1);
    if(command == 0x4a || command == 0x4e) renderSolidLine<Render::Color | Render::Alpha>(v0, v1);
    u32 color = queue.data[0];
    u32 point = queue.data[2];
    queue.reset();
    queue.write(color);
    queue.write(point);
    return;
  }

  //shaded line
  if(command == 0x50 || command == 0x51 || command == 0x52 || command == 0x53) {
    if(queue.write(value) < 4) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[2]).setPoint(queue.data[3]);
    if(command == 0x50 || command == 0x51) renderSolidLine<Render::Shade>(v0, v1);
    if(command == 0x52 || command == 0x53) renderSolidLine<Render::Shade | Render::Alpha>(v0, v1);
    return queue.reset();
  }

  //shaded polyline
  if(command == 0x58 || command == 0x5a || command == 0x5c || command == 0x5e) {
    if(value == 0x5555'5555 || value == 0x5000'5000) return queue.reset();
    if(queue.write(value) < 4) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto v1 = Vertex().setColor(queue.data[2]).setPoint(queue.data[3]);
    if(command == 0x58 || command == 0x5c) renderSolidLine<Render::Shade>(v0, v1);
    if(command == 0x5a || command == 0x5e) renderSolidLine<Render::Shade | Render::Alpha>(v0, v1);
    u32 color = queue.data[2];
    u32 point = queue.data[3];
    queue.reset();
    queue.write(color);
    queue.write(point);
    return;
  }

  //monochrome rectangle (variable size)
  if(command == 0x60 || command == 0x61 || command == 0x62 || command == 0x63) {
    if(queue.write(value) < 3) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto sz = Size().setSize(queue.data[2]);
    if(command == 0x60 || command == 0x61) renderRectangle<Render::Color>(v0, sz);
    if(command == 0x62 || command == 0x63) renderRectangle<Render::Color | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //textured rectangle (variable size)
  if(command == 0x64 || command == 0x65 || command == 0x66 || command == 0x67) {
    if(queue.write(value) < 4) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto sz = Size().setSize(queue.data[3]);
    setPalette(queue.data[2]);
    if(command == 0x64) renderRectangle<Render::Texel | Render::ModulateColor>(v0, sz);
    if(command == 0x65) renderRectangle<Render::Texel>(v0, sz);
    if(command == 0x66) renderRectangle<Render::Texel | Render::Alpha | Render::ModulateColor>(v0, sz);
    if(command == 0x67) renderRectangle<Render::Texel | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //monochrome rectangle (1x1 size)
  if(command == 0x68 || command == 0x69 || command == 0x6a || command == 0x6b) {
    if(queue.write(value) < 2) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto sz = Size().setSize(1, 1);
    if(command == 0x68 || command == 0x69) renderRectangle<Render::Color>(v0, sz);
    if(command == 0x6a || command == 0x6b) renderRectangle<Render::Color | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //textured rectangle (1x1 size)
  if(command == 0x6c || command == 0x6d || command == 0x6e || command == 0x6f) {
    if(queue.write(value) < 3) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto sz = Size().setSize(1, 1);
    setPalette(queue.data[2]);
    if(command == 0x6c) renderRectangle<Render::Texel | Render::ModulateColor>(v0, sz);
    if(command == 0x6d) renderRectangle<Render::Texel>(v0, sz);
    if(command == 0x6e) renderRectangle<Render::Texel | Render::Alpha | Render::ModulateColor>(v0, sz);
    if(command == 0x6f) renderRectangle<Render::Texel | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //monochrome rectangle (8x8 size)
  if(command == 0x70 || command == 0x71 || command == 0x72 || command == 0x73) {
    if(queue.write(value) < 2) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto sz = Size().setSize(8, 8);
    if(command == 0x70 || command == 0x71) renderRectangle<Render::Color>(v0, sz);
    if(command == 0x72 || command == 0x73) renderRectangle<Render::Color | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //textured rectangle (8x8 size)
  if(command == 0x74 || command == 0x75 || command == 0x76 || command == 0x77) {
    if(queue.write(value) < 3) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto sz = Size().setSize(8, 8);
    setPalette(queue.data[2]);
    if(command == 0x74) renderRectangle<Render::Texel | Render::ModulateColor>(v0, sz);
    if(command == 0x75) renderRectangle<Render::Texel>(v0, sz);
    if(command == 0x76) renderRectangle<Render::Texel | Render::Alpha | Render::ModulateColor>(v0, sz);
    if(command == 0x77) renderRectangle<Render::Texel | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //monochrome rectangle (16x16 size)
  if(command == 0x78 || command == 0x79 || command == 0x7a || command == 0x7b) {
    if(queue.write(value) < 2) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]);
    auto sz = Size().setSize(16, 16);
    if(command == 0x78 || command == 0x79) renderRectangle<Render::Color>(v0, sz);
    if(command == 0x7a || command == 0x7b) renderRectangle<Render::Color | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //textured rectangle (16x16 size)
  if(command == 0x7c || command == 0x7d || command == 0x7e || command == 0x7f) {
    if(queue.write(value) < 3) return;
    auto v0 = Vertex().setColor(queue.data[0]).setPoint(queue.data[1]).setTexel(queue.data[2]);
    auto sz = Size().setSize(16, 16);
    setPalette(queue.data[2]);
    if(command == 0x7c) renderRectangle<Render::Texel | Render::ModulateColor>(v0, sz);
    if(command == 0x7d) renderRectangle<Render::Texel>(v0, sz);
    if(command == 0x7e) renderRectangle<Render::Texel | Render::Alpha | Render::ModulateColor>(v0, sz);
    if(command == 0x7f) renderRectangle<Render::Texel | Render::Alpha>(v0, sz);
    return queue.reset();
  }

  //copy rectangle (VRAM to VRAM)
  if(command == 0x80) {
    if(queue.write(value) < 4) return;
    u16 sx     = (queue.data[1].bit( 0,15) & 1023);
    u16 sy     = (queue.data[1].bit(16,31) &  511);
    u16 tx     = (queue.data[2].bit( 0,15) & 1023);
    u16 ty     = (queue.data[2].bit(16,31) &  511);
    u16 width  = (queue.data[3].bit( 0,15) - 1 & 1023) + 1;
    u16 height = (queue.data[3].bit(16,31) - 1 &  511) + 1;
    u16 mask   = forceMask();
    for(uint y : range(height)) {
      u32 source = sx + (sy + y)*1024;
      u32 target = tx + (ty + y)*1024;
      for([[maybe_unused]] uint x : range(width)) {
        if(!checkMask(source)) {
          u16 data = vram.readHalf(source++ << 1);
          vram.writeHalf(target++ << 1, data | mask);
        } else {
          source++;
          target++;
        }
      }
    }
    return queue.reset();
  }

  //copy rectangle (CPU to VRAM)
  if(command == 0xa0) {
    if(queue.write(value) < 3) return;
    io.copy.x      = (queue.data[1].bit( 0,15) & 1023);
    io.copy.y      = (queue.data[1].bit(16,31) &  511);
    io.copy.width  = (queue.data[2].bit( 0,15) - 1 & 1023) + 1;
    io.copy.height = (queue.data[2].bit(16,31) - 1 &  511) + 1;
    io.copy.px     = 0;
    io.copy.py     = 0;
    io.mode        = Mode::CopyToVRAM;
    return queue.reset();
  }

  //copy rectangle (VRAM to CPU)
  if(command == 0xc0) {
    if(queue.write(value) < 3) return;
    io.copy.x      = (queue.data[1].bit( 0,15) & 1023);
    io.copy.y      = (queue.data[1].bit(16,31) &  511);
    io.copy.width  = (queue.data[2].bit( 0,15) - 1 & 1023) + 1;
    io.copy.height = (queue.data[2].bit(16,31) - 1 &  511) + 1;
    io.copy.px     = 0;
    io.copy.py     = 0;
    io.mode        = Mode::CopyFromVRAM;
    return queue.reset();
  }

  //draw mode
  if(command == 0xe1) {
    io.texturePageBaseX = data.bit( 0, 3);
    io.texturePageBaseY = data.bit( 4);
    io.semiTransparency = data.bit( 5, 6);
    io.textureDepth     = data.bit( 7, 8);
    io.dithering        = data.bit( 9);
    io.drawToDisplay    = data.bit(10);
    io.textureDisable   = data.bit(11);
    io.textureFlipX     = data.bit(12);
    io.textureFlipY     = data.bit(13);
    return;
  }

  //texture window
  if(command == 0xe2) {
    io.textureWindowMaskX   = data.bit( 0, 4);
    io.textureWindowMaskY   = data.bit( 5, 9);
    io.textureWindowOffsetX = data.bit(10,14);
    io.textureWindowOffsetY = data.bit(15,19);
    return;
  }

  //set drawing area (top left)
  if(command == 0xe3) {
    io.drawingAreaOriginX1 = data.bit( 0, 9);
    io.drawingAreaOriginY1 = data.bit(10,19);
    return;
  }

  //set drawing area (bottom right)
  if(command == 0xe4) {
    io.drawingAreaOriginX2 = data.bit( 0, 9);
    io.drawingAreaOriginY2 = data.bit(10,19);
    return;
  }

  //set drawing area
  if(command == 0xe5) {
    io.drawingAreaOffsetX = data.bit( 0,10);
    io.drawingAreaOffsetY = data.bit(11,21);
    return;
  }

  //mask bit
  if(command == 0xe6) {
    io.forceMaskBit = data.bit(0);
    io.checkMaskBit = data.bit(1);
    return;
  }

  debug(unimplemented, "GP0(", hex(command, 2L), ") = ", hex(value, 6L));
}
