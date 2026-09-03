/**
   @file micro_sd.cpp

   @brief library for communication with micro-SD card

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#include "micro_sd.h"
#include "log.h"  // SystemLog (for boot-log-visible SD diagnostics)

/**
   @brief Constructor
   @param none
   @return none
*/
MicroSd::MicroSd() {
  CardDetected = false;
  DetectAfterBoot = false;
  CardMountFailed = false;
  sdCardMutex = xSemaphoreCreateMutex();
}

/**
   @brief Reinit micro SD card
   @param none
   @return none
*/
void MicroSd::ReinitCard() {
  Serial.println(F("Reinit micro SD card!"));
  Serial.println(F("Deinit micro SD card"));
  SD_MMC.end();
  delay(50);
  Serial.println(F("Init micro SD card"));
  InitSdCard();
}

/**
   @brief Reformat the SD card to FAT32 (recovery for a corrupt card).

          Mounts with format_if_mount_failed = true: if the card is unreadable
          (corrupt FAT) it is reformatted so it works again; if the card already
          mounts cleanly it is left untouched (this call can't wipe a good card).
          Intended to be triggered manually from the web UI so the user never has
          to pull the card and reformat it in Windows.
   @return bool - true if the card is mounted (recovered or already OK) afterwards
*/
bool MicroSd::FormatCard() {
  xSemaphoreTake(sdCardMutex, portMAX_DELAY);
  bool ok = false;

  SystemLog.AddEvent(LogLevel_Warning, F("SD: manual Recover/Reformat requested"));

  /* close any file we might be holding open, then fully tear down the bus */
  if (file) {
    file.close();
  }
  SD_MMC.end();
  delay(100);
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_DATA0);

  /* slow, reliable clock for the format/mount */
  if (SD_MMC.begin("/sdcard", true, true, SD_INIT_FREQ_FALLBACK_KHZ)) {
    if (SD_MMC.cardType() != CARD_NONE) {
      ok = true;
      CardDetected = true;
      DetectAfterBoot = true;
      CardMountFailed = false;
      CheckCardUsedStatus();
      SystemLog.AddEvent(LogLevel_Info, F("SD: card is mounted after Recover/Reformat"));
    } else {
      CardDetected = false;
      SystemLog.AddEvent(LogLevel_Error, F("SD: Recover/Reformat - no card responding on the bus"));
    }
  } else {
    CardDetected = false;
    CardMountFailed = true;
    SystemLog.AddEvent(LogLevel_Error, F("SD: Recover/Reformat failed - card not responding"));
  }

  xSemaphoreGive(sdCardMutex);
  return ok;
}

/**
   @brief Whether a card was present on the bus but its filesystem failed to
          mount (i.e. it looks corrupt and can be recovered by reformatting).
   @return bool
*/
bool MicroSd::GetCardMountFailed() {
  return CardMountFailed;
}

/**
 * @brief Open file
 * 
 * @param i_file - file
 * @param i_path - path and file name
 * @return true 
 * @return false 
 */
bool MicroSd::OpenFile(File *i_file, String i_path) {
    bool status = false;

  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.println("Opening file: " + i_path);
#endif

    if (SD_MMC.cardType() == CARD_NONE) {
        Serial.println("No SD card detected");
        CardDetected = false;
    } else {

      *i_file = SD_MMC.open(i_path.c_str(), FILE_APPEND);
      if (!*i_file) {
#if (true == CONSOLE_VERBOSE_DEBUG)
        Serial.println("Failed to open file");
#endif
        CardDetected = false;
      } else {
        status = true;
#if (true == CONSOLE_VERBOSE_DEBUG)
        Serial.println("File opened");
#endif
      }
    }
  }
  return status;
}

/**
 * @brief Close file
 * 
 * @param i_file - file
 */
void MicroSd::CloseFile(File *i_file) {
  if (*i_file) {
    i_file->close();
  }
}

