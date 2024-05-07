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

class IN3dsRenderer {
  public:
    IN3dsRenderer(int surface_width_in) { surface_width = surface_width_in; }
    virtual void write_px_to_framebuffer(uint8_t *source, int px_size) = 0;

  protected:
    int surface_width;
    void ensure_3d_enabled() {
        if (!gfxIs3D()) {
            gfxSetWide(false);
            gfxSet3D(true);
        }
    }
    void ensure_3d_disabled() {
        if (gfxIs3D()) {
            gfxSet3D(false);
        }
        if (surface_width == GSP_SCREEN_HEIGHT_TOP_2X) {
            gfxSetWide(true);
        }
    }
};

class N3dsRendererDefault : public IN3dsRenderer {
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

    inline void write_px_to_framebuffer_2D(uint8_t *source, int px_size);
    inline void write_px_to_framebuffer_3D(uint8_t *source, int px_size);

  private:
    int offset_lut_size;
    int *dest_offset_lut;
    int *src_offset_lut;

    int offset_lut_size_3d;
    int *dest_offset_lut_3d;
    int *src_offset_lut_3d_l;
    int *src_offset_lut_3d_r;
};

class N3dsRendererBottom : public IN3dsRenderer {
  public:
    N3dsRendererBottom(int src_width, int src_height, int px_size);
    ~N3dsRendererBottom();
    void write_px_to_framebuffer(uint8_t *source, int px_size);

  private:
    inline int get_dest_offset(int x, int y, int dest_height);
    inline int get_source_offset(int x, int y, int src_width, int src_height,
                                 int dest_width, int dest_height);

  private:
    int offset_lut_size;
    int *dest_offset_lut;
    int *src_offset_lut;
};

class N3dsRendererDualScreen : public IN3dsRenderer {
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
