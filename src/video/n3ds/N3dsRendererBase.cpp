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

#include "N3dsRenderer.hpp"
#include "vshader_shbin.h"

#include <3ds.h>
#include <cstdlib>
#include <cstring>
#include <stdbool.h>
#include <stdexcept>
#include <unistd.h>

N3dsRendererBase::N3dsRendererBase(gfxScreen_t screen_in, int surface_width_in,
                                   int surface_height_in, int image_width_in,
                                   int image_height_in, int pixel_size,
                                   bool debug_in)
    : screen(screen_in), surface_width(surface_width_in),
      surface_height(surface_height_in), image_width(image_width_in),
      image_height(image_height_in), debug(debug_in), px_size(pixel_size) {
    cmdlist = (u32 *)linearAlloc(CMDLIST_SZ * 4);
    vramFb = vramAlloc(surface_width * surface_height * px_size);
    // Needs to be able to hold an 800x480
    vramTex = vramAlloc(MOON_CTR_VIDEO_TEX_W * MOON_CTR_VIDEO_TEX_H * px_size);
}

N3dsRendererBase::~N3dsRendererBase() {
    linearFree(cmdlist);
    vramFree(vramFb);
    vramFree(vramTex);

    // Clear both screens
    u8 *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    memset(top, 0, surface_width * GSP_SCREEN_WIDTH * px_size);
    gfxScreenSwapBuffers(GFX_TOP, true);

    GSPGPU_FramebufferFormat px_fmt_btm = gfxGetScreenFormat(GFX_BOTTOM);
    int px_size_btm = gspGetBytesPerPixel(px_fmt_btm);
    u8 *btm = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);
    memset(btm, 0, GSP_SCREEN_HEIGHT_BOTTOM * GSP_SCREEN_WIDTH * px_size_btm);
    gfxScreenSwapBuffers(GFX_BOTTOM, true);

    // Return to the default display width before exiting
    if (surface_width == GSP_SCREEN_HEIGHT_TOP_2X) {
        gfxSetWide(false);
    }
    printf("Closing stream...");
}

void N3dsRendererBase::ensure_3d_enabled() {
    if (!gfxIs3D()) {
        gfxSetWide(false);
        gfxSet3D(true);
    }
}

void N3dsRendererBase::ensure_3d_disabled() {
    if (gfxIs3D()) {
        gfxSet3D(false);
    }
    if (surface_width == GSP_SCREEN_HEIGHT_TOP_2X) {
        gfxSetWide(true);
    }
}

inline void N3dsRendererBase::write24(u8 *p, u32 val) {
    p[0] = val;
    p[1] = val >> 8;
    p[2] = val >> 16;
}

inline void N3dsRendererBase::draw_perf_counters() {
    u8 *dest = gfxGetFramebuffer(screen, GFX_LEFT, NULL, NULL);

    // Use a line going across the first scanline (left) for the perf counters.
    // Clear to black
    memset(dest, 0, GSP_SCREEN_WIDTH * 3);

    // Display frame target in the middle of the screen.
    double perf_tick_divisor =
        ((double)GSP_SCREEN_WIDTH) / ((double)(perf_frame_target_ticks * 2));
    u32 perf_px = 0;
    u32 perf_tmp_height = 0;

#define PERF_DRAW(ticks, r, g, b)                                              \
    perf_tmp_height = perf_tick_divisor * ((double)(ticks));                   \
    do {                                                                       \
        if (perf_px > GSP_SCREEN_WIDTH)                                        \
            break;                                                             \
        const u32 color = (r << 16) | (g << 8) | b;                            \
        memcpy(dest + (perf_px * 3), &color, 3);                               \
        perf_px++;                                                             \
    } while (perf_tmp_height-- > 0);

    PERF_DRAW(perf_decode_ticks, 255, 0, 0);
    PERF_DRAW(perf_fbcopy_ticks, 0, 0, 255);

    // Draw two green pixels at the center
    perf_px = (GSP_SCREEN_WIDTH / 2) - 1;
    PERF_DRAW(0, 0, 255, 0);
    PERF_DRAW(0, 0, 255, 0);
}

