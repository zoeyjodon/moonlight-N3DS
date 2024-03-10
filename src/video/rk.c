/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2015-2018 Iwan Timmer
 * Copyright (C) 2018 Martin Cerveny, Daniel Mehrwald
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

#ifndef __3DS__

#include "video.h"
#include "../util.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <libdrm/drm_fourcc.h>
#include <linux/videodev2.h>

#include <rockchip/rk_mpi.h>

#define MAX_FRAMES 16
#define RK_H264 0x7
#define RK_H265 0x1000004
#define RK_AV1  0x1000008

// Vendor-defined 10-bit format code used prior to 5.10
#ifndef DRM_FORMAT_NA12
#define DRM_FORMAT_NA12 fourcc_code('N', 'A', '1', '2')
#endif

// Upstreamed 10-bit format code used on 5.10+ kernels
#ifndef DRM_FORMAT_NV15
#define DRM_FORMAT_NV15 fourcc_code('N', 'V', '1', '5')
#endif

// Values for "Colorspace" connector property
#ifndef DRM_MODE_COLORIMETRY_DEFAULT
#define DRM_MODE_COLORIMETRY_DEFAULT     0
#endif
#ifndef DRM_MODE_COLORIMETRY_BT2020_RGB
#define DRM_MODE_COLORIMETRY_BT2020_RGB  9
#endif

// HDR structs copied from linux include/linux/hdmi.h for older libdrm versions
struct rk_hdr_metadata_infoframe
{
    uint8_t eotf;
    uint8_t metadata_type;

    struct
    {
        uint16_t x, y;
    } display_primaries[3];

    struct
    {
        uint16_t x, y;
    } white_point;

    uint16_t max_display_mastering_luminance;
    uint16_t min_display_mastering_luminance;

    uint16_t max_cll;
    uint16_t max_fall;
};

struct rk_hdr_output_metadata
{
    uint32_t metadata_type;

    union {
        struct rk_hdr_metadata_infoframe hdmi_metadata_type1;
    };
};

void *pkt_buf = NULL;
size_t pkt_buf_size = 0;
int fd;
int fb_id;
uint32_t plane_id, crtc_id, conn_id, hdr_metadata_blob_id, pixel_format;
int frm_eos;
int crtc_width;
int crtc_height;
RK_U32 frm_width;
RK_U32 frm_height;
int fb_x, fb_y, fb_width, fb_height;
bool atomic;

uint8_t last_colorspace = 0xFF;
bool last_hdr_state = false;

pthread_t tid_frame, tid_display;
pthread_mutex_t mutex;
pthread_cond_t cond;

drmModePlane *ovr = NULL;
drmModeEncoder *encoder = NULL;
drmModeConnector *connector = NULL;
drmModeRes *resources = NULL;
drmModePlaneRes *plane_resources = NULL;
drmModeCrtcPtr crtc = {0};

drmModePropertyPtr hdr_metadata_prop = NULL;

drmModeAtomicReqPtr drm_request = NULL;
drmModePropertyPtr plane_props[32];
drmModePropertyPtr conn_props[32];

MppCtx mpi_ctx;
MppApi *mpi_api;
MppPacket mpi_packet;
MppBufferGroup mpi_frm_grp;

struct {
  int prime_fd;
  int fb_id;
  uint32_t handle;
} frame_to_drm[MAX_FRAMES];

int set_property(uint32_t id, uint32_t type, drmModePropertyPtr *props, char *name, uint64_t value) {
  while (*props) {
    if (!strcasecmp(name, (*props)->name)) {
      if (atomic)
        return drmModeAtomicAddProperty(drm_request, id, (*props)->prop_id, value);
      else
        return drmModeObjectSetProperty(fd, id, type, (*props)->prop_id, value);
    }
    props++;
  }

  fprintf(stderr, "Property '%s' not found\n", name);
  return -EINVAL;
}

void *display_thread(void *param) {
  int ret;
  while (!frm_eos) {
    int _fb_id;

    pthread_mutex_lock(&mutex);
    while (fb_id == 0) {
      pthread_cond_wait(&cond, &mutex);
      if (fb_id == 0 && frm_eos) {
        pthread_mutex_unlock(&mutex);
        return NULL;
      }
    }
    _fb_id = fb_id;

    fb_id = 0;
    pthread_mutex_unlock(&mutex);

    if (atomic) {
      // We may need to modeset to apply colorspace changes when toggling HDR
      set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "FB_ID", _fb_id);
      ret = drmModeAtomicCommit(fd, drm_request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
      if (ret) {
        perror("drmModeAtomicCommit");
      }
    } else {
      ret = drmModeSetPlane(fd, plane_id, crtc_id, _fb_id, 0,
                            fb_x, fb_y, fb_width, fb_height,
                            0, 0, frm_width << 16, frm_height << 16);
      if (ret) {
        perror("drmModeSetPlane");
      }
    }
  }

  return NULL;
}

