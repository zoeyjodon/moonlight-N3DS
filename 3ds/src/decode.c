#include "3ds.h"

#include <sps.h>

#include <wut.h>
#include <h264/decode.h>
#include <gx2/utils.h>
#include <gx2/mem.h>

#include <unistd.h>
#include <stdbool.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

// memory requirement for the maximum supported level (level 42)
#define H264_MEM_REQUIREMENT (0x2200000 + 0x3ff + 0x480000)
#define H264_MEM_ALIGNMENT 0x400
#define H264_FRAME_SIZE(w, h) (((w) * (h) * 3) >> 1)
#define H264_FRAME_PITCH(w) (((w) + 0xff) & ~0xff)
#define H264_FRAME_HEIGHT(h) (((h) + 0xf) & ~0xf)
#define H264_MAX_FRAME_SIZE H264_FRAME_SIZE(H264_FRAME_PITCH(2800), 1408)

#define DECODER_BUFFER_SIZE 92*1024

yuv_texture_t textures[NUM_BUFFERS];
uint32_t currentTexture;

static void* decoder;
static char* decodebuffer;

static void frame_callback(H264DecodeOutput *output) { }

static void createYUVTextures(GX2Texture* yPlane, GX2Texture* uvPlane, uint32_t width, uint32_t height)
{
  memset(yPlane, 0, sizeof(GX2Texture));
  memset(uvPlane, 0, sizeof(GX2Texture));

  // make sure the height is aligned properly for the output
  height = H264_FRAME_HEIGHT(height);

  // create Y texture
  yPlane->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
  yPlane->surface.use = GX2_SURFACE_USE_TEXTURE;
  yPlane->surface.format = GX2_SURFACE_FORMAT_UNORM_R8;
  yPlane->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
  yPlane->surface.depth = 1;
  yPlane->surface.width = width;
  yPlane->surface.height = height;
  yPlane->surface.mipLevels = 1;
  yPlane->viewNumSlices = 1;
  yPlane->viewNumMips = 1;
  yPlane->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
  GX2CalcSurfaceSizeAndAlignment(&yPlane->surface);
  GX2InitTextureRegs(yPlane);

  // create UV texture
  uvPlane->surface.dim = GX2_SURFACE_DIM_TEXTURE_2D;
  uvPlane->surface.use = GX2_SURFACE_USE_TEXTURE;
  uvPlane->surface.format = GX2_SURFACE_FORMAT_UNORM_R8_G8;
  uvPlane->surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
  uvPlane->surface.depth = 1;
  uvPlane->surface.width = width / 2;
  uvPlane->surface.height = height / 2;
  uvPlane->surface.mipLevels = 1;
  uvPlane->viewNumSlices = 1;
  uvPlane->viewNumMips = 1;
  uvPlane->compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
  GX2CalcSurfaceSizeAndAlignment(&uvPlane->surface);
  GX2InitTextureRegs(uvPlane);

  yPlane->surface.image = memalign(H264_MEM_ALIGNMENT, H264_FRAME_SIZE(H264_FRAME_PITCH(width), height));
  GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, yPlane->surface.image, H264_FRAME_SIZE(H264_FRAME_PITCH(width), height));

  uvPlane->surface.image = yPlane->surface.image + yPlane->surface.imageSize;
}

static int n3ds_decoder_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {
  if (videoFormat != VIDEO_FORMAT_H264) {
    printf("Invalid video format\n");
    return -1;
  }

  decoder = memalign(H264_MEM_ALIGNMENT, H264_MEM_REQUIREMENT);
  if (decoder == NULL) {
    fprintf(stderr, "Not enough memory\n");
    return -1;
  }

  int res = H264DECCheckMemSegmentation(decoder, H264_MEM_REQUIREMENT);
  if (res != 0) {
    printf("h264_3ds: Invalid memory segmentation 0x%07X\n", res);
    return -1;
  }

  res = H264DECInitParam(H264_MEM_REQUIREMENT, decoder);
  if (res != 0) {
    printf("h264_3ds: Error initializing decoder 0x%07X\n", res);
    return -1;
  }

  res = H264DECSetParam_FPTR_OUTPUT(decoder, frame_callback);
  if (res != 0) {
    printf("h264_3ds: Error setting callback 0x%07X\n", res);
    return -1;
  }

  res = H264DECSetParam_OUTPUT_PER_FRAME(decoder, 1);
  if (res != 0) {
    printf("h264_3ds: Error setting OUTPUT_PER_FRAME 0x%07X\n", res);
    return -1;
  }

  res = H264DECOpen(decoder);
  if (res != 0) {
    printf("h264_3ds: Error opening decoder 0x%07X\n", res);
    return -1;
  }

  res = H264DECBegin(decoder);
  if (res != 0) {
    printf("h264_3ds: Error preparing decoder 0x%07X\n", res);
    return -1;
  }

  for (int i = 0; i < NUM_BUFFERS; i++) {
    createYUVTextures(&textures[i].yTex, &textures[i].uvTex, width, height);

    res = H264DECCheckMemSegmentation(textures[i].yTex.surface.image, H264_FRAME_SIZE(H264_FRAME_PITCH(width), height));
    if (res != 0) {
      printf("h264_3ds: Invalid texture memory segmentation 0x%07X\n", res);
      return -1;
    }
  }
  currentTexture = 0;

  decodebuffer = memalign(H264_MEM_ALIGNMENT, DECODER_BUFFER_SIZE + 64);
  if (decodebuffer == NULL) {
    fprintf(stderr, "Not enough memory\n");
    return -1;
  }

  return 0;
}

static void n3ds_decoder_cleanup() {
  H264DECFlush(decoder);
  H264DECEnd(decoder);
  H264DECClose(decoder);

  free(decoder);
  decoder = NULL;
  free(decodebuffer);
  decodebuffer = NULL;

  for (int i = 0; i < NUM_BUFFERS; i++) {
    free(textures[i].yTex.surface.image);
    textures[i].yTex.surface.image = NULL;
  }
}

static int n3ds_decoder_submit_decode_unit(PDECODE_UNIT decodeUnit) {
  if (decodeUnit->fullLength > DECODER_BUFFER_SIZE) {
    fprintf(stderr, "Video decode buffer too small\n");
    return DR_OK;
  }

  PLENTRY entry = decodeUnit->bufferList;
  int length = 0;
  while (entry != NULL) {
    memcpy(decodebuffer+length, entry->data, entry->length);
    length += entry->length;
    entry = entry->next;
  }

  int res = H264DECSetBitstream(decoder, decodebuffer, length, 0);
  if (res != 0) {
    printf("h264_3ds: Error setting bitstream 0x%07X\n", res);
    return DR_NEED_IDR;
  }

  yuv_texture_t* tex = &textures[currentTexture];

  res = H264DECExecute(decoder, tex->yTex.surface.image);
  if ((res & ~0xff) != 0) {
    printf("h264_3ds: Error decoding frame 0x%07X\n", res);
    return DR_NEED_IDR;
  }

  nextFrame++;

  add_frame(tex);

  currentTexture++;

  if (currentTexture >= NUM_BUFFERS) {
    currentTexture = 0;
  }

  return DR_OK;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_n3ds = {
  .setup = n3ds_decoder_setup,
  .cleanup = n3ds_decoder_cleanup,
  .submitDecodeUnit = n3ds_decoder_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT,
};
