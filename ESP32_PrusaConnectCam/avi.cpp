/**
   @file avi.cpp

   @brief Minimal MJPEG-AVI writer for on-device timelapse video. See avi.h.

   @bug: no known bug
*/

#include "avi.h"

#if (defined(ENABLE_TIMELAPSE_VIDEO) && (true == ENABLE_TIMELAPSE_VIDEO))

#include "log.h"

AviRecorder TimelapseAvi;

#define AVI_HEADER_SIZE   224   ///< bytes before the first frame chunk

/* header field offsets that get patched */
#define OFF_RIFF_SIZE     4     ///< RIFF chunk size (fileSize - 8)
#define OFF_TOTAL_FRAMES  48    ///< avih dwTotalFrames
#define OFF_STRH_LENGTH   140   ///< strh dwLength (frame count)
#define OFF_MOVI_SIZE     216   ///< movi LIST size (4 + moviBytes)

/* little-endian helpers into a byte buffer */
static void put_u32(uint8_t *b, uint32_t v) {
  b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF; b[2] = (v >> 16) & 0xFF; b[3] = (v >> 24) & 0xFF;
}
static void put_u16(uint8_t *b, uint16_t v) {
  b[0] = v & 0xFF; b[1] = (v >> 8) & 0xFF;
}
static void put_4cc(uint8_t *b, const char *cc) {
  b[0] = cc[0]; b[1] = cc[1]; b[2] = cc[2]; b[3] = cc[3];
}

AviRecorder::AviRecorder() {
  recording = false;
  frameCount = 0;
  moviBytes = 0;
  width = 0;
  height = 0;
  fps = 10;
  mutex = xSemaphoreCreateMutex();
}

/**
   @brief Build and write the 224-byte MJPEG-AVI header (counts/sizes = 0).
*/
void AviRecorder::WriteHeader() {
  uint8_t h[AVI_HEADER_SIZE];
  memset(h, 0, sizeof(h));

  uint32_t usPerFrame = (fps > 0) ? (1000000UL / fps) : 100000UL;

  put_4cc(h + 0, "RIFF");
  put_u32(h + 4, AVI_HEADER_SIZE - 8);   /* patched */
  put_4cc(h + 8, "AVI ");

  put_4cc(h + 12, "LIST");
  put_u32(h + 16, 192);                  /* hdrl LIST size (fixed) */
  put_4cc(h + 20, "hdrl");

  put_4cc(h + 24, "avih");
  put_u32(h + 28, 56);
  put_u32(h + 32, usPerFrame);           /* dwMicroSecPerFrame */
  put_u32(h + 48, 0);                    /* dwTotalFrames (patched) */
  put_u32(h + 56, 1);                    /* dwStreams */
  put_u32(h + 64, width);                /* dwWidth */
  put_u32(h + 68, height);               /* dwHeight */

  put_4cc(h + 88, "LIST");
  put_u32(h + 92, 116);                  /* strl LIST size (fixed) */
  put_4cc(h + 96, "strl");

  put_4cc(h + 100, "strh");
  put_u32(h + 104, 56);
  put_4cc(h + 108, "vids");
  put_4cc(h + 112, "MJPG");
  put_u32(h + 128, 1);                   /* dwScale */
  put_u32(h + 132, fps);                 /* dwRate -> fps = rate/scale */
  put_u32(h + 140, 0);                   /* dwLength (patched) */
  put_u16(h + 160, width);               /* rcFrame.right */
  put_u16(h + 162, height);              /* rcFrame.bottom */

  put_4cc(h + 164, "strf");
  put_u32(h + 168, 40);
  put_u32(h + 172, 40);                  /* biSize */
  put_u32(h + 176, width);               /* biWidth */
  put_u32(h + 180, height);              /* biHeight */
  put_u16(h + 184, 1);                   /* biPlanes */
  put_u16(h + 186, 24);                  /* biBitCount */
  put_4cc(h + 188, "MJPG");              /* biCompression */

  put_4cc(h + 212, "LIST");
  put_u32(h + 216, 4);                   /* movi LIST size (patched) */
  put_4cc(h + 220, "movi");

  file.seek(0);
  file.write(h, sizeof(h));
}