void *frame_thread(void *param) {

  int count = 0;
  int ret;
  int i;
  MppFrame frame = NULL;

  while (!frm_eos) {

    ret = mpi_api->decode_get_frame(mpi_ctx, &frame);
    if (ret != MPP_OK && ret != MPP_ERR_TIMEOUT) {
      if (count < 3) {
         fprintf(stderr, "Waiting for Frame (return code = %d, retry count = %d)\n", ret, count);
         usleep(10000);
         count++;
         continue;
      }
    }
    if (frame) {
      if (mpp_frame_get_info_change(frame)) {
        // new resolution
        assert(!mpi_frm_grp);

        frm_width = mpp_frame_get_width(frame);
        frm_height = mpp_frame_get_height(frame);
        RK_U32 hor_stride = mpp_frame_get_hor_stride(frame);
        RK_U32 ver_stride = mpp_frame_get_ver_stride(frame);
        MppFrameFormat fmt = mpp_frame_get_fmt(frame);
        assert((fmt == MPP_FMT_YUV420SP) || (fmt == MPP_FMT_YUV420SP_10BIT));

        // position overlay, scale to ratio
        float crt_ratio = (float)crtc_width/crtc_height;
        float frame_ratio = (float)frm_width/frm_height;

        if (crt_ratio>frame_ratio) {
          fb_width = frame_ratio/crt_ratio*crtc_width;
          fb_height = crtc_height;
          fb_x = (crtc_width-fb_width)/2;
          fb_y = 0;
        } else {
          fb_width = crtc_width;
          fb_height = crt_ratio/frame_ratio*crtc_height;
          fb_x = 0;
          fb_y = (crtc_height-fb_height)/2;
        }

        // create new external frame group and allocate (commit flow) new DRM buffers and DRM FB
        assert(!mpi_frm_grp);
        ret = mpp_buffer_group_get_external(&mpi_frm_grp, MPP_BUFFER_TYPE_DRM);
        assert(!ret);
        for (i = 0; i < MAX_FRAMES; i++) {

          // new DRM buffer
          struct drm_mode_create_dumb dmcd = {0};
          dmcd.bpp = 8; // hor_stride is already adjusted for 10 vs 8 bit
          dmcd.width = hor_stride;
          dmcd.height = ver_stride * 2; // documentation say not v*2/3 but v*2 (additional info included)
          ret = drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &dmcd);
          if (ret) {
            perror("drmIoctl(DRM_IOCTL_MODE_CREATE_DUMB)");
            exit(EXIT_FAILURE);
          }
          assert(dmcd.pitch == dmcd.width);
          assert(dmcd.size == dmcd.pitch * dmcd.height);
          frame_to_drm[i].handle = dmcd.handle;

          // commit DRM buffer to frame group
          struct drm_prime_handle dph = {0};
          dph.handle = dmcd.handle;
          dph.fd = -1;
          ret = drmIoctl(fd, DRM_IOCTL_PRIME_HANDLE_TO_FD, &dph);
          if (ret) {
            perror("drmIoctl(DRM_IOCTL_PRIME_HANDLE_TO_FD)");
            exit(EXIT_FAILURE);
          }
          MppBufferInfo info = {0};
          info.type = MPP_BUFFER_TYPE_DRM;
          info.size = dmcd.width * dmcd.height;
          info.fd = dph.fd;
          ret = mpp_buffer_commit(mpi_frm_grp, &info);
          assert(!ret);
          frame_to_drm[i].prime_fd = info.fd; // dups fd

          // allocate DRM FB from DRM buffer
          uint32_t handles[4] = {0}, pitches[4] = {0}, offsets[4] = {0};
          handles[0] = frame_to_drm[i].handle;
          offsets[0] = 0;
          pitches[0] = dmcd.pitch;
          handles[1] = frame_to_drm[i].handle;
          offsets[1] = pitches[0] * ver_stride;
          pitches[1] = dmcd.pitch;
          ret = drmModeAddFB2(fd, frm_width, frm_height, pixel_format, handles, pitches, offsets, &frame_to_drm[i].fb_id, 0);
          if (ret) {
            perror("drmModeAddFB2");
            exit(EXIT_FAILURE);
          }
        }
        // register external frame group
        ret = mpi_api->control(mpi_ctx, MPP_DEC_SET_EXT_BUF_GROUP, mpi_frm_grp);
        ret = mpi_api->control(mpi_ctx, MPP_DEC_SET_INFO_CHANGE_READY, NULL);

        // Set atomic properties for the plane prior to the first commit
        if (atomic) {
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "CRTC_ID", crtc_id);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "SRC_X", 0 << 16);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "SRC_Y", 0 << 16);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "SRC_W", frm_width << 16);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "SRC_H", frm_height << 16);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "CRTC_X", fb_x);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "CRTC_Y", fb_y);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "CRTC_W", fb_width);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "CRTC_H", fb_height);
          set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "ZPOS", 0);
        }

        // Set properties on the connector
        set_property(conn_id, DRM_MODE_OBJECT_CONNECTOR, conn_props, "allm_enable", 1); // HDMI ALLM (Game Mode)
        set_property(conn_id, DRM_MODE_OBJECT_CONNECTOR, conn_props, "Colorspace", last_hdr_state ? DRM_MODE_COLORIMETRY_BT2020_RGB : DRM_MODE_COLORIMETRY_DEFAULT);
      } else {
        // regular frame received

        MppBuffer buffer = mpp_frame_get_buffer(frame);
        if (buffer) {
          // find fb_id by frame prime_fd
          MppBufferInfo info;
          ret = mpp_buffer_info_get(buffer, &info);
          assert(!ret);
          for (i = 0; i < MAX_FRAMES; i++) {
            if (frame_to_drm[i].prime_fd == info.fd) break;
          }
          assert(i != MAX_FRAMES);
          // send DRM FB to display thread
          pthread_mutex_lock(&mutex);
          fb_id = frame_to_drm[i].fb_id;
          pthread_cond_signal(&cond);
          pthread_mutex_unlock(&mutex);
        } else {
          fprintf(stderr, "Frame no buff\n");
        }
      }

      frm_eos = mpp_frame_get_eos(frame);
      mpp_frame_deinit(&frame);
      frame = NULL;
    } else {
      if (!frm_eos) {
        fprintf(stderr, "Didn't get frame from MPP (return code = %d)\n", ret);
      }
      break;
    }
  }

  return NULL;
}

