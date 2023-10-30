// A simple software font renderer using freetype + gx2, to replace OSScreen
#include "font.h"

#include <stdarg.h>
#include <string.h>

#include <coreinit/memory.h>
#include <whb/gfx.h>
#include <gx2/draw.h>
#include <gx2/registers.h>
#include <gx2/utils.h>
#include <gx2r/surface.h>
#include <gx2r/draw.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include "shaders/font_texture.h"

static WHBGfxShaderGroup fontShader = {};
static GX2Texture fontTexture = {};
static GX2Sampler fontSampler = {};
static FT_Library ft_lib = NULL;
static FT_Face ft_face   = NULL;

static uint8_t font_r = 255, font_g = 255, font_b = 255, font_a = 255;

static const float font_vertex_buffer[] __attribute__ ((aligned (GX2_VERTEX_BUFFER_ALIGNMENT))) = {
   -1.0f, -1.0f,  0.0f, 1.0f,
    1.0f, -1.0f,  1.0f, 1.0f,
    1.0f,  1.0f,  1.0f, 0.0f,
   -1.0f,  1.0f,  0.0f, 0.0f,
};

void Font_Init(void)
{
    // Load the system font
    void *font = NULL;
    uint32_t size = 0;
    OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &font, &size);

    if (font && size) {
        FT_Init_FreeType(&ft_lib);
        FT_New_Memory_Face(ft_lib, (FT_Byte *) font, size, 0, &ft_face);
    }

    // Load the shader
    WHBGfxLoadGFDShaderGroup(&fontShader, 0, font_texture_gsh);

    // Initialize the shader attributes
    WHBGfxInitShaderAttribute(&fontShader, "in_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&fontShader, "in_texCoord", 0, 8, GX2_ATTRIB_FORMAT_FLOAT_32_32);
    
    // Initialize the fetch shader
    WHBGfxInitFetchShader(&fontShader);

    // Create a texture to draw the text to
    fontTexture.surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    fontTexture.surface.use = GX2_SURFACE_USE_TEXTURE;
    fontTexture.surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    fontTexture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    fontTexture.surface.depth = 1;
    fontTexture.surface.width = FONT_BUFFER_WIDTH;
    fontTexture.surface.height = FONT_BUFFER_HEIGHT;
    fontTexture.surface.mipLevels = 1;
    fontTexture.viewNumSlices = 1;
    fontTexture.viewNumMips = 1;
    fontTexture.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);

    GX2RCreateSurface(&fontTexture.surface, GX2R_RESOURCE_BIND_TEXTURE | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
    GX2InitTextureRegs(&fontTexture);

    // Create a sampler
    GX2InitSampler(&fontSampler, GX2_TEX_CLAMP_MODE_WRAP, GX2_TEX_MIP_FILTER_MODE_POINT);

    // Clear the texture buffer
    Font_Clear();
}

void Font_Deinit(void)
{
    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_lib);

    WHBGfxFreeShaderGroup(&fontShader);
    GX2RDestroySurfaceEx(&fontTexture.surface, GX2R_RESOURCE_BIND_NONE);
}

void Font_Draw(void)
{
    GX2SetFetchShader(&fontShader.fetchShader);
    GX2SetVertexShader(fontShader.vertexShader);
    GX2SetPixelShader(fontShader.pixelShader);

    GX2SetPixelTexture(&fontTexture, 0);
    GX2SetPixelSampler(&fontSampler, 0);

    GX2SetAttribBuffer(0, sizeof(font_vertex_buffer), 4 * sizeof(float), font_vertex_buffer);
    GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
}

void Font_Draw_TVDRC(void)
{
    WHBGfxBeginRender();

    WHBGfxBeginRenderTV();
    WHBGfxClearColor(GX2_CLEAR_COLOR);
    Font_Draw();
    WHBGfxFinishRenderTV();

    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(GX2_CLEAR_COLOR);
    Font_Draw();
    WHBGfxFinishRenderDRC();

    WHBGfxFinishRender();
}

void Font_Clear(void)
{
    uint8_t* pixels = (uint8_t*) GX2RLockSurfaceEx(&fontTexture.surface, 0, GX2R_RESOURCE_BIND_NONE);
    memset(pixels, 0, fontTexture.surface.imageSize);
    GX2RUnlockSurfaceEx(&fontTexture.surface, 0, GX2R_RESOURCE_BIND_NONE);
}

static void draw_freetype_bitmap(FT_Bitmap *bitmap, FT_Int x, FT_Int y, uint8_t* dst_buffer) {
    FT_Int i, j, p, q;
    FT_Int x_max = x + bitmap->width;
    FT_Int y_max = y + bitmap->rows;

    for (i = x, p = 0; i < x_max; i++, p++) {
        for (j = y, q = 0; j < y_max; j++, q++) {
            if (i < 0 || j < 0 || i >= FONT_BUFFER_WIDTH || j >= FONT_BUFFER_HEIGHT) {
                continue;
            }

            float opacity = bitmap->buffer[q * bitmap->pitch + p] / 255.0f;
            uint32_t offset = (i + j * fontTexture.surface.pitch) * 4;
            dst_buffer[offset    ] = font_r;
            dst_buffer[offset + 1] = font_g;
            dst_buffer[offset + 2] = font_b;
            dst_buffer[offset + 3] = font_a * opacity;
        }
    }
}

void Font_Printw(uint32_t x, uint32_t y, const wchar_t* string)
{
    FT_GlyphSlot slot = ft_face->glyph;
    FT_Vector pen = {(int) x, (int) y};
    uint8_t* pixels = (uint8_t*) GX2RLockSurfaceEx(&fontTexture.surface, 0, GX2R_RESOURCE_BIND_NONE);

    for (; *string; string++) {
        uint32_t charcode = *string;

        if (charcode == '\n') {
            pen.y += ft_face->size->metrics.height >> 6;
            pen.x = x;
            continue;
        }

        FT_Load_Glyph(ft_face, FT_Get_Char_Index(ft_face, charcode), FT_LOAD_DEFAULT);
        FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL);

        draw_freetype_bitmap(&slot->bitmap, pen.x + slot->bitmap_left, pen.y - slot->bitmap_top, pixels);
        pen.x += slot->advance.x >> 6;
    }

    GX2RUnlockSurfaceEx(&fontTexture.surface, 0, GX2R_RESOURCE_BIND_NONE);
}

void Font_Print(uint32_t x, uint32_t y, const char* string)
{
    wchar_t *buffer = calloc(strlen(string) + 1, sizeof(wchar_t));

    size_t num = mbstowcs(buffer, string, strlen(string));
    if (num > 0) {
        buffer[num] = 0;
    } 
    else {
        wchar_t* tmp = buffer;
        while ((*tmp++ = *string++));
    }

    Font_Printw(x, y, buffer);
    free(buffer);
}

void Font_Printf(uint32_t x, uint32_t y, const char* msg, ...)
{
    va_list args;
    va_start(args, msg);

    char* tmp = NULL;
    if((vasprintf(&tmp, msg, args) >= 0) && tmp) {
        Font_Print(x, y, tmp);
    }

    va_end(args);
    free(tmp);
}

void Font_SetColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    font_r = r;
    font_g = g;
    font_b = b;
    font_a = a;
}

void Font_SetSize(uint32_t size)
{
    FT_Set_Pixel_Sizes(ft_face, 0, size);
}