/**
 * @brief Check if file is opened
 * 
 * @param i_file - file
 * @return true 
 * @return false 
 */
bool MicroSd::CheckOpenFile(File *i_file) {
  if (!*i_file) {
#if (true == CONSOLE_VERBOSE_DEBUG)    
    Serial.println(F("File not opened!"));
#endif
    return false;
  } else {
    return true;
  }
}

/**
   @brief Init SD card. And check, if is SD card inserted
   @param none
   @return none
*/
void MicroSd::InitSdCard() {
  /* Start INIT Micro SD card */
  Serial.println(F("Start init micro-SD Card"));

  /* set SD card to 1-line/1-bit mode (XIAO ESP32-S3 Sense: CLK=7, CMD=9, D0=8) */
  /* https://github.com/espressif/arduino-esp32/blob/master/libraries/SD_MMC/src/SD_MMC.h */

  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_DATA0);

  CardDetected = false;
  bool mounted = false;

  /* Log the SD pin map once so it is visible in the boot log */
  SystemLog.AddEvent(LogLevel_Info, "SD pins (1-bit) CLK/CMD/D0: " + String(SD_PIN_CLK) + "/" + String(SD_PIN_CMD) + "/" + String(SD_PIN_DATA0));

  /* Let the card power up before the first access. In the fast early-boot path
     the card is not yet ready; an isolated test that waits ~3 s mounts fine at
     every clock, so give it time and retry over a several-second window. */
  delay(SD_INIT_SETTLE_MS);

  for (int attempt = 1; (attempt <= SD_INIT_RETRY_COUNT) && (false == mounted); attempt++) {
    int freq_khz = (attempt <= SD_INIT_HIGHFREQ_ATTEMPTS) ? SD_INIT_FREQ_KHZ : SD_INIT_FREQ_FALLBACK_KHZ;
    SystemLog.AddEvent(LogLevel_Info, "SD mount attempt " + String(attempt) + "/" + String(SD_INIT_RETRY_COUNT) + " at " + String(freq_khz) + " kHz (1-bit)");

    /* fully tear down and re-assert pins before every attempt (matches the
       known-good isolated test sequence) */
    SD_MMC.end();
    delay(50);
    SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_DATA0);

    if (SD_MMC.begin("/sdcard", true, SD_INIT_FORMAT_IF_FAILED, freq_khz)) {
      mounted = true;
      SystemLog.AddEvent(LogLevel_Info, "SD mounted at " + String(freq_khz) + " kHz");
    } else {
      /* cardType() after a failed mount: CARD_NONE(0) => card not responding on
         the bus (pins/clock/hardware); otherwise the FAT mount failed */
      SystemLog.AddEvent(LogLevel_Warning, "SD mount failed, cardType=" + String((int)SD_MMC.cardType()));
      delay(SD_INIT_RETRY_DELAY_MS);
    }
  }

  if (false == mounted) {
    /* distinguish "card present but filesystem corrupt" from "no card / not
       responding": cardType() != CARD_NONE means the card answered on the bus
       but the FAT could not be mounted -> corruption the user can recover from
       with the web "Recover / Reformat SD" button (no Windows needed). */
    if (SD_MMC.cardType() != CARD_NONE) {
      CardMountFailed = true;
      SystemLog.AddEvent(LogLevel_Error, F("SD card present but filesystem could NOT be mounted - likely corrupt. Use 'Recover / Reformat SD' on the System page to fix it."));
    } else {
      SystemLog.AddEvent(LogLevel_Error, F("SD Card Mount Failed after all attempts (no card / not responding on the bus)"));
    }
    CardDetected = false;
    CardSizeMB = 0;
    return;
  }
  CardMountFailed = false;

  /* check microSD card and card type */
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println(F("No SD_MMC card attached"));
    CardDetected = false;
    CardSizeMB = 0;
    return;
  }

  /* print card type */
  Serial.print(F("Found card. Card Type: "));
  if (cardType == CARD_MMC) {
    Serial.println(F("MMC"));
  } else if (cardType == CARD_SD) {
    Serial.println(F("SDSC"));
  } else if (cardType == CARD_SDHC) {
    Serial.println(F("SDHC"));
  } else {
    Serial.println(F("UNKNOWN"));
  }

  CardDetected = true;
  DetectAfterBoot = true;

  /* calculation card size */
  CheckCardUsedStatus();
}

