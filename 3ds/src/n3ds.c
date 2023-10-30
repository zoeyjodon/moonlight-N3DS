#include "n3ds.h"

#include <stdlib.h>
#include <stdio.h>
#include <malloc.h>
#include <stdarg.h>

#include <3ds.h>
#include <citro3d.h>

uint32_t currentFrame;
uint32_t nextFrame;
C3D_RenderTarget util_draw_screen_top;
C3D_RenderTarget util_draw_screen_top_3d;
C3D_RenderTarget util_draw_screen_bottom;

static LightLock queueMutex;
static yuv_texture_t* queueMessages[MAX_QUEUEMESSAGES];
static uint32_t queueWriteIndex;
static uint32_t queueReadIndex;

void n3ds_stream_init(uint32_t width, uint32_t height)
{
  currentFrame = nextFrame = 0;

  LightLock_Init(&queueMutex);
  queueReadIndex = queueWriteIndex = 0;

	gfxInitDefault();
	gfxSet3D(false);
	gfxSetWide(true);

	if(!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE * 1.5))
	{
    printf("[Error] C3D_Init() failed.\n");
    C2D_Fini();
    C3D_Fini();
	}

	if(!C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 1.5))
	{
		printf("[Error] C2D_Init() failed.\n");
    C2D_Fini();
    C3D_Fini();
	}

	C2D_Prepare();
	util_draw_screen_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	util_draw_screen_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
	util_draw_screen_top_3d = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
	if(!util_draw_screen_top || !util_draw_screen_bottom
	|| !util_draw_screen_top_3d)
	{
		printf("[Error] C2D_CreateScreenTarget() failed.\n");
    C2D_Fini();
    C3D_Fini();
	}

	C2D_TargetClear(util_draw_screen_top, C2D_Color32f(0, 0, 0, 0));
	C2D_TargetClear(util_draw_screen_bottom, C2D_Color32f(0, 0, 0, 0));
	C2D_TargetClear(util_draw_screen_top_3d, C2D_Color32f(0, 0, 0, 0));
}

void n3ds_stream_draw(void)
{
  yuv_texture_t* tex = get_frame();
  if (tex) {
    if (++currentFrame <= nextFrame - NUM_BUFFERS) {
      // display thread is behind decoder, skip frame
    }
    else {
      WHBGfxBeginRender();

      // TV
      WHBGfxBeginRenderTV();
      WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);

      GX2SetPixelTexture(&tex->yTex, 0);
      GX2SetPixelTexture(&tex->uvTex, 1);
      GX2SetPixelSampler(&screenSamp, 0);
      GX2SetPixelSampler(&screenSamp, 1);

      GX2SetFetchShader(&shaderGroup.fetchShader);
      GX2SetVertexShader(shaderGroup.vertexShader);
      GX2SetPixelShader(shaderGroup.pixelShader);

      GX2SetVertexUniformReg(0, 2, tvScreenSize);
      GX2SetAttribBuffer(0, ATTRIB_SIZE, ATTRIB_STRIDE, tvAttribs);
      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

      WHBGfxFinishRenderTV();

      // DRC
      WHBGfxBeginRenderDRC();
      WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);

      GX2SetPixelTexture(&tex->yTex, 0);
      GX2SetPixelTexture(&tex->uvTex, 1);
      GX2SetPixelSampler(&screenSamp, 0);
      GX2SetPixelSampler(&screenSamp, 1);

      GX2SetFetchShader(&shaderGroup.fetchShader);
      GX2SetVertexShader(shaderGroup.vertexShader);
      GX2SetPixelShader(shaderGroup.pixelShader);

      GX2SetVertexUniformReg(0, 2, drcScreenSize);
      GX2SetAttribBuffer(0, ATTRIB_SIZE, ATTRIB_STRIDE, drcAttribs);
      GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

      WHBGfxFinishRenderDRC();

      WHBGfxFinishRender();
    }
  }
}

void n3ds_stream_fini(void)
{
  free(tvAttribs);
  free(drcAttribs);

  WHBGfxFreeShaderGroup(&shaderGroup);
}

void* get_frame(void)
{
  LightLock_Lock(&queueMutex);

  uint32_t elements_in = queueWriteIndex - queueReadIndex;
  if(elements_in == 0) {
    LightLock_Unlock(&queueMutex);
    return NULL; // framequeue is empty
  }

  uint32_t i = (queueReadIndex)++ & (MAX_QUEUEMESSAGES - 1);
  yuv_texture_t* message = queueMessages[i];

  LightLock_Unlock(&queueMutex);
  return message;
}

void add_frame(yuv_texture_t* msg)
{
  LightLock_Lock(&queueMutex);

  uint32_t elements_in = queueWriteIndex - queueReadIndex;
  if (elements_in == MAX_QUEUEMESSAGES) {
    LightLock_Unlock(&queueMutex);
    return; // framequeue is full
  }

  uint32_t i = (queueWriteIndex)++ & (MAX_QUEUEMESSAGES - 1);
  queueMessages[i] = msg;

  LightLock_Unlock(&queueMutex);
}

void n3ds_setup_renderstate(void)
{
  WHBGfxBeginRenderTV();
  GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
  GX2SetBlendControl(GX2_RENDER_TARGET_0,
    GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD,
    TRUE,
    GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD
  );
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
  WHBGfxBeginRenderDRC();
  GX2SetColorControl(GX2_LOGIC_OP_COPY, 0xFF, FALSE, TRUE);
  GX2SetBlendControl(GX2_RENDER_TARGET_0,
    GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD,
    TRUE,
    GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_INV_SRC_ALPHA,
    GX2_BLEND_COMBINE_MODE_ADD
  );
  GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
}