void N3dsRendererBase::write_px_to_framebuffer_gpu(uint8_t *__restrict source) {
    // Do nothing when GPU right is lost, otherwise we hang when going to
    // the home menu.
    if (!gspHasGpuRight()) {
        return;
    }

    u64 start_ticks = svcGetSystemTick();

    // Tile the source image into the scratch buffer.
    tile_source_to_vram(source);

    // Build and submit GPU command list to perform the transform/draw.
    build_and_submit_gpu_cmdlist_for_transform();

    // Process the prepared command list and wait for completion.
    process_cmdlist_and_wait();

    // Copy the transformed framebuffer into the display framebuffer.
    copy_vram_to_framebuffer_to_screen(source);

    // Finalize: perf counting and buffer swap.
    finalize_frame_and_swap(start_ticks);
}

void N3dsRendererBase::tile_source_to_vram(uint8_t *__restrict source) {
    // Transfer the decoded source into a scratch tiled texture in VRAM.
    // - MOON_CTR_VIDEO_TEX_W/H: texture dimensions (1024x512) chosen to
    //   accommodate the largest expected source and align to PICA tile sizes.
    // - GX_TRANSFER_FLIP_VERT(1): source coordinate system is flipped
    //   vertically relative to PICA's origin, so flip during transfer.
    // - GX_TRANSFER_OUT_TILED(1): store in tiled layout for faster GPU access.
    GX_DisplayTransfer(
        (u32 *)source,
        GX_BUFFER_DIM(MOON_CTR_VIDEO_TEX_W, MOON_CTR_VIDEO_TEX_H),
        (u32 *)vramTex,
        GX_BUFFER_DIM(MOON_CTR_VIDEO_TEX_W, MOON_CTR_VIDEO_TEX_H),
        GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) |
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
            GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565));
}