/**
   @brief List directory on the micro SD card
   @param fs::FS - card
   @param String - Directory name
   @param uint8_t - levels
   @return none
*/
void MicroSd::ListDir(fs::FS &fs, String DirName, uint8_t levels) {
  if (true == CardDetected) {
    Serial.printf("Listing directory: %s\n", DirName.c_str());

    File root = fs.open(DirName.c_str());
    if (!root) {
      Serial.println(F("Failed to open directory"));
      return;
    }
    if (!root.isDirectory()) {
      Serial.println(F("Not a directory"));
      return;
    }

    File file = root.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        Serial.print(F("  DIR : "));
        Serial.println(file.name());
        if (levels) {
          ListDir(fs, file.path(), levels - 1);
        }
      } else {
        Serial.print(F("  FILE: "));
        Serial.print(file.name());
        Serial.print(F("  SIZE: "));
        Serial.println(file.size());
      }
      file = root.openNextFile();
    }
  }
}

/**
   @brief Check directory on the micro SD card
   @param fs::FS - card
   @param String - dir name
   @return bool - status
*/
bool MicroSd::CheckDir(fs::FS &fs, String path) {
  bool status = false;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Checking Dir: %s... ", path.c_str());
#endif
  
    if (fs.exists(path.c_str())) {
      status = true;
    }

  }
  return status;
}

/**
   @brief List directory on the micro SD card
   @param fs::FS - card
   @param String - dir name
   @return bool - status
*/
bool MicroSd::CreateDir(fs::FS &fs, String path) {
  bool status = false;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Creating Dir: %s... ", path.c_str());
#endif

    if (fs.mkdir(path.c_str())) {
      status = true;
    }

#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.println((status == true) ? "Created" : "Failed");
#endif
  }
  return status;
}

/**
   @brief remove directory on the micro SD card
   @param fs::FS - card
   @param String - dir name
   @return bool - status
*/
bool MicroSd::RemoveDir(fs::FS &fs, String path) {
  bool status = false;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Removing Dir: %s... ", path.c_str());
#endif

    if (fs.rmdir(path.c_str())) {
      status = true;
    }

#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.println((status == true) ? "Removed" : "Failed");
#endif
  }
  return status;
}

/**
   @brief Read file and print data to console
   @param fs::FS - card
   @param String - file name
   @return none
*/
void MicroSd::ReadFileConsole(fs::FS &fs, String path) {
  if (true == CardDetected) {
    Serial.printf("Reading file: %s\n", path.c_str());

    File file = fs.open(path.c_str());
    if (!file) {
      Serial.println(F("Failed to open file for reading"));
      return;
    }

    Serial.print(F("Read from file: "));
    while (file.available()) {
      Serial.write(file.read());
    }
  }
}

/**
   @brief Write message to file
   @param fs::FS - card
   @param String - file name
   @param String - message
   @return bool - status
*/
bool MicroSd::WriteFile(fs::FS &fs, String path, String message) {
  bool status = false;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Writing file: %s... ", path.c_str());
#endif

    File file = fs.open(path.c_str(), FILE_WRITE);
    if (!file) {
#if (true == CONSOLE_VERBOSE_DEBUG)
      Serial.printf("Failed to open file for writing");
#endif
    } else {
      if (file.print(message.c_str())) {
        status = true;
      }

#if (true == CONSOLE_VERBOSE_DEBUG)
      Serial.println((status == true) ? "File written" : "Write Failed");
#endif
    }
  }
  return status;
}

