/**
   @file avi.h

   @brief Minimal MJPEG-AVI writer for on-device timelapse video.

   Wraps the camera's JPEG frames into a standard AVI (MJPG) container on the
   SD card, timestamp-named. The header is patched after every frame, so the
   file stays playable even if recording is interrupted (e.g. power loss).
   No index (idx1) is written - players still play it, just without seeking.

   @bug: no known bug
*/

#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD_MMC.h>

#include "mcu_cfg.h"

#if (defined(ENABLE_TIMELAPSE_VIDEO) && (true == ENABLE_TIMELAPSE_VIDEO))

class AviRecorder {
private:
  File file;                     ///< open AVI file
  bool recording;                ///< true while a recording is active
  uint32_t frameCount;           ///< frames written
  uint32_t moviBytes;            ///< bytes of frame data in the movi list (chunks incl. headers/pad)
  uint16_t width;                ///< frame width
  uint16_t height;               ///< frame height
  uint16_t fps;                  ///< playback frames per second
  String path;                   ///< current file path
  SemaphoreHandle_t mutex;       ///< guards start/stop vs. frame append

  void WriteHeader();            ///< write the 224-byte AVI header (placeholders)
  void PatchHeader();            ///< patch size/count fields to the current state

public:
  AviRecorder();
  ~AviRecorder(){};

  bool Begin(String i_path, uint16_t i_w, uint16_t i_h, uint16_t i_fps);
  bool AddFrame(const uint8_t *i_jpeg, size_t i_len);
  bool End();
  bool IsRecording();
  uint32_t GetFrameCount();
  String GetPath();
};

extern AviRecorder TimelapseAvi;  ///< global timelapse AVI recorder

#endif // ENABLE_TIMELAPSE_VIDEO
/* EOF */