void N3dsRendererBase::build_and_submit_gpu_cmdlist_for_transform() {
    GPUCMD_SetBuffer(cmdlist, CMDLIST_SZ, 0);

    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
    // GPU expects framebuffer address in units of 8 bytes, so shift right by 3.
    GPUCMD_AddWrite(GPUREG_COLORBUFFER_LOC, osConvertVirtToPhys(vramFb) >> 3);
    GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_LOC, 0);
    // Render/Framebuffer dim register layout:
    //  - bit24 (1<<24) enables the register's special mode expected by PICA
    //  - bits [23:12] hold (width - 1)
    //  - bits [11:0] hold height
    GPUCMD_AddWrite(GPUREG_RENDERBUF_DIM,
                    (1 << 24) | ((surface_width - 1) << 12) | surface_height);
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_DIM,
                    (1 << 24) | ((surface_width - 1) << 12) | surface_height);
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_BLOCK32, 0);

    GPUCMD_AddWrite(GPUREG_DEPTH_COLOR_MASK, 0xF << 8); // Write RGBA, no depth
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST1, 0);
    GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);
    GPUCMD_AddWrite(GPUREG_COLORBUFFER_FORMAT, GPU_RGB565 << 16);
    GPUCMD_AddWrite(GPUREG_COLORBUFFER_READ, 0x0);
    GPUCMD_AddWrite(GPUREG_COLORBUFFER_WRITE, 0xF);
    GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_READ, 0);
    GPUCMD_AddWrite(GPUREG_DEPTHBUFFER_WRITE, 0);

    GPUCMD_AddWrite(GPUREG_VIEWPORT_XY, 0);

    // Note: width/height are swapped and halved here because the draw
    // performs a 90-degree transform (rotation) and the PICA viewport
    // expects half-scale values in this configuration.
    // - f32tof24/f32tof31 convert floats to the fixed-point format required
    //   by the GPU registers.
    // - the `<< 1` on INVW/INVH aligns the fixed-point format expected by PICA.
    GPUCMD_AddWrite(GPUREG_VIEWPORT_WIDTH, f32tof24(surface_height / 2));
    GPUCMD_AddWrite(GPUREG_VIEWPORT_INVW,
                    f32tof31(2.0 / ((double)surface_height)) << 1);
    GPUCMD_AddWrite(GPUREG_VIEWPORT_HEIGHT, f32tof24(surface_width / 2));
    GPUCMD_AddWrite(GPUREG_VIEWPORT_INVH,
                    f32tof31(2.0 / ((double)surface_width)) << 1);

    GPUCMD_AddWrite(GPUREG_SCISSORTEST_MODE, 0);
    GPUCMD_AddWrite(GPUREG_SCISSORTEST_POS, 0);
    GPUCMD_AddWrite(GPUREG_SCISSORTEST_DIM, 0);

    GPUCMD_AddWrite(GPUREG_DEPTHMAP_ENABLE, 1);
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_SCALE, f32tof24(-1.0));
    GPUCMD_AddWrite(GPUREG_DEPTHMAP_OFFSET, 0);
    GPUCMD_AddWrite(GPUREG_STENCIL_TEST, 0);
    GPUCMD_AddWrite(GPUREG_FRAGOP_ALPHA_TEST, 0);
    GPUCMD_AddWrite(GPUREG_LOGIC_OP, 3);                 // COPY operation
    GPUCMD_AddWrite(GPUREG_COLOR_OPERATION, 0x00E40000); // deafult, Logic op

    // Texturing
    GPUCMD_AddWrite(GPUREG_TEXUNIT0_TYPE, GPU_RGB565);
    // TEXUNIT0_DIM packs width/height into a 32-bit register: low 16 bits
    // height, high 16 bits width.
    GPUCMD_AddWrite(GPUREG_TEXUNIT0_DIM,
                    MOON_CTR_VIDEO_TEX_H | (MOON_CTR_VIDEO_TEX_W << 16));
    // Texture address similarly uses 8-byte units for the GPU address.
    GPUCMD_AddWrite(GPUREG_TEXUNIT0_ADDR1, osConvertVirtToPhys(vramTex) >> 3);
    GPUCMD_AddWrite(GPUREG_TEXUNIT0_PARAM, GPU_NEAREST | (GPU_LINEAR << 1));

    GPUCMD_AddWrite(GPUREG_TEXUNIT_CONFIG, 1 | (1 << 12) | (1 << 16));

    // There are multiple TEXENV registers spaced sequentially; the loop
    // programs six of them. Register spacing is 4 bytes between env registers.
    for (int i = 0; i < 6; i++) {
        GPUCMD_AddWrite(GPUREG_TEXENV0_SOURCE + (i * 4), 0x003003);
        GPUCMD_AddWrite(GPUREG_TEXENV0_OPERAND + (i * 4), 0);
        GPUCMD_AddWrite(GPUREG_TEXENV0_COMBINER + (i * 4), 0);
        GPUCMD_AddWrite(GPUREG_TEXENV0_SCALE + (i * 4), 0);
    }

    // Attribute buffers
    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_LOC, 0);
    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_FORMAT_LOW, 0);
    // ATTRIB FORMAT HIGH: (0xFFF << 16) => attribute size/format mask,
    // (1<<28) sets the number of fixed attributes (here used for two
    // fixed vertex attributes required by the shader).
    GPUCMD_AddWrite(GPUREG_ATTRIBBUFFERS_FORMAT_HIGH,
                    (0xFFF << 16) | (1 << 28));

    // Vertex Shader
    static DVLB_s *vshader_dvlb = NULL;
    static shaderProgram_s program;

    if (!vshader_dvlb) {
        vshader_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
        shaderProgramInit(&program);
        shaderProgramSetVsh(&program, &vshader_dvlb->DVLE[0]);
    }

    shaderProgramUse(&program);

    GPUCMD_AddWrite(GPUREG_VSH_NUM_ATTR, 1);
    GPUCMD_AddMaskedWrite(GPUREG_VSH_INPUTBUFFER_CONFIG, 0xB, 1 | (0xA0 << 24));
    GPUCMD_AddWrite(GPUREG_VSH_ATTRIBUTES_PERMUTATION_LOW, 0x00000010);
    GPUCMD_AddWrite(GPUREG_VSH_ATTRIBUTES_PERMUTATION_HIGH, 0);

    // Geometry Pipeline
    GPUCMD_AddWrite(GPUREG_FACECULLING_CONFIG, 0);
    GPUCMD_AddWrite(GPUREG_GEOSTAGE_CONFIG, 0);
    GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 2, (1 << 8) | 1);
    // INDEXBUFFER_CONFIG = 0x80000000 disables indexed drawing (special
    // sentinel used by this pipeline configuration).
    GPUCMD_AddWrite(GPUREG_INDEXBUFFER_CONFIG, 0x80000000);
    GPUCMD_AddWrite(GPUREG_RESTART_PRIMITIVE, 1);

    // Vertex Data
    GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 1, 1);
    GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 0);
    // FIXEDATTRIB_INDEX = 0xF selects the fixed attribute indices the
    // shader will read; 0xF matches the attribute layout we upload below.
    GPUCMD_AddWrite(GPUREG_FIXEDATTRIB_INDEX, 0xF);

    // Vertex attributes will be added by upload_vertex_attributes_and_draw()
    upload_vertex_attributes_and_draw();

    // End Geometry Pipeline
    GPUCMD_AddMaskedWrite(GPUREG_START_DRAW_FUNC0, 1, 1);
    GPUCMD_AddMaskedWrite(GPUREG_GEOSTAGE_CONFIG2, 1, 0);
    GPUCMD_AddWrite(GPUREG_VTX_FUNC, 1);

    // Stop Command List
    GPUCMD_AddMaskedWrite(GPUREG_PRIMITIVE_CONFIG, 0x8, 0x00000000);
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_FLUSH, 1);
    GPUCMD_AddWrite(GPUREG_FRAMEBUFFER_INVALIDATE, 1);
}