/**
   @brief Added text to end of file
   @param fs::FS - card
   @param String - file name
   @param String - message
   @return bool - status
*/
bool MicroSd::AppendFile(fs::FS &fs, String path, String message) {
  bool status = false;

  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Appending to file: %s... ", path.c_str());
#endif

    File file = fs.open(path.c_str(), FILE_APPEND);
    if (!file) {
#if (true == CONSOLE_VERBOSE_DEBUG)
      Serial.println("Failed to open file for appending");
#endif
      CardDetected = false;
    } else {
      if (file.print(message.c_str())) {
        status = true;
      } 

#if (true == CONSOLE_VERBOSE_DEBUG)
      Serial.println((status == true) ? "Message appended" : "Append Failed");
#endif
    }
  }
  return status;
}

/**
   @brief Added text to end of file
   @param File - file
   @param String - message
   @return bool - status
*/
bool MicroSd::AppendFile(File *i_file, String *i_msg) {
  /* take mutex */
  xSemaphoreTake(sdCardMutex, portMAX_DELAY);
  bool status = false;

  /* check if card is corrupted */
  if (false == isCardCorrupted()) {
    xSemaphoreGive(sdCardMutex);
    return false;
  }
  
  /* check if card is detected */
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Appending to file:");
#endif

    /* check if file is opened */
    if (!*i_file) {
      Serial.println("File not opened");
      CardDetected = false;

    } else {
      /* write to file */
      if (i_file->print(i_msg->c_str())) {
        if (*i_file) {
          i_file->flush();

          /* check if write was OK */
          if (!i_file->getWriteError()) {
#if (true == CONSOLE_VERBOSE_DEBUG)
            Serial.println("Write OK");
#endif
            status = true;

          } else {
            Serial.println(F("Failed write to file"));

          }
        } else {
          Serial.println(F("File not opened!"));

        }
      } else {
        Serial.println(F("Failed write to file!"));

      }
#if (true == CONSOLE_VERBOSE_DEBUG)
      Serial.println((status == true) ? "Message appended" : "Append Failed");
#endif
    }
  }

  /* give mutex */
  xSemaphoreGive(sdCardMutex);
  return status;
}

/**
   @brief Rename file on the SD card
   @param fs::FS - card
   @param String - origin file name
   @param String - new file name
   @return bool - status
*/
bool MicroSd::RenameFile(fs::FS &fs, String path1, String path2) {
  bool status = false;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Renaming file %s to %s... ", path1.c_str(), path2.c_str());
#endif
    if (fs.rename(path1.c_str(), path2.c_str())) {
      status = true;
    }

#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.println((status == true) ? "File renamed" : "Rename Failed");
#endif
  }

  return status;
}

/**
   @brief Delete file on the SD card
   @param fs::FS - card
   @param String - file name
   @return bool - status
*/
bool MicroSd::DeleteFile(fs::FS &fs, String path) {
  bool status = false;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Deleting file: %s... ", path.c_str());
#endif
    if (fs.remove(path.c_str())) {
      status = true;
    }

#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.println((status == true) ? "File deleted" : "Delete Failed");
#endif
  }

  return status;
}

/**
   @brief Get file size in the kb
   @param fs::FS - card
   @param String - file name
   @return uint32_t - size
*/
uint32_t MicroSd::GetFileSize(fs::FS &fs, String path) {
  uint32_t ret = 0;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("Getting file size: %s... ", path.c_str());
#endif
    File file = fs.open(path.c_str(), FILE_APPEND);
    if (!file) {
#if (true == CONSOLE_VERBOSE_DEBUG)
      Serial.println("Failed to open file for appending");
#endif
      return 0;
    }

    ret = file.size() / 1024; /* convert from bytes to kb */
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf(" File size: %d\n ", ret);
#endif
  }

  return ret; /* kb*/
}