int rk_setup(int videoFormat, int width, int height, int redrawRate, void* context, int drFlags) {

  int ret;
  int i;
  int j;
  int format;

  if (videoFormat & VIDEO_FORMAT_MASK_H264) {
    format = RK_H264;
  } else if (videoFormat & VIDEO_FORMAT_MASK_H265) {
    format = RK_H265;
  } else if (videoFormat & VIDEO_FORMAT_MASK_AV1) {
    format = RK_AV1;
  } else {
    fprintf(stderr, "Video format not supported\n");
    return -1;
  }

  // We need atomic plane properties for HDR, but atomic seems to perform quite bad
  // on RK3588 for some reason. The performance of commits seems to go down the longer
  // the stream runs. We'll use the legacy API for non-HDR streams as a workaround.
  atomic = !!(videoFormat & VIDEO_FORMAT_MASK_10BIT);

  MppCodingType mpp_type = (MppCodingType)format;
  ret = mpp_check_support_format(MPP_CTX_DEC, mpp_type);
  if (ret) {
    fprintf(stderr, "Selected video format is not supported\n");
    return -1;
  }

  fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
  if (fd < 0) {
    fprintf(stderr, "Unable to open card0: %d\n", errno);
    return -1;
  }

  resources = drmModeGetResources(fd);
  if (!resources) {
    perror("drmModeGetResources");
    return -1;
  }

  // find active monitor
  for (i = 0; i < resources->count_connectors; ++i) {
    connector = drmModeGetConnector(fd, resources->connectors[i]);
    if (!connector) {
      continue;
    }
    if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
      break;
    }
    drmModeFreeConnector(connector);
  }
  assert(i < resources->count_connectors);

  conn_id = connector->connector_id;

  {
    drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR);
    assert(props->count_props < sizeof(conn_props) / sizeof(conn_props[0]));
    for (j = 0; j < props->count_props; j++) {
      drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[j]);
      if (!prop) {
        continue;
      }
      if (!strcmp(prop->name, "HDR_OUTPUT_METADATA")) {
        hdr_metadata_prop = prop;
      }
      conn_props[j] = prop;
    }
    drmModeFreeObjectProperties(props);
  }

  for (i = 0; i < resources->count_encoders; ++i) {
    encoder = drmModeGetEncoder(fd, resources->encoders[i]);
    if (!encoder) {
      continue;
    }
    if (encoder->encoder_id == connector->encoder_id) {
      break;
    }
    drmModeFreeEncoder(encoder);
  }
  assert(i < resources->count_encoders);

  for (i = 0; i < resources->count_crtcs; ++i) {
    if (resources->crtcs[i] == encoder->crtc_id) {
      crtc = drmModeGetCrtc(fd, resources->crtcs[i]);
      if (!crtc) {
        perror("drmModeGetCrtc");
        continue;
      }
      break;
    }
  }
  assert(i < resources->count_crtcs);
  crtc_id = crtc->crtc_id;
  crtc_width = crtc->width;
  crtc_height = crtc->height;
  uint32_t crtc_bit = (1 << i);

  ret = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
  if (ret) {
    perror("drmSetClientCap(DRM_CLIENT_CAP_UNIVERSAL_PLANES)");
  }
  if (atomic) {
    ret = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (ret) {
      perror("drmSetClientCap(DRM_CLIENT_CAP_ATOMIC)");
      atomic = false;
    }
    else {
      drm_request = drmModeAtomicAlloc();
      assert(drm_request);
    }
  }
  plane_resources = drmModeGetPlaneResources(fd);
  if (!plane_resources) {
    perror("drmModeGetPlaneResources");
    return -1;
  }

  // search for OVERLAY (for active connector, unused, NV12 support)
  for (i = 0; i < plane_resources->count_planes; i++) {
    ovr = drmModeGetPlane(fd, plane_resources->planes[i]);
    if (!ovr) {
      continue;
    }
    for (j = 0; j < ovr->count_formats; j++) {
      if (videoFormat & VIDEO_FORMAT_MASK_10BIT) {
        // 10-bit formats use NA12 (vendor-defined) or NV15 (upstreamed in 5.10+)
        if (ovr->formats[j] == DRM_FORMAT_NA12 || ovr->formats[j] == DRM_FORMAT_NV15) {
          break;
        }
      } else if (ovr->formats[j] == DRM_FORMAT_NV12) {
        // 8-bit formats always use NV12
        break;
      }
    }
    if (j < ovr->count_formats) {
      pixel_format = ovr->formats[j];
    } else {
      continue;
    }
    if ((ovr->possible_crtcs & crtc_bit) && !ovr->crtc_id) {
      drmModeObjectPropertiesPtr props = drmModeObjectGetProperties(fd, plane_resources->planes[i], DRM_MODE_OBJECT_PLANE);
      if (!props) {
        continue;
      }

      assert(props->count_props < sizeof(plane_props) / sizeof(plane_props[0]));
      for (j = 0; j < props->count_props; j++) {
        drmModePropertyPtr prop = drmModeGetProperty(fd, props->props[j]);
        if (!prop) {
          continue;
        }
        plane_props[j] = prop;
        if (!strcmp(prop->name, "type") && (props->prop_values[j] == DRM_PLANE_TYPE_OVERLAY ||
                                            props->prop_values[j] == DRM_PLANE_TYPE_CURSOR ||
                                            props->prop_values[j] == DRM_PLANE_TYPE_PRIMARY)) {
          plane_id = ovr->plane_id;
        }
      }
      if (plane_id) {
        drmModeFreeObjectProperties(props);
        break;
      } else {
        for (j = 0; j < props->count_props; j++) {
          drmModeFreeProperty(plane_props[j]);
          plane_props[j] = NULL;
        }
        drmModeFreeObjectProperties(props);
      }
    }
    drmModeFreePlane(ovr);
  }

  if (!plane_id) {
    fprintf(stderr, "Unable to find suitable plane\n");
    return -1;
  }

  // DRM defines rotation in degrees counter-clockwise while we define
  // rotation in degrees clockwise, so we swap the 90 and 270 cases
  int displayRotation = drFlags & DISPLAY_ROTATE_MASK;
  switch (displayRotation) {
  case DISPLAY_ROTATE_90:
    set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "rotation", DRM_MODE_ROTATE_270);
    break;
  case DISPLAY_ROTATE_180:
    set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "rotation", DRM_MODE_ROTATE_180);
    break;
  case DISPLAY_ROTATE_270:
    set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "rotation", DRM_MODE_ROTATE_90);
    break;
  default:
    set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "rotation", DRM_MODE_ROTATE_0);
    break;
  }

  // hide cursor by move in left lower corner
  drmModeMoveCursor(fd, crtc_id, 0, crtc_height);

  // MPI SETUP

  ensure_buf_size(&pkt_buf, &pkt_buf_size, INITIAL_DECODER_BUFFER_SIZE);
  ret = mpp_packet_init(&mpi_packet, pkt_buf, pkt_buf_size);
  assert(!ret);

  ret = mpp_create(&mpi_ctx, &mpi_api);
  if (ret) {
    fprintf(stderr, "mpp_create() failed: %d\n", ret);
    return -1;
  }

  // decoder split mode (multi-data-input) need to be set before init
  int param = 1;
  ret = mpi_api->control(mpi_ctx, MPP_DEC_SET_PARSER_SPLIT_MODE, &param);
  assert(!ret);

  ret = mpp_init(mpi_ctx, MPP_CTX_DEC, mpp_type);
  if (ret) {
    fprintf(stderr, "mpp_init() failed: %d\n", ret);
    return -1;
  }

  // set blocked read on Frame Thread
  param = MPP_POLL_BLOCK;
  ret = mpi_api->control(mpi_ctx, MPP_SET_OUTPUT_BLOCK, &param);
  assert(!ret);

  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);

  pthread_create(&tid_frame, NULL, frame_thread, NULL);
  pthread_create(&tid_display, NULL, display_thread, NULL);

  return 0;
}

