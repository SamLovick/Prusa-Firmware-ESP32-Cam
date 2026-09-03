/**
   @file server.cpp

   @brief Library for WEB server

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug

*/

#include "WebServer.h"
#include "Certificate.h"

AsyncWebServer server(WEB_SERVER_PORT);

/**
   @brief Load configuration from EEPROM
   @param none
   @return none
*/
void Server_LoadCfg() {
  WebBasicAuth.EnableAuth = SystemConfig.LoadBasicAuthFlag();
  WebBasicAuth.UserName = SystemConfig.LoadBasicAuthUsername();
  WebBasicAuth.Password = SystemConfig.LoadBasicAuthPassword();
}

/**
   @brief Init WEB server
   @param none
   @return none
*/
void Server_InitWebServer() {
  SystemLog.AddEvent(LogLevel_Info, F("Starting init WEB server"));

  /* route for get last capture photo */
  server.on("/saved-photo.jpg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get photo"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (SystemCamera.GetCameraCaptureSuccess() == false) {
      request->send(404, "text/plain", "Photo not found!");
      return;
    }
    SystemCamera.SetPhotoSending(true);

    SystemLog.AddEvent(LogLevel_Verbose, "Photo size: " + String(SystemCamera.GetPhotoFb()->len) + " bytes");

    if (SystemCamera.GetPhotoExifData()->header != NULL) {
      /* send photo with exif data */
      SystemLog.AddEvent(LogLevel_Verbose, F("Send photo with EXIF data"));
      size_t total_len = SystemCamera.GetPhotoExifData()->len + SystemCamera.GetPhotoFb()->len - SystemCamera.GetPhotoExifData()->offset;
      auto response = request->beginChunkedResponse("image/jpg", [total_len](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        size_t len = 0;
        if (index < SystemCamera.GetPhotoExifData()->len) {
          len = min(maxLen, SystemCamera.GetPhotoExifData()->len - index);
          memcpy(buffer, SystemCamera.GetPhotoExifData()->header + index, len);
        } else {
          size_t offset = index - SystemCamera.GetPhotoExifData()->len + SystemCamera.GetPhotoExifData()->offset;
          len = min(maxLen, SystemCamera.GetPhotoFb()->len - offset);
          memcpy(buffer, SystemCamera.GetPhotoFb()->buf + offset, len);
        }
        return len;
      });
      response->addHeader("Content-Length", String(total_len));
      request->send(response);

    } else {
      /* send photo without exif data */
      SystemLog.AddEvent(LogLevel_Verbose, F("Send photo without EXIF data"));
      request->send(200, "image/jpg", SystemCamera.GetPhotoFb()->buf, SystemCamera.GetPhotoFb()->len);
    }

    SystemCamera.SetPhotoSending(false);
  });

  /* route to jquery */
  server.on("/jquery-3.7.0.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get jquery-3.7.0.min.js"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "application/javascript", jquery_3_7_0_js);
  });

  Server_InitWebServer_JsonData();
  Server_InitWebServer_WebPages();
  Server_InitWebServer_Icons();
  Server_InitWebServer_Actions();
  Server_InitWebServer_Sets();
  Server_InitWebServer_Update();
  Server_InitWebServer_Stream();

  /* route for not found page */
  server.onNotFound(Server_handleNotFound);

  /* start WEB server */
  server.begin();
}

/**
   @brief Init WEB server json data
   @param none
   @return none
*/
void Server_InitWebServer_JsonData() {
  /* route for json with cfg parameters */
  server.on("/json_input", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get json_input"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), Server_GetJsonData().c_str());
  });

  /* route for json with wifi networks */
  server.on("/json_wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get json_wifi"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), SystemWifiMngt.GetAvailableWifiNetworks().c_str());
  });

  /* route for san wi-fi networks */
  server.on("/wifi_scan", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: scan WI-FI networks"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SCANNING);
    SystemWifiMngt.ScanWiFiNetwork();
  });

  /* route for firmware update processing status */
  server.on("/UpdateProcessing", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false)
      return;

    JsonDocument doc_json;
    doc_json["processed_bytes"] = String(FirmwareUpdate.TransferedBytes);
    doc_json["processed_percent"] = String(FirmwareUpdate.PercentProcess);
    doc_json["updating"] = FirmwareUpdate.Processing;
    doc_json["message"] = FirmwareUpdate.UpdatingStatus;
    String string_json = "";
    serializeJson(doc_json, string_json);

    request->send(200, "application/json", string_json);
  });
}