/**
   @brief Patch the size/count fields to reflect the current frame count.
*/
void AviRecorder::PatchHeader() {
  uint8_t b[4];
  put_u32(b, AVI_HEADER_SIZE + moviBytes - 8);  file.seek(OFF_RIFF_SIZE);    file.write(b, 4);
  put_u32(b, frameCount);                        file.seek(OFF_TOTAL_FRAMES); file.write(b, 4);
  put_u32(b, frameCount);                        file.seek(OFF_STRH_LENGTH);  file.write(b, 4);
  put_u32(b, 4 + moviBytes);                     file.seek(OFF_MOVI_SIZE);    file.write(b, 4);
  file.flush();
}

/**
   @brief Start a new AVI recording.
*/
bool AviRecorder::Begin(String i_path, uint16_t i_w, uint16_t i_h, uint16_t i_fps) {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ret = false;

  if (true == recording) {
    SystemLog.AddEvent(LogLevel_Warning, F("[AVI] Already recording"));
  } else {
    file = SD_MMC.open(i_path.c_str(), FILE_WRITE);
    if (!file) {
      SystemLog.AddEvent(LogLevel_Error, F("[AVI] Failed to create file: "), i_path);
    } else {
      path = i_path;
      width = i_w;
      height = i_h;
      fps = (i_fps > 0) ? i_fps : 10;
      frameCount = 0;
      moviBytes = 0;
      WriteHeader();
      file.flush();
      recording = true;
      ret = true;
      SystemLog.AddEvent(LogLevel_Info, F("[AVI] Recording started: "), i_path);
    }
  }

  xSemaphoreGive(mutex);
  return ret;
}

/**
   @brief Append one JPEG frame as a '00dc' chunk and patch the header.
*/
bool AviRecorder::AddFrame(const uint8_t *i_jpeg, size_t i_len) {
  if ((false == recording) || (i_jpeg == NULL) || (i_len == 0)) {
    return false;
  }

  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ret = false;

  if (true == recording) {
    uint8_t chunkHdr[8];
    put_4cc(chunkHdr, "00dc");
    put_u32(chunkHdr + 4, (uint32_t)i_len);

    file.seek(AVI_HEADER_SIZE + moviBytes);
    size_t w1 = file.write(chunkHdr, 8);
    size_t w2 = file.write(i_jpeg, i_len);

    /* AVI chunks are word-aligned: pad odd-length frames with one 0 byte */
    size_t pad = (i_len & 1) ? 1 : 0;
    if (pad) {
      uint8_t z = 0;
      file.write(&z, 1);
    }

    if ((w1 == 8) && (w2 == i_len)) {
      moviBytes += 8 + i_len + pad;
      frameCount++;
      PatchHeader();
      ret = true;
    } else {
      SystemLog.AddEvent(LogLevel_Error, F("[AVI] Frame write failed - stopping recording"));
      file.close();
      recording = false;
    }
  }

  xSemaphoreGive(mutex);
  return ret;
}

/**
   @brief Finish the recording and close the file.
*/
bool AviRecorder::End() {
  xSemaphoreTake(mutex, portMAX_DELAY);
  bool ret = false;

  if (true == recording) {
    PatchHeader();
    file.flush();
    file.close();
    recording = false;
    ret = true;
    SystemLog.AddEvent(LogLevel_Info, "[AVI] Recording stopped: " + path + " (" + String(frameCount) + " frames)");
  }

  xSemaphoreGive(mutex);
  return ret;
}

bool AviRecorder::IsRecording() {
  return recording;
}

uint32_t AviRecorder::GetFrameCount() {
  return frameCount;
}

String AviRecorder::GetPath() {
  return path;
}

#endif // ENABLE_TIMELAPSE_VIDEO
/* EOF */