/**
   @brief Check file count with partial match
   @param fs::FS - card
   @param String - dir name
   @param String - file name
   @return int16_t - count
*/
uint16_t MicroSd::FileCount(fs::FS &fs, String DirName, String FileName) {
  uint16_t FileCount = 0;
  if (true == CardDetected) {
#if (true == CONSOLE_VERBOSE_DEBUG)
    Serial.printf("File name count: %s\n", DirName.c_str());
#endif

    File root = fs.open(DirName.c_str());
    if (!root) {
      Serial.println(F("Failed to open directory"));
      return 0;
    }
    if (!root.isDirectory()) {
      Serial.println(F("Not a directory"));
      return 0;
    }

    File file = root.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
#if (true == CONSOLE_VERBOSE_DEBUG)
        Serial.print("  FILE: ");
        Serial.print(file.name());
        Serial.print("  SIZE: ");
        Serial.print(file.size());
#endif
        if (String(file.name()).indexOf(FileName) != -1) {
          FileCount++;
#if (true == CONSOLE_VERBOSE_DEBUG)
          Serial.print(" - MATCH");
#endif
        }
#if (true == CONSOLE_VERBOSE_DEBUG)
        Serial.println("");
#endif
      }
      file = root.openNextFile();
    }
  }

  return FileCount;
}

/**
   @brief Remove files in directory
   @param fs::FS - card
   @param String - dir name
   @param int - max files
   @return bool - status
*/
bool MicroSd::RemoveFilesInDir(fs::FS &fs, String path, int maxFiles) {
  bool ret = false;
  File dir = fs.open(path.c_str());
  if (!dir) {
    return ret;
  }

  int fileCount = 0;
  File file = dir.openNextFile();
  while (file) {
    ret = true;
    String fileName = path + "/" + file.name();
    fs.remove(fileName.c_str());
    Serial.printf("Removing file: %s\n", fileName.c_str());
    fileCount++;
    if (fileCount >= maxFiles) {
      break;
    }
    file = dir.openNextFile();
  }

  return ret;
}


int MicroSd::CountFilesInDir(fs::FS &fs, String path) {
  uint16_t file_count = FileCount(fs, path, "");
  return file_count;
}

/**
   @brief Check card used status
   @param none
   @return bool - status
*/
void MicroSd::CheckCardUsedStatus() {

  CardSizeMB = SD_MMC.cardSize()  / (1024 * 1024);
  CardTotalMB = SD_MMC.totalBytes() / (1024 * 1024);
  CardUsedMB = SD_MMC.usedBytes() / (1024 * 1024);
  CardFreeMB = CardSizeMB - CardUsedMB;
  FreeSpacePercent = (CardFreeMB * 100) / CardSizeMB;
  UsedSpacePercent = 100 - FreeSpacePercent;

#if (true == CONSOLE_VERBOSE_DEBUG)  
  Serial.printf("Card size: %d MB, Total: %d MB, Used: %d MB, Free: %d GB, Free: %d %% \n", CardSizeMB, CardTotalMB, CardUsedMB, CardFreeMB, FreeSpacePercent);
#endif
}

/**
 * @brief Function to check if card is corrupted
 * 
 * @return true 
 * @return false 
 */
bool MicroSd::isCardCorrupted() {
  bool ret = true;
  if (true == CardDetected) {
    /* Lightweight presence check ONLY. This runs on every SD log append, so it
       must not do any expensive I/O. The previous version called
       SD_MMC.usedBytes() (and cardSize()) here, which walks the whole FAT -
       seconds on a large / near-full card, and worse at the lower SD clock -
       and was stalling the async web server whenever verbose logging (which
       writes every web request to the SD log) was enabled.
       cardType() just returns the cached type from the mounted card, no bus
       scan. Free space is tracked separately by CheckCardUsedStatus(), and a
       full card / write error is caught by the file.getWriteError() check in
       AppendFile(). */
    if (SD_MMC.cardType() == CARD_NONE) {
      Serial.println(F("No card detected!"));
      CardDetected = false;
      ret = false;
    }
  } else {
    ret = false;
  }

  return ret;
}