/**
   @brief Init WEB server web pages
   @param none
   @return none
*/
void Server_InitWebServer_WebPages() {
  /* Route for root / web page */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get index.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", index_html);
  });

  /* Route for styles */
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get styles.css"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/css", styles_css);
  });

  /* Route for java scripts */
  server.on("/scripts.js", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get scripts.js"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, "application/javascript", scripts_js);
  });


  /* Route for config web page */
  server.on("/page_config.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_config.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_config_html);
  });

  /* Route for wifi web page */
  server.on("/page_wifi.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_wifi.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_wifi_html);
  });

  /* Route for auth web page */
  server.on("/page_auth.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_auth.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_auth_html);
  });

  /* Route for system web page */
  server.on("/page_system.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_system.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_system_html);
  });

  /* route for temperature web page */
  server.on("/page_temperature.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get page_temperature.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", page_temperature_html);
  });

  /* route to license page */
  server.on("/license.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get license.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", license_html);
  });

  /* route to gtac page */
  server.on("/gtac.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get gtac.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", gtac_html);
  });

  /* route to privacy policy page */
  server.on("/privacypolicy.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get privacypolicy.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", privacypolicy_html);
  });

  /* route to cookie page */
  server.on("/cookie.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get cookie.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", cookies_html);
  });

  /* route to logs page */
  server.on("/get_logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_logs.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (true == SystemLog.GetCardDetectedStatus()) {
      request->send(SD_MMC, SystemLog.GetFilePath() + SystemLog.GetFileName(), "text/plain");
      //SystemLog.LogOpenFile();
    } else {
      request->send(404, "text/plain", "Micro SD card not found with FAT32 partition!");
    }
  });

  /* route to get the captured boot log */
  server.on("/get_bootlog", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_bootlog"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    request->send(200, "text/plain", SystemLog.GetBootLog());
  });

  /* route to get the captured error/warning log */
  server.on("/get_errorlog", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_errorlog"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    request->send(200, "text/plain", SystemLog.GetErrorLog());
  });

  /* route to list photo files on the SD card (timelapse folder) with links */
  server.on("/sd_files", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /sd_files"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (false == SystemLog.GetCardDetectedStatus()) {
      request->send(404, F("text/plain"), F("Micro SD card not found"));
      return;
    }

    /* Static page SHELL only - no SD access here, so it ALWAYS renders (buttons
       visible even if the card is huge or the scan misbehaves). The file list is
       fetched separately from /sd_list (capped) and filled in by JS. */
    request->send(200, F("text/html"), F(
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>SD files</title><style>body{font-family:sans-serif;margin:1em}"
      "a{color:#0a7;text-decoration:none}td,th{padding:3px 12px;text-align:left}"
      "tr:nth-child(even){background:#f3f3f3}"
      ".btn{padding:6px 14px;margin:4px 6px 4px 0;cursor:pointer}"
      "#msg{margin-left:8px;color:#0a7}</style></head><body>"
      "<h3>Files on SD card (/timelapse)</h3>"
      "<p><a href='/'>&larr; Back</a></p>"
      "<p><button class='btn' onclick='downloadSel()'>Download selected</button>"
      "<button class='btn' onclick='deleteSel()'>Delete selected</button>"
      "<button class='btn' onclick='deleteAll()'>Delete ALL</button>"
      "<span id='msg'></span></p>"
      "<table><thead><tr><th><input type='checkbox' id='all' onclick='toggleAll(this.checked)'></th>"
      "<th>File</th><th>Size</th></tr></thead><tbody id='rows'></tbody></table>"
      "<p id='cnt'>Loading file list...</p>"
      "<script>"
      "var FOLDER='/timelapse';"
      "function sel(){return Array.prototype.slice.call(document.querySelectorAll('.ck')).filter(function(c){return c.checked}).map(function(c){return c.value})}"
      "function toggleAll(v){document.querySelectorAll('.ck').forEach(function(c){c.checked=v})}"
      "function load(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){"
      "var rows=document.getElementById('rows'),cnt=document.getElementById('cnt');"
      "if(x.status!=200){cnt.innerText='SD card not available';return;}"
      "var t=x.responseText, lines=t.length?t.split('\\n'):[], html='', n=0, trunc=false;"
      "for(var i=0;i<lines.length;i++){if(!lines[i])continue;"
      "if(lines[i]=='TRUNC'){trunc=true;continue;}"
      "var p=lines[i].split('\\t'),nm=p[0],sz=p[1]||'';"
      "html+=\"<tr><td><input type='checkbox' class='ck' value='\"+nm+\"'></td><td><a href='/sd_get?path=\"+FOLDER+'/'+encodeURIComponent(nm)+\"'>\"+nm+\"</a></td><td align='right'>\"+sz+\" B</td></tr>\";n++;}"
      "rows.innerHTML=html;cnt.innerText=n+' file(s)'+(trunc?' (first '+n+' shown; use Delete ALL to clear the backlog)':'');}};"
      "x.open('GET','/sd_list',true);x.send();}"
      "function downloadSel(){var f=sel();if(!f.length){alert('No files selected');return}"
      "var i=0;(function next(){if(i>=f.length)return;var a=document.createElement('a');"
      "a.href='/sd_get?path='+FOLDER+'/'+encodeURIComponent(f[i]);a.download=f[i];"
      "document.body.appendChild(a);a.click();document.body.removeChild(a);i++;setTimeout(next,800)})()}"
      "function deleteSel(){var f=sel();if(!f.length){alert('No files selected');return}"
      "if(!confirm('Delete '+f.length+' file(s)? This cannot be undone.'))return;"
      "var m=document.getElementById('msg');m.innerText=' deleting...';"
      "var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){"
      "m.innerText=' '+x.responseText;setTimeout(function(){location.reload()},700)}};"
      "x.open('POST','/sd_delete',true);x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"
      "x.send('confirm=yes&files='+encodeURIComponent(f.join(',')))}"
      "function deleteAll(){if(!confirm('Delete ALL files in /timelapse? This cannot be undone.'))return;"
      "var m=document.getElementById('msg');m.innerText=' deleting all...';"
      "function round(){var x=new XMLHttpRequest();x.onreadystatechange=function(){if(x.readyState==4){"
      "m.innerText=' '+x.responseText;"
      "if(x.responseText.indexOf('more remain')>=0){round();}else{setTimeout(function(){location.reload()},700);}}};"
      "x.open('POST','/sd_delete_all',true);x.setRequestHeader('Content-Type','application/x-www-form-urlencoded');"
      "x.send('confirm=yes');}round();}"
      "load();"
      "</script></body></html>"));
  });

  /* data endpoint for the SD files page: capped, plain-text "name<TAB>size" per
     line (a final "TRUNC" line if the cap was hit). Kept small and separate so
     the page shell above always renders. */
  server.on("/sd_list", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /sd_list"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (false == SystemLog.GetCardDetectedStatus()) {
      request->send(404, F("text/plain"), F("no card"));
      return;
    }

    const uint16_t SD_LIST_MAX = 500;
    String out;
    out.reserve(20000);
    uint16_t count = 0;
    bool truncated = false;
    File root = SD_MMC.open(TIMELAPS_PHOTO_FOLDER);
    if (root && root.isDirectory()) {
      File f = root.openNextFile();
      while (f) {
        if (!f.isDirectory()) {
          String base = String(f.name());
          int slash = base.lastIndexOf('/');
          if (slash >= 0) base = base.substring(slash + 1);
          out += base;  out += '\t';  out += String(f.size());  out += '\n';
          count++;
          if (count >= SD_LIST_MAX) { truncated = true; break; }
        }
        f = root.openNextFile();
      }
    }
    if (truncated) { out += F("TRUNC\n"); }
    request->send(200, F("text/plain"), out);
  });

  /* route to delete ALL files in the timelapse folder (POST + confirm). Deletes
     in bounded batches so a huge backlog doesn't block the server too long; the
     client calls it repeatedly until nothing remains. */
  server.on("/sd_delete_all", HTTP_POST, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Warning, F("WEB server: /sd_delete_all"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (false == SystemLog.GetCardDetectedStatus()) {
      request->send(404, F("text/plain"), F("Micro SD card not found"));
      return;
    }
    if (!request->hasParam("confirm", true) || request->getParam("confirm", true)->value() != "yes") {
      request->send(400, F("text/plain"), F("Missing confirmation"));
      return;
    }

    String folder = TIMELAPS_PHOTO_FOLDER;
    const uint16_t BATCH = 200;   /* delete at most this many per request */
    uint16_t deleted = 0;
    bool more = false;
    File root = SD_MMC.open(folder.c_str());
    if (root && root.isDirectory()) {
      File f = root.openNextFile();
      while (f) {
        if (!f.isDirectory()) {
          String base = String(f.name());
          int slash = base.lastIndexOf('/');
          if (slash >= 0) base = base.substring(slash + 1);
          String full = folder + "/" + base;
          f.close();                       /* close before removing */
          if (SD_MMC.remove(full.c_str())) { deleted++; }
          if (deleted >= BATCH) { more = true; break; }
        }
        f = root.openNextFile();
      }
    }
    request->send(200, F("text/plain"),
                  "Deleted " + String(deleted) + (more ? F(" (more remain)") : F(" - done")));
  });

  /* route to delete selected files from the timelapse folder (POST + confirm) */
  server.on("/sd_delete", HTTP_POST, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Warning, F("WEB server: /sd_delete"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (false == SystemLog.GetCardDetectedStatus()) {
      request->send(404, F("text/plain"), F("Micro SD card not found"));
      return;
    }
    if (!request->hasParam("confirm", true) || request->getParam("confirm", true)->value() != "yes"
        || !request->hasParam("files", true)) {
      request->send(400, F("text/plain"), F("Missing confirmation or files"));
      return;
    }

    String folder = String(TIMELAPS_PHOTO_FOLDER) + "/";
    String list = request->getParam("files", true)->value();  /* comma-separated basenames */
    uint16_t deleted = 0, failed = 0;
    int start = 0;
    while (start < (int)list.length()) {
      int comma = list.indexOf(',', start);
      String name = (comma < 0) ? list.substring(start) : list.substring(start, comma);
      start = (comma < 0) ? list.length() : comma + 1;
      name.trim();
      /* security: basename only - reject anything with a path separator or traversal */
      if (name.length() == 0 || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) {
        continue;
      }
      String full = folder + name;
      if (SD_MMC.exists(full.c_str()) && SD_MMC.remove(full.c_str())) {
        deleted++;
      } else {
        failed++;
      }
    }
    request->send(200, F("text/plain"), "Deleted " + String(deleted) + " file(s)"
                  + (failed ? (", " + String(failed) + " failed") : String("")));
  });

  /* route to download a file from the SD card (restricted to the timelapse folder) */
  server.on("/sd_get", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /sd_get"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (false == SystemLog.GetCardDetectedStatus()) {
      request->send(404, F("text/plain"), F("Micro SD card not found"));
      return;
    }
    if (false == request->hasParam("path")) {
      request->send(400, F("text/plain"), F("Missing path"));
      return;
    }
    String path = request->getParam("path")->value();

    /* security: only files inside the timelapse folder, no path traversal */
    String folder = String(TIMELAPS_PHOTO_FOLDER) + "/";
    if ((false == path.startsWith(folder)) || (path.indexOf("..") >= 0)) {
      request->send(403, F("text/plain"), F("Forbidden path"));
      return;
    }
    if (false == SD_MMC.exists(path.c_str())) {
      request->send(404, F("text/plain"), F("File not found"));
      return;
    }

    String ct = F("application/octet-stream");
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) ct = F("image/jpeg");
    else if (path.endsWith(".avi")) ct = F("video/x-msvideo");

    request->send(SD_MMC, path, ct, true);  /* true = download as attachment */
  });