void N3dsRendererBase::upload_vertex_attributes_and_draw() {
    union {
        u32 packed[3];
        struct {
            u8 x[3], y[3], z[3], w[3];
        };
    } param;

/*
 * ATTR packs four 24-bit fixed-point values into the GPU attribute stream.
 * The GPU expects attributes in a particular packed order; the swap of
 * packed[0] and packed[2] corrects the ordering so the incremental writes
 * land in the hardware registers in the expected sequence.
 */
#define ATTR(X, Y, Z, W)                                                       \
    {                                                                          \
        write24(param.x, f32tof24(X));                                         \
        write24(param.y, f32tof24(Y));                                         \
        write24(param.z, f32tof24(Z));                                         \
        write24(param.w, f32tof24(W));                                         \
                                                                               \
        u32 p = param.packed[0];                                               \
        param.packed[0] = param.packed[2];                                     \
        param.packed[2] = p;                                                   \
        GPUCMD_AddIncrementalWrites(GPUREG_FIXEDATTRIB_DATA0, param.packed,    \
                                    3);                                        \
    }

    // Texture coordinate scale factors (source dimensions / texture size).
    float sw = image_width / ((float)MOON_CTR_VIDEO_TEX_W);
    float sh = image_height / ((float)MOON_CTR_VIDEO_TEX_H);
    // `hh` maps pixel-space offsets into normalized device coordinates
    // used by the vertex shader (2.0 / width maps to [-1,1] range).
    float hh = 2.0f / surface_width;

    ATTR(1.0, -1.0, 0.0, 0.0); // TR
    ATTR(sw, -hh, 0.0, 0.0);

    ATTR(-1.0, -1.0, 0.0, 0.0); // TL
    ATTR(sw, sh, 0.0, 0.0);

    ATTR(1.0, 1.0, 0.0, 0.0); // BR
    ATTR(0.0, -hh, 0.0, 0.0);

    ATTR(-1.0, 1.0, 0.0, 0.0); // BL
    ATTR(0.0, sh, 0.0, 0.0);

#undef ATTR
}

