/**
   @file prusalink.cpp

   @brief PrusaLink (v1) print-state poller. See prusalink.h.

   @bug: no known bug
*/

#include "prusalink.h"

#if (defined(ENABLE_PRUSALINK) && (true == ENABLE_PRUSALINK))

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "log.h"
#include "camera.h"
#include "avi.h"
#include "cfg.h"

static String currentState = F("unknown");
static bool wasPrinting = false;
static bool autoStartedRecording = false;
static uint32_t lastPollMs = 0;
static bool autoTimelapseEnabled = PRUSALINK_AUTO_TIMELAPSE;  ///< runtime toggle (default from mcu_cfg.h)
static String plIp = "";           ///< printer IP (from config)
static String plKey = "";          ///< API key (from config)
static bool plCfgLoaded = false;   ///< config loaded from EEPROM

/* load the printer IP + API key from EEPROM */
void PrusaLink_LoadCfg() {
  plIp = SystemConfig.LoadPrusaLinkIp();
  plKey = SystemConfig.LoadPrusaLinkApiKey();
  plCfgLoaded = true;
}

/* update the printer IP + API key and persist them (blank key = keep current) */
void PrusaLink_SetConfig(String i_ip, String i_key) {
  if (i_ip.length() > 0) {
    plIp = i_ip;
    SystemConfig.SavePrusaLinkIp(i_ip);
  }
  if (i_key.length() > 0) {
    plKey = i_key;
    SystemConfig.SavePrusaLinkApiKey(i_key);
  }
  plCfgLoaded = true;
  lastPollMs = 0;  /* poll again promptly with the new config */
}

String PrusaLink_GetIp() {
  if (false == plCfgLoaded) { PrusaLink_LoadCfg(); }
  return plIp;
}

bool PrusaLink_IsKeySet() {
  if (false == plCfgLoaded) { PrusaLink_LoadCfg(); }
  return (plKey.length() > 0);
}

/* Consider these states as "a print is active" (recording should run) */
static bool StateIsActive(const String &s) {
  return (s == F("PRINTING")) || (s == F("PAUSED"));
}

#if (defined(ENABLE_TIMELAPSE_VIDEO) && (true == ENABLE_TIMELAPSE_VIDEO))
/**
   @brief Start a timelapse video for the current print (auto).
*/
static void PrusaLink_StartTimelapse() {
  if (false == SystemLog.GetCardDetectedStatus()) {
    SystemLog.AddEvent(LogLevel_Warning, F("[PrusaLink] Print started but no SD card - timelapse skipped"));
    return;
  }
  if (true == TimelapseAvi.IsRecording()) {
    return;  /* already recording (e.g. started manually) */
  }
  if (false == SystemLog.CheckDir(SD_MMC, TIMELAPS_PHOTO_FOLDER)) {
    SystemLog.CreateDir(SD_MMC, TIMELAPS_PHOTO_FOLDER);
  }
  String fn = String(TIMELAPS_PHOTO_FOLDER) + "/" + TIMELAPS_VIDEO_PREFIX + SystemLog.GetSystemTime() + TIMELAPS_VIDEO_SUFFIX;
  if (true == TimelapseAvi.Begin(fn, SystemCamera.GetFrameSizeWidth(), SystemCamera.GetFrameSizeHeight(), TIMELAPS_VIDEO_FPS)) {
    autoStartedRecording = true;
    SystemLog.AddEvent(LogLevel_Info, F("[PrusaLink] Print started - timelapse recording: "), fn);
  }
}

/**
   @brief Stop the auto-started timelapse video when the print ends.
*/
static void PrusaLink_StopTimelapse() {
  if ((true == TimelapseAvi.IsRecording()) && (true == autoStartedRecording)) {
    TimelapseAvi.End();
    SystemLog.AddEvent(LogLevel_Info, F("[PrusaLink] Print ended - timelapse stopped"));
  }
  autoStartedRecording = false;
}
#endif

/**
   @brief Handle a printer-state transition.
*/
static void PrusaLink_HandleState(const String &state) {
  bool printing = StateIsActive(state);

  if ((true == printing) && (false == wasPrinting)) {
    SystemLog.AddEvent(LogLevel_Info, F("[PrusaLink] Printer state -> "), state);
#if (defined(ENABLE_TIMELAPSE_VIDEO) && (true == ENABLE_TIMELAPSE_VIDEO))
    if (true == autoTimelapseEnabled) {
      PrusaLink_StartTimelapse();
    }
#endif
  } else if ((false == printing) && (true == wasPrinting)) {
    SystemLog.AddEvent(LogLevel_Info, F("[PrusaLink] Printer state -> "), state);
#if (defined(ENABLE_TIMELAPSE_VIDEO) && (true == ENABLE_TIMELAPSE_VIDEO))
    PrusaLink_StopTimelapse();  /* always finalize an auto-started recording, even if just disabled */
#endif
  }

  wasPrinting = printing;
}

/**
   @brief Poll the PrusaLink API for the current printer state.
*/
void PrusaLink_Poll() {
  /* rate limit */
  if ((lastPollMs != 0) && ((millis() - lastPollMs) < (PRUSALINK_POLL_INTERVAL_S * 1000UL))) {
    return;
  }
  lastPollMs = millis();

  if (false == plCfgLoaded) {
    PrusaLink_LoadCfg();
  }

  if (WL_CONNECTED != WiFi.status()) {
    currentState = F("no wifi");
    return;
  }
  if (plKey.length() == 0) {
    currentState = F("no api key");
    return;
  }

  String url = "http://" + plIp + "/api/v1/status";

  WiFiClient client;
  HTTPClient http;
  http.setConnectTimeout(PRUSALINK_HTTP_TIMEOUT_MS);
  http.setTimeout(PRUSALINK_HTTP_TIMEOUT_MS);

  if (false == http.begin(client, url)) {
    currentState = F("http error");
    return;
  }
  http.addHeader("X-Api-Key", plKey);

  int code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      currentState = F("bad json");
    } else {
      const char *st = doc["printer"]["state"] | "UNKNOWN";
      currentState = String(st);
      PrusaLink_HandleState(currentState);
    }
  } else if (code == 401 || code == 403) {
    currentState = F("auth failed");
  } else {
    currentState = "offline (" + String(code) + ")";
  }

  http.end();
}

String PrusaLink_GetState() {
  return currentState;
}

bool PrusaLink_IsPrinting() {
  return StateIsActive(currentState);
}

void PrusaLink_SetAutoTimelapse(bool i_data) {
  autoTimelapseEnabled = i_data;
  SystemLog.AddEvent(LogLevel_Info, F("[PrusaLink] Auto timelapse: "), String(i_data ? "enabled" : "disabled"));
}

bool PrusaLink_GetAutoTimelapse() {
  return autoTimelapseEnabled;
}

#endif // ENABLE_PRUSALINK
/* EOF */