#if (defined(ENABLE_TIMELAPSE_VIDEO) && (true == ENABLE_TIMELAPSE_VIDEO))
  /* route to start/stop/query the timelapse MJPEG-AVI recording */
  server.on("/tl_video", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /tl_video"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("status")) {
      String s = TimelapseAvi.IsRecording()
                   ? ("recording (" + String(TimelapseAvi.GetFrameCount()) + " frames)")
                   : String("idle");
      request->send(200, F("text/plain"), s);
      return;
    }

    if (false == SystemLog.GetCardDetectedStatus()) {
      request->send(404, F("text/plain"), F("Micro SD card not found"));
      return;
    }

    if (request->hasArg("start")) {
      if (true == TimelapseAvi.IsRecording()) {
        request->send(200, F("text/plain"), "Already recording: " + TimelapseAvi.GetPath());
        return;
      }
      if (false == SystemLog.CheckDir(SD_MMC, TIMELAPS_PHOTO_FOLDER)) {
        SystemLog.CreateDir(SD_MMC, TIMELAPS_PHOTO_FOLDER);
      }
      String fn = String(TIMELAPS_PHOTO_FOLDER) + "/" + TIMELAPS_VIDEO_PREFIX + SystemLog.GetSystemTime() + TIMELAPS_VIDEO_SUFFIX;
      bool ok = TimelapseAvi.Begin(fn, SystemCamera.GetFrameSizeWidth(), SystemCamera.GetFrameSizeHeight(), TIMELAPS_VIDEO_FPS);
      request->send(200, F("text/plain"), ok ? ("Recording started: " + fn) : String("Failed to start recording"));

    } else if (request->hasArg("stop")) {
      bool ok = TimelapseAvi.End();
      request->send(200, F("text/plain"), ok ? ("Recording stopped: " + TimelapseAvi.GetPath() + " (" + String(TimelapseAvi.GetFrameCount()) + " frames)") : String("Not recording"));

    } else {
      request->send(400, F("text/plain"), F("Invalid request"));
    }
  });
#endif

#if (defined(ENABLE_PRUSALINK) && (true == ENABLE_PRUSALINK))
  /* route to read the current PrusaLink printer state (+ auto-timelapse flag) */
  server.on("/prusalink_status", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false)
      return;
    String s = PrusaLink_GetState() + (PrusaLink_GetAutoTimelapse() ? "  [auto-TL ON]" : "  [auto-TL OFF]");
    request->send(200, F("text/plain"), s);
  });

  /* route to get / set the PrusaLink printer IP + API key */
  server.on("/pl_config", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false)
      return;

    /* set: ip and/or key provided (blank key keeps the stored one) */
    if (request->hasParam("ip") || request->hasParam("key")) {
      String ip = request->hasParam("ip") ? request->getParam("ip")->value() : String("");
      String key = request->hasParam("key") ? request->getParam("key")->value() : String("");
      PrusaLink_SetConfig(ip, key);
      request->send(200, F("text/plain"), F("PrusaLink config saved"));
      return;
    }

    /* get: return "ip;keyset" so the form can prefill the IP and show key state */
    String s = PrusaLink_GetIp() + ";" + (PrusaLink_IsKeySet() ? "1" : "0");
    request->send(200, F("text/plain"), s);
  });

  /* route to enable/disable auto timelapse on print */
  server.on("/pl_auto", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (request->hasArg("on")) {
      PrusaLink_SetAutoTimelapse(true);
      request->send(200, F("text/plain"), F("Auto timelapse ON"));
    } else if (request->hasArg("off")) {
      PrusaLink_SetAutoTimelapse(false);
      request->send(200, F("text/plain"), F("Auto timelapse OFF"));
    } else {
      request->send(400, F("text/plain"), F("Invalid request"));
    }
  });
#endif

  /* route to enable/disable routine (high-frequency) log messages */
  server.on("/routine_log", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /routine_log"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.SetRoutineLogEnable(true);
      request->send(200, "text/plain", "Routine log ON");
    } else if (request->hasArg("off")) {
      SystemLog.SetRoutineLogEnable(false);
      request->send(200, "text/plain", "Routine log OFF");
    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* route to get temperature */
  server.on("/get_temp", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_temp"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    request->send(200, "text/plain", String(ExternalTemperatureSensor.GetTemperature()));
  });

  /* route to get humidity */
  server.on("/get_hum", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_hum"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    request->send(200, "text/plain", String(ExternalTemperatureSensor.GetHumidity()));
  });
}

