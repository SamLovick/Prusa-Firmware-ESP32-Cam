/**
   @file prusalink.h

   @brief Poll the printer's local PrusaLink API (v1) for print state.

   On a Prusa Core One (Buddy firmware) this reads GET /api/v1/status with an
   X-Api-Key header and interprets printer.state. When a print becomes active
   it can auto-start the MJPEG-AVI timelapse recording, and stop/finalize it
   when the print finishes.

   @bug: no known bug
*/

#pragma once

#include <Arduino.h>
#include "mcu_cfg.h"

#if (defined(ENABLE_PRUSALINK) && (true == ENABLE_PRUSALINK))

/* Poll the PrusaLink API. Rate-limited internally to PRUSALINK_POLL_INTERVAL_S;
   safe to call frequently from a periodic task. */
void PrusaLink_Poll();

/* Last known printer state string (e.g. "PRINTING", "IDLE", "offline"). */
String PrusaLink_GetState();

/* True while the printer is actively printing (PRINTING or PAUSED). */
bool PrusaLink_IsPrinting();

/* Runtime enable/disable of auto start/stop of the timelapse on print. */
void PrusaLink_SetAutoTimelapse(bool);
bool PrusaLink_GetAutoTimelapse();

/* Printer IP + API key config (stored in EEPROM). */
void PrusaLink_LoadCfg();
void PrusaLink_SetConfig(String i_ip, String i_key);  ///< blank key = keep current
String PrusaLink_GetIp();
bool PrusaLink_IsKeySet();

#endif // ENABLE_PRUSALINK
/* EOF */