void rk_cleanup() {

  int i;
  int ret;

  frm_eos = 1;
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);

  pthread_join(tid_display, NULL);

  pthread_cond_destroy(&cond);
  pthread_mutex_destroy(&mutex);

  ret = mpi_api->reset(mpi_ctx);
  assert(!ret);

  pthread_join(tid_frame, NULL);

  if (mpi_frm_grp) {
    ret = mpp_buffer_group_put(mpi_frm_grp);
    assert(!ret);
    mpi_frm_grp = NULL;
    for (i = 0; i < MAX_FRAMES; i++) {
      ret = drmModeRmFB(fd, frame_to_drm[i].fb_id);
      if (ret) {
        perror("drmModeRmFB");
      }
      struct drm_mode_destroy_dumb dmdd = {0};
      dmdd.handle = frame_to_drm[i].handle;
      ret = drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dmdd);
      if (ret) {
        perror("drmIoctl(DRM_IOCTL_MODE_DESTROY_DUMB)");
      }
    }
  }

  mpp_packet_deinit(&mpi_packet);
  mpp_destroy(mpi_ctx);
  free(pkt_buf);

  // Undo the connector-wide changes we performed
  if (atomic)
    drmModeAtomicSetCursor(drm_request, 0);
  set_property(conn_id, DRM_MODE_OBJECT_CONNECTOR, conn_props, "HDR_OUTPUT_METADATA", 0);
  set_property(conn_id, DRM_MODE_OBJECT_CONNECTOR, conn_props, "allm_enable", 0);
  set_property(conn_id, DRM_MODE_OBJECT_CONNECTOR, conn_props, "Colorspace", DRM_MODE_COLORIMETRY_DEFAULT);
  if (atomic)
    drmModeAtomicCommit(fd, drm_request, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);

  if (hdr_metadata_blob_id) {
    drmModeDestroyPropertyBlob(fd, hdr_metadata_blob_id);
    hdr_metadata_blob_id = 0;
  }

  if (atomic)
    drmModeAtomicFree(drm_request);
  drmModeFreePlane(ovr);
  drmModeFreePlaneResources(plane_resources);
  drmModeFreeEncoder(encoder);
  drmModeFreeConnector(connector);
  drmModeFreeCrtc(crtc);
  drmModeFreeResources(resources);

  close(fd);
}