/**
   @brief Write picture to the SD card
   @param fs::FS - card
   @param String - file name
   @return String - data
*/  
bool MicroSd::WritePicture(String i_PhotoName, uint8_t *i_PhotoData, size_t i_PhotoLen) {
#if (true == CONSOLE_VERBOSE_DEBUG)  
  Serial.println(f("WritePicture"));
#endif
  bool ret_stat = false;

  File file = SD_MMC.open(i_PhotoName, FILE_WRITE);

  if (file) {
    size_t ret = 0;

    ret = file.write(i_PhotoData, i_PhotoLen);
    if (ret != i_PhotoLen) {
      Serial.println(F("Failed. Error while writing to file"));
    } else {
      Serial.printf("Saved as %s\n", i_PhotoName.c_str());
      ret_stat = true;
    }
    file.close();
  } else {
    Serial.printf("Failed. Could not open file: %s\n", i_PhotoName.c_str());
  }
  
 return ret_stat;
}

/**
   @brief Write picture to the SD card with EXIF data
   @param fs::FS - card
   @param String - file name
   @param uint8_t - data
   @param size_t - data length
   @param const uint8_t - EXIF data
   @param size_t - EXIF data length
   @return bool - status
*/
bool MicroSd::WritePicture(String i_PhotoName, uint8_t *i_PhotoData, size_t i_PhotoLen, const uint8_t *i_PtohoExif, size_t i_PhotoExifLen) {

#if (true == CONSOLE_VERBOSE_DEBUG)  
  Serial.println(F("WritePicture EXIF"));
#endif
  bool ret_stat = false;

  File file = SD_MMC.open(i_PhotoName, FILE_WRITE);

  if (file) {
    size_t ret = 0;

    ret = file.write(i_PtohoExif, i_PhotoExifLen);
    ret += file.write(i_PhotoData, i_PhotoLen);
    if (ret != (i_PhotoLen + i_PhotoExifLen)) {
#if (true == CONSOLE_VERBOSE_DEBUG)        
      Serial.println(F("Failed. Error while writing to file"));
#endif
      ret_stat = false;
    } else {
#if (true == CONSOLE_VERBOSE_DEBUG)  
      Serial.printf("Saved as %s\n", i_PhotoName.c_str());
#endif
      ret_stat = true;
    }
    file.close();
  } else {
#if (true == CONSOLE_VERBOSE_DEBUG)  
    Serial.printf("Failed. Could not open file: %s\n", i_PhotoName.c_str());
#endif
    ret_stat = false;
  }
  
 return ret_stat;
}

/**
   @brief Get card detected status
   @param none
   @return bool - status
*/
bool MicroSd::GetCardDetectedStatus() {
  return CardDetected;
}

/**
   @brief Get card size
   @param none
   @return uint16_t - size
*/
uint16_t MicroSd::GetCardSizeMB() {
  return CardSizeMB;
}

/**
   @brief Get card detect after boot
   @param none
   @return bool - status
*/
bool MicroSd::GetCardDetectAfterBoot() {
  return DetectAfterBoot;
}

/**
   @brief Get card total MB
   @param none
   @return uint16_t - size
*/
uint16_t MicroSd::GetCardTotalMB() {
  return CardTotalMB;
}

/**
   @brief Get card used MB
   @param none
   @return uint16_t - size
*/
uint16_t MicroSd::GetCardUsedMB() {
  return CardUsedMB;
}

/**
   @brief Get card free MB
   @param none
   @return uint16_t - size
*/
uint16_t MicroSd::GetCardFreeMB() {
  return CardFreeMB;
}

/**
   @brief Get free space percent
   @param none
   @return uint8_t - percent
*/
uint8_t MicroSd::GetFreeSpacePercent() {
  return FreeSpacePercent;
}

/**
   @brief Get used space percent
   @param none
   @return uint8_t - percent
*/
uint8_t MicroSd::GetUsedSpacePercent() {
  return UsedSpacePercent;
}

/* EOF */