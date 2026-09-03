/**
   @file ov5640_af.h

   @brief OV5640 autofocus support for the PrusaConnect ESP32 camera.

   The OV5640 sensor contains an internal 8051 MCU that drives the VCM
   (voice-coil motor) autofocus lens. This module is a thin wrapper around the
   esp_camera driver's built-in autofocus API (esp_camera_af_*), exposing
   single / continuous / pause / release focus commands to the web interface.

   @bug: no known bug
*/

#pragma once

#include "Arduino.h"
#include "mcu_cfg.h"

#if (defined(ENABLE_OV5640_AUTOFOCUS) && (true == ENABLE_OV5640_AUTOFOCUS))

#include "esp_camera.h"  // sensor_t, OV5640_PID

/* Initialize autofocus for the given esp_camera sensor handle. For an OV5640
   this downloads the AF firmware via the driver. Returns true on success.
   Blocking (up to a few seconds) - call at init only. */
bool OV5640_LoadAfFirmware(sensor_t *sensor);

/* AF commands. Return true when the driver accepted the command. */
bool OV5640_AfSingle();      ///< trigger one focus cycle
bool OV5640_AfContinuous();  ///< enable continuous autofocus (AUTO mode)
bool OV5640_AfPause();       ///< pause continuous AF (switch to MANUAL, hold position)
bool OV5640_AfRelease();     ///< release focus (manual position / AF off)

/* Read the raw AF status byte. */
uint8_t OV5640_AfStatus();

/* Human readable status string for the current AF state. */
String OV5640_AfStatusText();

/* True when autofocus was successfully initialized. */
bool OV5640_AfAvailable();

#endif // ENABLE_OV5640_AUTOFOCUS
/* EOF */
