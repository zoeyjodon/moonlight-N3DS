/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015 Iwan Timmer
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include "../../system/AtomicVar.hpp"
#include "../../system/subscriber.hpp"
#include <3ds.h>
#include <Limelight.h>
#include <memory>

#define MOON_CTR_VIDEO_TEX_W 1024
#define MOON_CTR_VIDEO_TEX_H 512
// TODO: No idea why, but this seems to be the magic number to make dual screen
// offsets work
#define MOON_CTR_VIDEO_TEX_H_OFFSET 32
#define CMDLIST_SZ 0x800

class IN3dsRenderer {
  public:
    virtual ~IN3dsRenderer() = default;
    virtual void write_px_to_framebuffer(uint8_t *source) = 0;
    virtual void set_perf_decode_ticks(u64 ticks) = 0;
};

class N3dsRendererBase {
  public:
    N3dsRendererBase(gfxScreen_t screen_in, int surface_width_in,
                     int surface_height_in, int image_width_in,
                     int image_height_in, int pixel_size,
                     bool debug_in = false);
    virtual ~N3dsRendererBase();

  protected:
    inline void draw_perf_counters();
    void write_px_to_framebuffer_gpu(uint8_t *__restrict source);
    void ensure_3d_enabled();
    void ensure_3d_disabled();
    inline void write24(u8 *p, u32 val);

  public:
    u64 perf_frame_target_ticks = SYSCLOCK_ARM11 * ((double)(1.0 / 60.0));
    u64 perf_decode_ticks;
    u64 perf_fbcopy_ticks;

  protected:
    gfxScreen_t screen;
    int surface_width;
    int surface_height;
    int image_width;
    int image_height;
    int px_size;
    bool debug;
    u32 *cmdlist = NULL;
    void *vramFb = NULL;
    void *vramTex = NULL;
};

class N3dsRendererTop : public N3dsRendererBase {
  public:
    N3dsRendererTop(int dest_width, int dest_height, int src_width,
                    int src_height, int px_size, bool debug_in = false);
    ~N3dsRendererTop() = default;
    void write_px_to_framebuffer(uint8_t *source);
    void set_perf_decode_ticks(u64 ticks);
};

class N3dsRendererBottom : public N3dsRendererBase {
  public:
    N3dsRendererBottom(int src_width, int src_height, int px_size,
                       bool debug_in = false);
    ~N3dsRendererBottom() = default;
    void write_px_to_framebuffer(uint8_t *source);
    void write_px_to_framebuffer_raw(const uint8_t *source, int offset = 0,
                                     int size = 0);
};

class N3dsRendererMock : public IN3dsRenderer {
  public:
    N3dsRendererMock();
    ~N3dsRendererMock();
    void write_px_to_framebuffer(uint8_t *source) override;
    void set_perf_decode_ticks(u64 ticks) override;
};

class N3dsRendererNormal : public IN3dsRenderer {
  public:
    N3dsRendererNormal(int dest_width, int dest_height, int src_width,
                       int src_height, int px_size, bool debug = false);
    ~N3dsRendererNormal();
    void write_px_to_framebuffer(uint8_t *source) override;
    void set_perf_decode_ticks(u64 ticks) override;
    void set_bottom_screen(const uint8_t *source, int offset = 0, int size = 0);

  private:
    N3dsRendererTop top_renderer;
    N3dsRendererBottom bottom_renderer;
};

class N3dsRendererDualScreenStretch : public IN3dsRenderer {
  public:
    N3dsRendererDualScreenStretch(int dest_width, int dest_height,
                                  int src_width, int src_height, int px_size);
    ~N3dsRendererDualScreenStretch();
    void write_px_to_framebuffer(uint8_t *source) override;
    void set_perf_decode_ticks(u64 ticks) override;

  private:
    int source_offset;
    N3dsRendererTop top_renderer;
    N3dsRendererBottom bottom_renderer;
};

class N3dsRendererDualScreenMirror : public IN3dsRenderer {
  public:
    N3dsRendererDualScreenMirror(int dest_width, int dest_height, int src_width,
                                 int src_height, int px_size);
    ~N3dsRendererDualScreenMirror();
    void write_px_to_framebuffer(uint8_t *source) override;
    void set_perf_decode_ticks(u64 ticks) override;

  private:
    N3dsRendererTop top_renderer;
    N3dsRendererBottom bottom_renderer;
};

class N3dsRendererDualScreenMagnify : public IN3dsRenderer, ISubscriber {
  public:
    N3dsRendererDualScreenMagnify(int dest_width, int dest_height,
                                  int src_width, int src_height, int px_size);
    ~N3dsRendererDualScreenMagnify();
    void write_px_to_framebuffer(uint8_t *source) override;
    void set_perf_decode_ticks(u64 ticks) override;
    void set_crop_region(int center_x, int center_y);
    void accept(IMessage *message) override;

  private:
    int image_width;
    int image_height;
    int px_size;
    N3dsRendererTop top_renderer;
    N3dsRendererBottom bottom_renderer;
    AtomicVar<int> pixel_offset = 0;
};