/**
   @brief Init WEB server icons
   @param none
   @return none
*/
void Server_InitWebServer_Icons() {
  /* route to logo */
  server.on("/esp32_cam.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get esp32_cam.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", esp32_cam_logo_svg);
  });

  /* route to github icon */
  server.on("/github-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get github-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", github_icon_svg);
  });

  /* route to light on icon */
  server.on("/light-on-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get light-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", light_icon_on_svg);
  });

  /* route to light off icon */
  server.on("/light-off-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get light-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", light_icon_off_svg);
  });

  /* route to refresh icon */
  server.on("/refresh-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get refresh-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", refresh_icon_svg);
  });

  /* route to reboot icon */
  server.on("/reboot-icon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get reboot-icon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", reboot_icon_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-1.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-1.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_1_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-2.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-2.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_2_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-3.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-3.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_3_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-4.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-4.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_4_svg);
  });

  /* route to wifi icon */
  server.on("/wifi-icon-0.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get wifi-icon-0.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", wifi_icon_0_svg);
  });

  /* route to eye icon */
  server.on("/eye.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get eye.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", eye_svg);
  });

  /* route to eye-slash icon */
  server.on("/eye-slash.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get eye-slash.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", eye_slash_svg);
  });

  /* route to favicon */
  server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get favicon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", favicon_svg);
  });
}

/**
   @brief Init WEB server actions
   @param none
   @return none
*/
void Server_InitWebServer_Actions() {
  /*route for capture photo */
  server.on("/action_capture", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_capture Take photo"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    SystemCamera.CapturePhoto();
    request->send(200, "text/plain", "Take Photo");
  });

  /* route for send photo to prusa backend */
  server.on("/action_send", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_send send photo to cloud"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    Connect.SetSendingIntervalExpired();
    request->send(200, "text/plain", "Send Photo");
  });

  /* route for change LED status */
  server.on("/action_led", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_led Change LED status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    SystemCamera.SetFlashStatus(!SystemCamera.GetFlashStatus());
    SystemCamera.SetCameraFlashEnable(false);

    request->send(200, "text/plain", "Change LED status");
  });

  /* route for change LED status */
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /light set LED status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning light ON"));
      SystemCamera.SetFlashStatus(true);
      SystemCamera.SetCameraFlashEnable(false);
      request->send(200, "text/plain", "Light ON");

    } else if (request->hasArg("off")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning light OFF"));
      SystemCamera.SetFlashStatus(false);
      SystemCamera.SetCameraFlashEnable(false);
      request->send(200, "text/plain", "Light OFF");

    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* route for digital zoom (OV5640): /zoom?level=100..400, or query current */
  server.on("/zoom", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /zoom"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    /* reply is always "current,max" (percent) so the UI can bound the slider */
    if (request->hasParam("level")) {
      SystemCamera.SetZoom(request->getParam("level")->value().toInt());
    }
    request->send(200, F("text/plain"), String(SystemCamera.GetZoom()) + "," + String(SystemCamera.GetMaxZoom()));
  });

  /* route for pan/tilt of the zoom crop window: /pantilt?pan=0..100&tilt=0..100 */
  server.on("/pantilt", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /pantilt"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasParam("pan")) {
      SystemCamera.SetPan(request->getParam("pan")->value().toInt());
    }
    if (request->hasParam("tilt")) {
      SystemCamera.SetTilt(request->getParam("tilt")->value().toInt());
    }
    /* persist only on the final (drag-release) update to avoid flash wear */
    if (request->hasParam("save")) {
      SystemCamera.SavePanTilt();
    }
    /* reply "pan,tilt" (percent) */
    request->send(200, F("text/plain"), String(SystemCamera.GetPan()) + "," + String(SystemCamera.GetTilt()));
  });

  /* route to lock exposure/gain at the current auto level (stops AEC hunting) */
  server.on("/lock_exposure", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /lock_exposure"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool ok = SystemCamera.LockExposureToCurrent();
    request->send(ok ? 200 : 500, F("text/plain"),
                  ok ? F("Exposure locked to current level") : F("Exposure lock not supported"));
  });

  /* route to save the current camera image settings as a preset */
  server.on("/cam_save", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /cam_save"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    bool ok = SystemCamera.SaveSettingsPreset();
    request->send(ok ? 200 : 500, F("text/plain"),
                  ok ? F("Camera settings saved") : F("Save failed"));
  });

  /* route to restore the last saved camera image settings preset */
  server.on("/cam_load", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /cam_load"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    bool ok = SystemCamera.LoadSettingsPreset();
    request->send(ok ? 200 : 404, F("text/plain"),
                  ok ? F("Last saved settings loaded") : F("No saved settings yet"));
  });

  /* route to recover/reformat the SD card (destructive; POST + confirm) */
  server.on("/sd_format", HTTP_POST, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Warning, F("WEB server: /sd_format"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    /* require an explicit confirm so it can't be triggered by accident */
    if (!request->hasParam("confirm", true) || request->getParam("confirm", true)->value() != "yes") {
      request->send(400, F("text/plain"), F("Missing confirmation"));
      return;
    }

    bool ok = SystemLog.FormatCard();
    request->send(ok ? 200 : 500, F("text/plain"),
                  ok ? F("SD card recovered / reformatted OK") : F("SD recover failed - card not responding"));
  });

  /* route for change FLASH status */
  server.on("/flash", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /flash set flash status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning flash ON"));
      SystemCamera.SetCameraFlashEnable(true);
      SystemCamera.SetFlashStatus(false);
      request->send(200, "text/plain", "Flash ON");

    } else if (request->hasArg("off")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning flash OFF"));
      SystemCamera.SetCameraFlashEnable(false);
      SystemCamera.SetFlashStatus(false);
      request->send(200, "text/plain", "Flash OFF");

    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* reboot MCU */
  server.on("/action_reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_reboo reboot MCU!"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_REBOOT_MCU);
    delay(100); /* wait for sending data */
    ESP.restart();
  });

  /* route for change LED status */
  server.on("/action_sderase", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_sderase remove files from SD card"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    StartRemoveSdCard = 1;

    request->send(200, F("text/plain"), "Starting remove files from SD card");
  });