void N3dsRendererBase::process_cmdlist_and_wait() {
    gspWaitForEvent(GSPGPU_EVENT_PPF, 0);

    u32 *unused;
    u32 cmdlist_len;
    GPUCMD_Split(&unused, &cmdlist_len);
    GSPGPU_FlushDataCache(cmdlist, cmdlist_len);

    extern u32 __ctru_linear_heap;
    extern u32 __ctru_linear_heap_size;
    GX_FlushCacheRegions(cmdlist, cmdlist_len * 4, (u32 *)__ctru_linear_heap,
                         __ctru_linear_heap_size, NULL, 0);

    // ProcessCommandList expects a byte size (cmdlist_len is a word count),
    // so multiply by 4 to convert to bytes.
    GX_ProcessCommandList(cmdlist, cmdlist_len * 4, 2);

    gspWaitForEvent(GSPGPU_EVENT_P3D, 0);
}

void N3dsRendererBase::copy_vram_to_framebuffer_to_screen(
    uint8_t *__restrict source) {
    // Copy into framebuffer, untiled
    if ((screen == GFX_TOP) && gfxIs3D()) {
        // Left
        u32 *dest_left =
            (u32 *)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        auto surface_width_3d = surface_width / 2;
        GX_DisplayTransfer(
            (u32 *)vramFb, GX_BUFFER_DIM(surface_height, surface_width_3d),
            dest_left, GX_BUFFER_DIM(surface_height, surface_width_3d),
            GX_TRANSFER_OUT_TILED(0) |
                GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
                GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
                GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

        // Right
        u32 *dest_right =
            (u32 *)gfxGetFramebuffer(GFX_TOP, GFX_RIGHT, NULL, NULL);
        // Compute the u32-word offset for the right-eye framebuffer.
        // - surface_height * surface_width * px_size: total bytes for a
        //   single-screen buffer.
        // - dividing by (sizeof(u32) * 2) converts bytes -> u32 words and
        //   accounts for the left/right interleaving used by the top-screen
        //   framebuffer layout (hence the extra division by 2).
        auto surface_offset_3d =
            surface_height * surface_width * px_size / (sizeof(u32) * 2);
        GX_DisplayTransfer((u32 *)vramFb + surface_offset_3d,
                           GX_BUFFER_DIM(surface_height, surface_width_3d),
                           dest_right,
                           GX_BUFFER_DIM(surface_height, surface_width_3d),
                           GX_TRANSFER_OUT_TILED(0) |
                               GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
                               GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
                               GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    } else {
        u32 *dest = (u32 *)gfxGetFramebuffer(screen, GFX_LEFT, NULL, NULL);
        GX_DisplayTransfer((u32 *)vramFb,
                           GX_BUFFER_DIM(surface_height, surface_width), dest,
                           GX_BUFFER_DIM(surface_height, surface_width),
                           GX_TRANSFER_OUT_TILED(0) |
                               GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB565) |
                               GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB565) |
                               GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    }
    gspWaitForEvent(GSPGPU_EVENT_PPF, 0);
}

void N3dsRendererBase::finalize_frame_and_swap(u64 start_ticks) {
    perf_fbcopy_ticks = svcGetSystemTick() - start_ticks;
    if (debug) {
        draw_perf_counters();
    }

    gfxScreenSwapBuffers(screen, true);
}
