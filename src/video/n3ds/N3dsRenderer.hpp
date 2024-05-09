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

#include <3ds.h>
#include <Limelight.h>

#define MOON_CTR_VIDEO_TEX_W 1024
#define MOON_CTR_VIDEO_TEX_H 512
#define CMDLIST_SZ 0x800

class N3dsRendererBase {
  public:
    N3dsRendererBase(int surface_width_in, int surface_height_in,
                     int image_width_in, int image_height_in, int pixel_size);
    ~N3dsRendererBase();
    virtual void write_px_to_framebuffer(uint8_t *source, int px_size) = 0;

  public:
    u64 perf_frame_target_ticks = SYSCLOCK_ARM11 * ((double)(1.0 / 60.0));
    u64 perf_decode_ticks;
    u64 perf_fbcopy_ticks;

  protected:
    inline void draw_perf_counters(uint8_t *__restrict dest, int px_size);
    void write_px_to_framebuffer_gpu(uint8_t *__restrict source,
                                     uint8_t *__restrict dest,
                                     uint8_t *__restrict dest_debug,
                                     int px_size);
    void ensure_3d_enabled();
    void ensure_3d_disabled();
    inline void write24(u8 *p, u32 val);

  protected:
    int surface_width;
    int surface_height;
    int image_width;
    int image_height;
    u32 *cmdlist = NULL;
    void *vramFb = NULL;
    void *vramTex = NULL;
};

class N3dsRendererDefault : public N3dsRendererBase {
  public:
    N3dsRendererDefault(int dest_width, int dest_height, int src_width,
                        int src_height, int px_size);
    ~N3dsRendererDefault();
    void write_px_to_framebuffer(uint8_t *source, int px_size);

  private:
    inline int get_dest_offset(int x, int y, int dest_height);
    inline int get_source_offset(int x, int y, int src_width, int src_height,
                                 int dest_width, int dest_height);
    inline int get_source_offset_3d_l(int x, int y, int src_width,
                                      int src_height, int dest_width,
                                      int dest_height);
    inline int get_source_offset_3d_r(int x, int y, int src_width,
                                      int src_height, int dest_width,
                                      int dest_height);

    inline void init_px_to_framebuffer_2d(int dest_width, int dest_height,
                                          int src_width, int src_height,
                                          int px_size);
    inline void init_px_to_framebuffer_3d(int dest_width, int dest_height,
                                          int src_width, int src_height,
                                          int px_size);

    inline void write_px_to_framebuffer_2D(uint8_t *__restrict source,
                                           uint8_t *__restrict scratch,
                                           int px_size);
    inline void write_px_to_framebuffer_3D(uint8_t *__restrict source,
                                           int px_size);

  private:
    int offset_lut_size;
    int *dest_offset_lut;
    int *src_offset_lut;

    int offset_lut_size_3d;
    int *dest_offset_lut_3d;
    int *src_offset_lut_3d_l;
    int *src_offset_lut_3d_r;
};

class N3dsRendererBottom : public N3dsRendererBase {
  public:
    N3dsRendererBottom(int src_width, int src_height, int px_size);
    ~N3dsRendererBottom();
    void write_px_to_framebuffer(uint8_t *source, int px_size);
};

class N3dsRendererDualScreen : public N3dsRendererBase {
  public:
    N3dsRendererDualScreen(int dest_width, int dest_height, int src_width,
                           int src_height, int px_size);
    ~N3dsRendererDualScreen();
    void write_px_to_framebuffer(uint8_t *source, int px_size);

  private:
    inline int get_dest_offset(int x, int y, int dest_height);

    inline int get_source_offset_ds_top(int x, int y, int src_width,
                                        int src_height, int dest_width,
                                        int dest_height);

    inline int get_source_offset_ds_bottom(int x, int y, int src_width,
                                           int src_height, int dest_width,
                                           int dest_height);

    inline void init_px_to_framebuffer_ds(int dest_width, int dest_height,
                                          int src_width, int src_height,
                                          int px_size);

  private:
    int offset_lut_size;
    int *dest_offset_lut;

    int offset_lut_size_ds_bottom;
    int *dest_offset_lut_ds_bottom;
    int *src_offset_lut_ds_top;
    int *src_offset_lut_ds_bottom;
};