#if (defined(ENABLE_OV5640_AUTOFOCUS) && (true == ENABLE_OV5640_AUTOFOCUS))
  /* OV5640 autofocus: trigger a single focus cycle */
  server.on("/af_single", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /af_single"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (false == OV5640_AfAvailable()) {
      request->send(200, F("text/plain"), "Autofocus not available");
      return;
    }
    bool ok = OV5640_AfSingle();
    request->send(200, F("text/plain"), ok ? "Autofocus (single) triggered" : "Autofocus command failed");
  });

  /* OV5640 autofocus: enable continuous autofocus */
  server.on("/af_continuous", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /af_continuous"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (false == OV5640_AfAvailable()) {
      request->send(200, F("text/plain"), "Autofocus not available");
      return;
    }
    bool ok = OV5640_AfContinuous();
    request->send(200, F("text/plain"), ok ? "Continuous autofocus enabled" : "Autofocus command failed");
  });

  /* OV5640 autofocus: pause continuous autofocus */
  server.on("/af_pause", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /af_pause"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (false == OV5640_AfAvailable()) {
      request->send(200, F("text/plain"), "Autofocus not available");
      return;
    }
    bool ok = OV5640_AfPause();
    request->send(200, F("text/plain"), ok ? "Autofocus paused" : "Autofocus command failed");
  });

  /* OV5640 autofocus: release focus (VCM to infinity) */
  server.on("/af_release", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /af_release"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    if (false == OV5640_AfAvailable()) {
      request->send(200, F("text/plain"), "Autofocus not available");
      return;
    }
    bool ok = OV5640_AfRelease();
    request->send(200, F("text/plain"), ok ? "Autofocus released" : "Autofocus command failed");
  });

  /* OV5640 autofocus: read current AF status */
  server.on("/af_status", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /af_status"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), OV5640_AfStatusText());
  });
#endif
}