int rk_submit_decode_unit(PDECODE_UNIT decodeUnit) {

  int result = DR_OK;
  PLENTRY entry = decodeUnit->bufferList;
  int length = 0;

  if (ensure_buf_size(&pkt_buf, &pkt_buf_size, decodeUnit->fullLength)) {
    // Buffer was reallocated, so update the mpp_packet accordingly
    mpp_packet_set_data(mpi_packet, pkt_buf);
    mpp_packet_set_size(mpi_packet, pkt_buf_size);
  }

  while (entry != NULL) {
    memcpy(pkt_buf+length, entry->data, entry->length);
    length += entry->length;
    entry = entry->next;
  }

  mpp_packet_set_pos(mpi_packet, pkt_buf);
  mpp_packet_set_length(mpi_packet, length);

  if (last_hdr_state != decodeUnit->hdrActive) {
    if (hdr_metadata_prop != NULL) {
      int err;

      if (hdr_metadata_blob_id) {
        drmModeDestroyPropertyBlob(fd, hdr_metadata_blob_id);
        hdr_metadata_blob_id = 0;
      }

      if (decodeUnit->hdrActive) {
        struct rk_hdr_output_metadata outputMetadata;
        SS_HDR_METADATA sunshineHdrMetadata;

        // Sunshine will have HDR metadata but GFE will not
        if (!LiGetHdrMetadata(&sunshineHdrMetadata)) {
          memset(&sunshineHdrMetadata, 0, sizeof(sunshineHdrMetadata));
        }

        outputMetadata.metadata_type = 0; // HDMI_STATIC_METADATA_TYPE1
        outputMetadata.hdmi_metadata_type1.eotf = 2; // SMPTE ST 2084
        outputMetadata.hdmi_metadata_type1.metadata_type = 0; // Static Metadata Type 1
        for (int i = 0; i < 3; i++) {
          outputMetadata.hdmi_metadata_type1.display_primaries[i].x = sunshineHdrMetadata.displayPrimaries[i].x;
          outputMetadata.hdmi_metadata_type1.display_primaries[i].y = sunshineHdrMetadata.displayPrimaries[i].y;
        }
        outputMetadata.hdmi_metadata_type1.white_point.x = sunshineHdrMetadata.whitePoint.x;
        outputMetadata.hdmi_metadata_type1.white_point.y = sunshineHdrMetadata.whitePoint.y;
        outputMetadata.hdmi_metadata_type1.max_display_mastering_luminance = sunshineHdrMetadata.maxDisplayLuminance;
        outputMetadata.hdmi_metadata_type1.min_display_mastering_luminance = sunshineHdrMetadata.minDisplayLuminance;
        outputMetadata.hdmi_metadata_type1.max_cll = sunshineHdrMetadata.maxContentLightLevel;
        outputMetadata.hdmi_metadata_type1.max_fall = sunshineHdrMetadata.maxFrameAverageLightLevel;

        err = drmModeCreatePropertyBlob(fd, &outputMetadata, sizeof(outputMetadata), &hdr_metadata_blob_id);
        if (err < 0) {
          hdr_metadata_blob_id = 0;
          fprintf(stderr, "Failed to create HDR metadata blob: %d\n", errno);
        }
      }

      err = drmModeObjectSetProperty(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR, hdr_metadata_prop->prop_id, hdr_metadata_blob_id);
      if (err < 0) {
        fprintf(stderr, "Failed to set HDR metadata: %d\n", errno);
      } else {
        printf("Set display HDR mode: %s\n", decodeUnit->hdrActive ? "active" : "inactive");
      }
    } else {
      fprintf(stderr, "HDR_OUTPUT_METADATA property is not supported by your display/kernel. Do you have an HDR display connected?\n");
    }

    // Adjust plane EOTF property
    set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "EOTF", decodeUnit->hdrActive ? 2 : 0); // PQ or SDR
  }

  if (last_colorspace != decodeUnit->colorspace) {
    uint32_t v4l2_colorspace;
    switch (decodeUnit->colorspace) {
    default:
      fprintf(stderr, "Unknown frame colorspace: %d\n", decodeUnit->colorspace);
      /* fall-through */
    case COLORSPACE_REC_601:
      v4l2_colorspace = V4L2_COLORSPACE_SMPTE170M;
      break;
    case COLORSPACE_REC_709:
      v4l2_colorspace = V4L2_COLORSPACE_REC709;
      break;
    case COLORSPACE_REC_2020:
      v4l2_colorspace = V4L2_COLORSPACE_BT2020;
      break;
    }
    set_property(plane_id, DRM_MODE_OBJECT_PLANE, plane_props, "COLOR_SPACE", v4l2_colorspace);
  }

  last_colorspace = decodeUnit->colorspace;
  last_hdr_state = decodeUnit->hdrActive;

  while (MPP_OK != mpi_api->decode_put_packet(mpi_ctx, mpi_packet));

  return result;
}

DECODER_RENDERER_CALLBACKS decoder_callbacks_rk = {
  .setup = rk_setup,
  .cleanup = rk_cleanup,
  .submitDecodeUnit = rk_submit_decode_unit,
  .capabilities = CAPABILITY_DIRECT_SUBMIT,
};

#endif
