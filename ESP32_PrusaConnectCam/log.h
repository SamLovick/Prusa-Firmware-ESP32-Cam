/**
   @file log.h

   @brief log library

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#pragma once

#include <Arduino.h>

#include "micro_sd.h"
#include "log_level.h"
#include "mcu_cfg.h"

class Logs : public MicroSd {
private:
  LogLevel_enum LogLevel;     ///< LogLevel
  String FileName;            ///< log File name
  String FilePath;            ///< log file patch
  uint16_t FileMaxSize;       ///< log file max size
  bool NtpTimeSynced;         ///< status NTP time sync
  String LogMsg;              ///< log message
  File LogFile;               ///< log file object
  bool LogFileOpened;         ///< log file opened status
  SemaphoreHandle_t LogMutex; ///< log mutex

  String BootLog = "";                                   ///< captured boot messages (until boot complete)
  bool BootComplete = false;                             ///< true once boot finished; stop capturing boot log
  String ErrorLog = "";                                  ///< captured error/warning messages (ring buffer)
  bool RoutineEnable = LOG_ROUTINE_MESSAGES_DEFAULT;     ///< enable/disable routine (high-frequency) messages

  void CaptureBuffers(const String&, LogLevel_enum);     ///< append a logged message to the boot / error buffers

public:
  Logs();
  Logs(String, String);
  Logs(LogLevel_enum, String, String);
  Logs(String, String, uint16_t);
  Logs(LogLevel_enum, String, String, uint16_t);
  ~Logs(){};

  void Init();
  void LogOpenFile();
  void LogCloseFile();
  void LogCheckOpenedFile();
  void AddEvent(LogLevel_enum, String, bool = true, bool = true);
  void AddEvent(LogLevel_enum, const __FlashStringHelper*, String, bool = true, bool = true);

  /* Routine (high-frequency) messages - only logged when routine logging is enabled */
  void AddEventRoutine(LogLevel_enum, String, bool = true, bool = true);
  void AddEventRoutine(LogLevel_enum, const __FlashStringHelper*, String, bool = true, bool = true);

  void MarkBootComplete();          ///< stop capturing to the boot log
  String GetBootLog();              ///< get captured boot messages
  String GetErrorLog();             ///< get captured error/warning messages
  void ClearErrorLog();             ///< clear the error/warning buffer
  void SetRoutineLogEnable(bool);   ///< enable/disable routine messages
  bool GetRoutineLogEnable();       ///< get routine messages state

  void SetLogLevel(LogLevel_enum);
  void SetFileName(String);
  void SetFilePath(String);
  void SetFileMaxSize(uint16_t);
  void SetNtpTimeSynced(bool);

  String GetFileName();
  String GetFilePath();
  LogLevel_enum GetLogLevel();
  bool GetNtpTimeSynced();
  void CheckMaxLogFileSize();
  void CheckCardSpace();
  bool GetLogFileOpened();

  String GetSystemTime();
};

extern Logs SystemLog;  ///< log object

/* EOF */