/**
   @brief Init WEB server sets
   @param none
   @return none
*/
void Server_InitWebServer_Sets() {
  /* route to set integer value */
  server.on("/set_int", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_int"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool response = false;
    String response_msg = "";

    /* set refresh interval */
    if (request->hasParam("refresh")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set refresh interval"));
      uint8_t value = request->getParam("refresh")->value().toInt();
      if ((value >= REFRESH_INTERVAL_MIN) && (value <= REFRESH_INTERVAL_MAX)) {
        Connect.SetRefreshInterval(value);
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = "ERROR! Bad value. Minimum is " + String(REFRESH_INTERVAL_MIN) + ", maximum " + String(REFRESH_INTERVAL_MAX) + " second";
      }
      response = true;
    }

    /* set saturation */
    if (request->hasParam("saturation")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set saturation"));
      SystemCamera.SetSaturation(request->getParam("saturation")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set contrast */
    if (request->hasParam("contrast")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set contrast"));
      SystemCamera.SetContrast(request->getParam("contrast")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set brightness */
    if (request->hasParam("brightness")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set brightness"));
      SystemCamera.SetBrightness(request->getParam("brightness")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set frame size */
    if (request->hasParam("framesize")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set framesize"));
      SystemCamera.SetFrameSize(request->getParam("framesize")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set photo quality */
    if (request->hasParam("photo_quality")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set photo_quality"));
      SystemCamera.SetPhotoQuality(73 - request->getParam("photo_quality")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set flash time */
    if (request->hasParam("flash_time")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set flash_time"));
      SystemCamera.SetCameraFlashTime(request->getParam("flash_time")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set white balancing mode */
    if (request->hasParam("wb_mode")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set wb_mode"));
      SystemCamera.SetAwbMode(request->getParam("wb_mode")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto exposition level */
    if (request->hasParam("ae_level")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set ae_level"));
      SystemCamera.SetAeLevel(request->getParam("ae_level")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto exposition controll value */
    if (request->hasParam("aec_value")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set aec_value"));
      SystemCamera.SetAecValue(request->getParam("aec_value")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto gain correction value */
    if (request->hasParam("agc_gain")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set agc_gain"));
      SystemCamera.SetAgcGain(request->getParam("agc_gain")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set image exif rotation */
    if (request->hasParam("image_rotation")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set image EXIF rotation"));
      SystemCamera.SetCameraImageRotation(request->getParam("image_rotation")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set log level /set_int?log_level=2 */
    if (request->hasParam("log_level")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set log_level"));
      LogLevel_enum level = (LogLevel_enum)request->getParam("log_level")->value().toInt();
      if ((level >= LogLevel_Error) && (level <= LogLevel_Verbose)) {
        SystemConfig.SaveLogLevel(level);
        SystemLog.SetLogLevel(level);
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = MSG_SAVE_NOTOK;
      }

      response = true;
    }

    /*  set network ip method. 0 - DHCP, 1 - Static */
    if (request->hasParam("ipcfg")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set ipcfg"));
      SystemWifiMngt.SetNetIpMethod(request->getParam("ipcfg")->value().toInt());
      response_msg = MSG_SAVE_OK_REBOOT;

      response = true;
    }

    /* set saturation */
    if (request->hasParam("temp_unit")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set temp_unit"));
      ExternalTemperatureSensor.SetUnit((TemperatureSensorUnit_enum) request->getParam("temp_unit")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    if (true == response) {
      request->send(200, F("text/html"), response_msg.c_str());
    }
  });

  /* route to set bool value */
  server.on("/set_bool", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_bool"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool response = false;

    /* check cfg for hmirror */
    if (request->hasParam("hmirror")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set hmirror"));
      SystemCamera.SetHMirror(Server_TransfeStringToBool(request->getParam("hmirror")->value()));
      response = true;
    }

    /* set vertical flip */
    if (request->hasParam("vflip")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set hmirror"));
      SystemCamera.SetVFlip(Server_TransfeStringToBool(request->getParam("vflip")->value()));
      response = true;
    }

    /* set lens correction */
    if (request->hasParam("lenc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set lensc"));
      SystemCamera.SetLensC(Server_TransfeStringToBool(request->getParam("lenc")->value()));
      response = true;
    }

    /* set exposure controll */
    if (request->hasParam("exposure_ctrl")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set exposure ctrl"));
      SystemCamera.SetExposureCtrl(Server_TransfeStringToBool(request->getParam("exposure_ctrl")->value()));
      response = true;
    }

    /* set auto white balancing */
    if (request->hasParam("awb")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set awb"));
      SystemCamera.SetAwb(Server_TransfeStringToBool(request->getParam("awb")->value()));
      response = true;
    }

    /* set auto white balancing gain */
    if (request->hasParam("awb_gain")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set awb_gain"));
      SystemCamera.SetAwbGain(Server_TransfeStringToBool(request->getParam("awb_gain")->value()));
      response = true;
    }

    /* set bad pixel correction */
    if (request->hasParam("bpc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set bpc"));
      SystemCamera.SetBpc(Server_TransfeStringToBool(request->getParam("bpc")->value()));
      response = true;
    }

    /* set white pixel correction */
    if (request->hasParam("wpc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set wpc"));
      SystemCamera.SetWpc(Server_TransfeStringToBool(request->getParam("wpc")->value()));
      response = true;
    }

    /* set raw gama correction */
    if (request->hasParam("raw_gama")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set raw_gama"));
      SystemCamera.SetRawGama(Server_TransfeStringToBool(request->getParam("raw_gama")->value()));
      response = true;
    }

    /* set automatic exposure correction */
    if (request->hasParam("aec2")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set aec2"));
      SystemCamera.SetAec2(Server_TransfeStringToBool(request->getParam("aec2")->value()));
      response = true;
    }

    /* set gain controll */
    if (request->hasParam("gain_ctrl")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set gain_ctrl"));
      SystemCamera.SetGainCtrl(Server_TransfeStringToBool(request->getParam("gain_ctrl")->value()));
      response = true;
    }

    /* set flash */
    if (request->hasParam("flash")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set flash"));
      SystemCamera.SetCameraFlashEnable(Server_TransfeStringToBool(request->getParam("flash")->value()));
      SystemCamera.SetFlashStatus(false);
      response = true;
    }

    /* set service AP */
    if (request->hasParam("serviceap_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set service AP enable"));
      SystemWifiMngt.SetEnableServiceAp(Server_TransfeStringToBool(request->getParam("serviceap_enable")->value()));
      response = true;
    }

    /* set timelaps enable */
    if (request->hasParam("timelaps_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set timelaps enable"));
#if (ENABLE_SD_CARD == true)
      bool val = Server_TransfeStringToBool(request->getParam("timelaps_enable")->value());
      if ((true == val) && (SystemLog.GetCardDetectedStatus() == true)) {
        Connect.SetTimeLapsPhotoSaveStatus(val);
      } else {
        Connect.SetTimeLapsPhotoSaveStatus(false);
      }
#else
        Connect.SetTimeLapsPhotoSaveStatus(false);
#endif
      response = true;
    }

    /* set external temperature sensor enable */
    if (request->hasParam("extsens_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set enable ext temperature"));
      ExternalTemperatureSensor.EnableSensor(Server_TransfeStringToBool(request->getParam("extsens_enable")->value()));
      response = true;
    }

    if (true == response) {
      request->send(200, F("text/html"), MSG_SAVE_OK);
    }
  });

  /* route for set token for authentification to prusa backend*/
  server.on("/set_token", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_token"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SAVE_OK);

    if (request->hasParam("token")) {
      Connect.SetToken(request->getParam("token")->value());
    }
  });

  /* route for set prusa connect hostname /set_hostname?hostname=*/
  server.on("/set_hostname", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_hostname"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SAVE_OK);

    if (request->hasParam("hostname")) {
      Connect.SetPrusaConnectHostname(request->getParam("hostname")->value());
    }
  });

  /* route for set WI-FI credentials */
  server.on("/wifi_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set WI-FI credentials"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String TmpPassword = "";
    String TmpSsid = "";
    /* get SSID */
    if (request->hasParam("wifi_ssid")) {
      TmpSsid = request->getParam("wifi_ssid")->value();
    }

    /* get password */
    if (request->hasParam("wifi_pass")) {
      TmpPassword = request->getParam("wifi_pass")->value();
    }

    /* check min and max length WI-FI ssid and password */
#if (WIFI_DISABLE_UNENCRYPTED_STA_PASS_CHECK == false)
    if (((TmpPassword.length() > 0) && (TmpSsid.length() > 0)) && ((TmpPassword.length() < EEPROM_ADDR_WIFI_PASSWORD_LENGTH) && (TmpSsid.length() < EEPROM_ADDR_WIFI_SSID_LENGTH))) {
#else
    if ((TmpSsid.length() > 0) && (TmpSsid.length() < EEPROM_ADDR_WIFI_SSID_LENGTH)) {
#endif

      /* send OK response */
      request->send(200, F("text/html"), MSG_SAVE_OK_WIFI);

      /* save ssid and password */
      SystemWifiMngt.SetStaCredentials(TmpSsid, TmpPassword);
      SystemWifiMngt.WiFiStaConnect();

    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });

  /* route for set WI-FI static IP address */
  server.on("/wifi_net_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set WI-FI static IP address"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String tmpIp = "";
    String tmpMask = "";
    String tmpGw = "";
    String tmpDns = "";

    /* get ip */
    if (request->hasParam("ip")) {
      tmpIp = request->getParam("ip")->value();
    }

    /* get mask */
    if (request->hasParam("mask")) {
      tmpMask = request->getParam("mask")->value();
    }

    /* get gw */
    if (request->hasParam("gw")) {
      tmpGw = request->getParam("gw")->value();
    }

    /* get dns */
    if (request->hasParam("dns")) {
      tmpDns = request->getParam("dns")->value();
    }

    /* check min and max length network parameters */
    if (((tmpIp.length() > 0) && (tmpMask.length() > 0) && (tmpGw.length() > 0) && (tmpDns.length() > 0)) && ((tmpIp.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpMask.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpGw.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpDns.length() <= IPV4_ADDR_MAX_LENGTH))) {

      /* save ssid and password */
      SystemWifiMngt.SetNetworkConfig(tmpIp, tmpMask, tmpGw, tmpDns);

      /* send OK response */
      request->send(200, F("text/html"), MSG_SAVE_OK_REBOOT);

    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });


  /* route for set basic auth */
  server.on("/basicauth_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set basic auth user name and password"));
    bool ret = false;
    String ret_msg = "";

    if (Server_CheckBasicAuth(request) == false)
      return;

    /* get username */
    if (request->hasParam("auth_username")) {
      WebBasicAuth.UserName = request->getParam("auth_username")->value();

      /* check min and max length  */
      if ((WebBasicAuth.UserName.length() > 0) && (WebBasicAuth.UserName.length() < EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH)) {
        SystemConfig.SaveBasicAuthUsername(WebBasicAuth.UserName);
        ret = true;
      } else {
        ret = false;
        ret_msg = "Maximum username length: " + String(EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH);
      }
    }

    /* get password */
    if (request->hasParam("auth_password")) {
      WebBasicAuth.Password = request->getParam("auth_password")->value();

      /* check min and max length  */
      if ((WebBasicAuth.Password.length() > 0) && (WebBasicAuth.Password.length() < EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH)) {
        SystemConfig.SaveBasicAuthPassword(WebBasicAuth.Password);
        ret = true;
      } else {
        ret = false;
        ret_msg = "Maximum password length: " + String(EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH);
      }
    }

    /* get enable / disable we bauth */
    if (request->hasParam("basicauth_enable")) {
      WebBasicAuth.EnableAuth = Server_TransfeStringToBool(request->getParam("basicauth_enable")->value());
      SystemConfig.SaveBasicAuthFlag(WebBasicAuth.EnableAuth);
      ret = true;
    }

    /* send OK response */
    if (true == ret) {
      request->send(200, F("text/html"), MSG_SAVE_OK);

    } else {
      String msg = MSG_SAVE_NOTOK;
      msg += " " + ret_msg;
      request->send(200, F("text/html"), msg.c_str());
    }
  });

  /* route for set firmware size */
  server.on("/set_firmware_size", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Info, F("WEB server: /set_firmware_size"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SAVE_OK);

    /* check cfg for flash */
    if (request->hasParam("size")) {
      SystemLog.AddEvent(LogLevel_Info, String(request->getParam("size")->value().toInt()));
      FirmwareUpdate.FirmwareSize = request->getParam("size")->value().toInt();
    }
  });

  /* route for set firmware size */
  server.on("/set_mdns", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_mdns"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    /* check cfg for mdns */
    if (request->hasParam("mdns")) {
      String tmp = request->getParam("mdns")->value();
      if (tmp.length() < EEPROM_ADDR_MDNS_RECORD_LENGTH) {
        request->send(200, F("text/html"), MSG_SAVE_OK_REBOOT);
        SystemWifiMngt.SetMdns(tmp);

      } else {
        String msg = "Error save mDNS. Maximum length: " + String(EEPROM_ADDR_MDNS_RECORD_LENGTH);
        request->send(200, F("text/html"), msg.c_str());
      }
    }
  });
}

/**
   @brief Init WEB FW update
   @param none
   @return none
*/
void Server_InitWebServer_Update() {
  System_UpdateInit();

  /* route for firmware update with file */
  server.on(
    "/upload", HTTP_POST, [](AsyncWebServerRequest* request) {
      String string_json = "";
      JsonDocument doc_json;

      String responseMessage = (Update.hasError()) ? SYSTEM_MSG_UPDATE_FAIL : SYSTEM_MSG_UPDATE_DONE;
      doc_json["message"] = responseMessage;
      FirmwareUpdate.UpdatingStatus = responseMessage;

      /* get error string */
      if (Update.hasError()) {
        String errorMessage = Update.errorString();
        doc_json["errorMessage"] = errorMessage;
        FirmwareUpdate.UpdatingStatus += errorMessage;
        FirmwareUpdate.Processing = false;
      }

      /* make and send json msg */
      serializeJson(doc_json, string_json);
      AsyncWebServerResponse* response = request->beginResponse(200, "application/json", string_json);
      response->addHeader("Connection", "close");
      request->send(response);
    },
    [](AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!index) {
        FirmwareUpdate.Processing = true;
        SystemLog.AddEvent(LogLevel_Info, F("Start FW update from file: "), filename);
        FirmwareUpdate.UpdatingStatus = String(SYSTEM_MSG_UPDATE_PROCESS);
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
          Update.printError(Serial);
        }
      }

      if (Update.write(data, len) != len) {
        Update.printError(Serial);
      }

      if (final) {
        FirmwareUpdate.Processing = false;
        if (Update.end(true)) {
          FirmwareUpdate.UpdatingStatus = String(SYSTEM_MSG_UPDATE_DONE);
          SystemLog.AddEvent(LogLevel_Info, F("Update FW from file done. Reboot MCU"));
        } else {
          Update.printError(Serial);
          SystemLog.AddEvent(LogLevel_Error, String(SYSTEM_MSG_UPDATE_FAIL));
        }
      }
    });

  /* route for start web OTA update from server */
  server.on("/web_ota_update", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Info, F("WEB server: /web_ota_update"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_UPDATE_START);
    FirmwareUpdate.Processing = true;

    /* check flag */
    if (request->hasParam("update")) {
      bool ret = Server_TransfeStringToBool(request->getParam("update")->value());
      if (ret == true) {
        FirmwareUpdate.StartOtaUpdate = true;
      } else {
        FirmwareUpdate.StartOtaUpdate = false;
      }
    }
  });

  /* get OTA FW version on the server */
  server.on("/check_web_ota_update", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /check_web_ota_update"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    System_CheckNewVersion();
    request->send(200, F("text/html"), FirmwareUpdate.CheckNewVersionFwStatus.c_str());
  });
}

/**
   @brief Init WEB server stream
   @param none
   @return none
*/
void Server_InitWebServer_Stream() {
  server.on("/stream.mjpg", HTTP_GET, Server_streamJpg);
}

/**
   @brief Pause WEB server
   @param none
   @return none
*/
void Server_pause() {
  server.end();
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: pause"));
}

/**
   @brief Resume WEB server
   @param none
   @return none
*/
void Server_resume() {
  server.begin();
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: resume"));
}

/**
 * @brief Handle cache request
 * 
 * @param AsyncWebServerRequest* - http request
 * @param const char* - content type
 * @param const char* - data
 */
void Server_handleCacheRequest(AsyncWebServerRequest* request, const char* contentType, const char* data) {
  /*
  AsyncWebServerResponse* response = request->beginResponse(200, contentType, data);
  response->addHeader("Cache-Control", "public, max-age=" + String(WEB_CACHE_INTERVAL));
  request->send(response); 
  */

  AsyncWebServerResponse* response = request->beginChunkedResponse(contentType, [data](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    const char* dataStart = data + index; // current position in data
    size_t dataLeft = strlen(data) - index; // how many bytes are left to send
    size_t chunkSize = dataLeft < maxLen ? dataLeft : maxLen; // how many bytes we can send now
    memcpy(buffer, dataStart, chunkSize); // copy chunk of data to buffer
    return chunkSize; // return chunk size
  });

  /* HTML pages change with the firmware, so don't cache them - otherwise a
     firmware update doesn't show until the browser cache expires (24h) or the
     user hard-refreshes. Static assets (JS/CSS/icons) keep the long cache. */
  if (strcmp(contentType, "text/html") == 0) {
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  } else {
    response->addHeader("Cache-Control", "public, max-age=" + String(WEB_CACHE_INTERVAL));
  }
  request->send(response);
}

/**
   @brief if the page was not found on ESP, then print which page is not there
   @param AsyncWebServerRequest* - request
   @return none
*/
void Server_handleNotFound(AsyncWebServerRequest* request) {
  String message = F("URL not Found\n\n");

  message += "URI: " + request->url() + "\nMethod: ";
  message += (request->method() == HTTP_GET) ? F("GET") : F("POST");
  message += "\nArguments: " + String(request->args()) + "\n";

  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }

  request->send(404, F("text/plain"), message);
}

/**
   @brief make json data for WEB page on the ESP32
   @param none
   @return String - json data
*/
String Server_GetJsonData() {
  String uptime = "";
  String string_json = "";
  JsonDocument doc_json;
  Server_GetModuleUptime(uptime);

  doc_json["token"] = Connect.GetToken();
  doc_json["fingerprint"] = Connect.GetFingerprint();
  doc_json["refreshInterval"] = String(Connect.GetRefreshInterval());
  doc_json["photoquality"] = String(73 - SystemCamera.GetPhotoQuality());
  doc_json["framesize"] = String(SystemCamera.GetFrameSize());
  doc_json["brightness"] = String(SystemCamera.GetBrightness());
  doc_json["contrast"] = String(SystemCamera.GetContrast());
  doc_json["saturation"] = String(SystemCamera.GetSaturation());
  doc_json["hmirror"] = Server_TranslateBoolToString(SystemCamera.GetHMirror());
  doc_json["vflip"] = Server_TranslateBoolToString(SystemCamera.GetVFlip());
  doc_json["lensc"] = Server_TranslateBoolToString(SystemCamera.GetLensC());
  doc_json["exposure_ctrl"] = Server_TranslateBoolToString(SystemCamera.GetExposureCtrl());
  doc_json["awb"] = Server_TranslateBoolToString(SystemCamera.GetAwb());
  doc_json["awb_gain"] = Server_TranslateBoolToString(SystemCamera.GetAwbGain());
  doc_json["wb_mode"] = String(SystemCamera.GetAwbMode());
  doc_json["bpc"] = Server_TranslateBoolToString(SystemCamera.GetBpc());
  doc_json["wpc"] = Server_TranslateBoolToString(SystemCamera.GetWpc());
  doc_json["raw_gama"] = Server_TranslateBoolToString(SystemCamera.GetRawGama());
  doc_json["aec2"] = Server_TranslateBoolToString(SystemCamera.GetAec2());
  doc_json["ae_level"] = SystemCamera.GetAeLevel();
  doc_json["aec_value"] = SystemCamera.GetAecValue();
  doc_json["gain_ctrl"] = Server_TranslateBoolToString(SystemCamera.GetGainCtrl());
  doc_json["agc_gain"] = SystemCamera.GetAgcGaint();
  doc_json["led"] = Server_TranslateBoolToString(SystemCamera.GetFlashStatus());
  doc_json["flash"] = Server_TranslateBoolToString(SystemCamera.GetCameraFlashEnable());
  doc_json["flash_time"] = SystemCamera.GetCameraFlashTime();
  doc_json["ssid"] = SystemWifiMngt.GetStaSsid();
  doc_json["bssid"] = SystemWifiMngt.GetStaBssid();
  doc_json["rssi"] = String(WiFi.RSSI());
  doc_json["rssi_percentage"] = String(SystemWifiMngt.Rssi2Percent(WiFi.RSSI()));
  doc_json["tx_power"] = SystemWifiMngt.TranslateTxPower(WiFi.getTxPower());
  doc_json["ip"] = WiFi.localIP().toString();
  doc_json["wifi_mode"] = SystemWifiMngt.GetWiFiMode();
  doc_json["mdns"] = SystemWifiMngt.GetMdns();
  doc_json["service_ap_ssid"] = SystemWifiMngt.GetServiceApSsid();
  doc_json["serviceap"] = Server_TranslateBoolToString(SystemWifiMngt.GetEnableServiceAp());
  doc_json["auth"] = Server_TranslateBoolToString(WebBasicAuth.EnableAuth);
  doc_json["auth_username"] = WebBasicAuth.UserName;
  doc_json["last_upload_status"] = Connect.GetBackendReceivedStatus();
  doc_json["wifi_network_status"] = SystemWifiMngt.GetStaStatus();
  doc_json["log_level"] = String(SystemLog.GetLogLevel());
  doc_json["uptime"] = uptime;
  doc_json["user_name"] = WebBasicAuth.UserName;
  doc_json["hostname"] = Connect.GetPrusaConnectHostname();
  doc_json["ip_cfg"] = SystemWifiMngt.GetNetIpMethod();
  doc_json["net_ip"] = SystemWifiMngt.GetNetStaticIp();
  doc_json["net_mask"] = SystemWifiMngt.GetNetStaticMask();
  doc_json["net_gw"] = SystemWifiMngt.GetNetStaticGateway();
  doc_json["net_dns"] = SystemWifiMngt.GetNetStaticDns();
  doc_json["image_rotation"] = SystemCamera.GetCameraImageRotation();
  doc_json["timelaps"] = Server_TranslateBoolToString(Connect.GetTimeLapsPhotoSaveStatus());
  doc_json["sd_status"] = (SystemLog.GetCardDetectedStatus() == true) ? F("Card detected")
                          : (SystemLog.GetCardMountFailed() == true) ? F("Corrupt - use Recover/Reformat below")
                          : F("No card detected");
  doc_json["sd_total"] = SystemLog.GetCardSizeMB();
  doc_json["sd_free_p"] = SystemLog.GetFreeSpacePercent();
  doc_json["sd_used_p"] = SystemLog.GetUsedSpacePercent();
  doc_json["mcu_temp"] = String(McuTemperature.TemperatureCelsius) + " *C";
  doc_json["sw_build"] = SW_BUILD;
  doc_json["sw_ver"] = SW_VERSION;
  doc_json["sw_variant"] = String(FW_VARIANT) + " (" + String(FW_VARIANT_VERSION) + ")";
  doc_json["routine_log"] = Server_TranslateBoolToString(SystemLog.GetRoutineLogEnable());
  doc_json["sw_new_ver"] = FirmwareUpdate.NewVersionFw;
  doc_json["extsens_stat"] = ExternalTemperatureSensor.GetSensorStatus();
  doc_json["extsen_en"] = ExternalTemperatureSensor.GetUserEnableSensor();
  doc_json["ext_temp"] = ExternalTemperatureSensor.GetTemperatureString();
  doc_json["ext_hum"] = ExternalTemperatureSensor.GetHumidityString();
  doc_json["exttemp_unit"] = ExternalTemperatureSensor.GetTemperatureUnit();

  serializeJson(doc_json, string_json);
  SystemLog.AddEvent(LogLevel_Verbose, string_json);
  return string_json;
}

/**
   @brief If basic auth is enabled, then check if the user is logged in
   @param AsyncWebServerRequest - request
   @return bool - status
*/
bool Server_CheckBasicAuth(AsyncWebServerRequest* request) {
  if ((!request->authenticate(WebBasicAuth.UserName.c_str(), WebBasicAuth.Password.c_str())) && (true == WebBasicAuth.EnableAuth)) {
    SystemLog.AddEvent(LogLevel_Verbose, F("Unauthorized! Sending longin request"));
    request->requestAuthentication();
    return false;
  }

  return true;
}

/**
   @brief Stream JPG image from camera
   @param AsyncWebServerRequest - request
   @return void
*/
void Server_streamJpg(AsyncWebServerRequest* request) {
  AsyncJpegStreamResponse* response = new AsyncJpegStreamResponse(&SystemCamera, &SystemLog);
  if (!response) {
    request->send(501);
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
   @brief Get module uptime
   @param String - uptime value
   @return void
*/
void Server_GetModuleUptime(String& readableTime) {
  unsigned long currentMillis;
  unsigned long seconds;
  unsigned long minutes;
  unsigned long hours;
  unsigned long days;

  currentMillis = millis();
  seconds = currentMillis / 1000;
  minutes = seconds / 60;
  hours = minutes / 60;
  days = hours / 24;
  currentMillis %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  readableTime = String(days) + " days, ";

  if (hours < 10) {
    readableTime += "0";
  }
  readableTime += String(hours) + ":";

  if (minutes < 10) {
    readableTime += "0";
  }
  readableTime += String(minutes) + ":";

  if (seconds < 10) {
    readableTime += "0";
  }
  readableTime += String(seconds);
}

/**
   @brief Convert string bool variable to bool
   @param String - data
   @return bool - status
*/
bool Server_TransfeStringToBool(String data) {
  bool ret = false;
  if (data.indexOf("true") >= 0) {
    ret = true;
  } else if (data.indexOf("false") >= 0) {
    ret = false;
  }

  return ret;
}

String Server_TranslateBoolToString(bool i_data) {
  String ret = "";
  if (true == i_data) {
    ret = "true";
  } else {
    ret = "";
  }

  return ret;
}

/* EOF */
