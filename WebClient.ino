void callHome() {
  if (WiFi.status() != WL_CONNECTED) {
    addLog(LOG_LEVEL_ERROR, "WEBCL: Not calling home: Not connected to WiFi");
    return;
  }
  addLog(LOG_LEVEL_INFO, "WEBCL: Calling home");
  
  updateHTML();

  String cfgData = httpsGet("/api/callhome/", "&ver=" + String(version) + "&uptime=" + String(millis() / 1000) + "&free=" + String(system_get_free_heap_size()) + "&rssi=" + getWiFiStrength(10) + "&ip=" + formatIP(WiFi.localIP()));
  String returnValue;
  bool settingsChanged = 0;
  bool returnBool;
  
  // Process commands from server
  returnValue = getVarFromString("Command:", cfgData);
  if (returnValue != "") {
    if (returnValue.equalsIgnoreCase("reboot")) {
      addLog(LOG_LEVEL_INFO, "CORE : Reboot request from server, rebooting");
      ESP.restart();
    }
    if (returnValue.equalsIgnoreCase("log")) {
      addLog(LOG_LEVEL_INFO, "CORE : Upload log request from server");
      // logging to SD not implemented
    }
  }

  // get desired log level from server
  logLevel = getVarFromString("Loglevel:", cfgData).toInt();

  // check DST
  bool WAS_DST = Settings.DST;
  returnValue = getVarFromString("DST:", cfgData);
  if(returnValue == "1") {
    Settings.DST = 1;
    if(WAS_DST == 0) {
      addLog(LOG_LEVEL_INFO, "WEBCL: Server reports it is DST, forwarding the clock 1 hour");
      setTime(sysTime + SECS_PER_HOUR);
      settingsChanged = 1;
    }
  } else {
    Settings.DST = 0;
    if(WAS_DST == 1) {
      addLog(LOG_LEVEL_INFO, "WEBCL: Server reports it is not DST, reversing the clock 1 hour");
      setTime(sysTime - SECS_PER_HOUR);
      settingsChanged = 1;
    }
  }

  if(settingsChanged) {
    addLog(LOG_LEVEL_INFO, "FILE : Settings changed, saving new settings");
    SaveSettings();
  }

  // check version
  returnValue = getVarFromString("Version:", cfgData);
  if (returnValue != "") {
    float srvVer = returnValue.toFloat();
    if (srvVer == 0) {
      addLog(LOG_LEVEL_INFO, "OTA  : No software available from server");
      return;
    }
    addLog(LOG_LEVEL_DEBUG, "OTA  : SW version available on server: " + String(srvVer));
    if (version == srvVer) {
      addLog(LOG_LEVEL_INFO, "OTA  : SW version up to date");
    }
    if (version < srvVer) {
      addLog(LOG_LEVEL_INFO, "OTA  : New version available");
      OTA();
    }
    if (version > srvVer && srvVer > 1) {
      addLog(LOG_LEVEL_INFO, "OTA  : Forced downgrade");
      OTA();
    }
  }
}

String getVarFromString(String var, String cfgData) {
  String line;
  String curChar;
  String foundValue;
  int datalen = cfgData.length();
  for (int i = 0; i < datalen; i++) {
    curChar = cfgData.substring(i, i + 1);
    if (curChar != "\n" && curChar != "\r") {
      line += curChar;
    } else {
      if (line.startsWith(var)) {
        foundValue = line.substring(var.length());
        foundValue.trim();
        return (foundValue);
      }
      line = "";
    }
  }
  return ("");
}

String urlOpen(String path, String query) {
  if (path.startsWith("https://")) {
    return (httpsGet(path, query));
  } else if (path.startsWith("http://")) {
    return (httpGet(path, query));
  } else {
    addLog(LOG_LEVEL_ERROR, "WEBCL: Unsupported URL: " + path);
  }
}

String httpsGet(String path, String query) {
  if (WiFi.status() != WL_CONNECTED) {
    addLog(LOG_LEVEL_ERROR, "WEBCL: Unable to connect: Not connected to WiFi");
  }
  WiFiClientSecure webclient;
  int port = 443;
  addLog(LOG_LEVEL_DEBUG, "WEBCL: Starting https request (" + path + ")" );
  unsigned int contentLength = 0;
  unsigned int contentPos = 0;
  String response = "";
  String responseCode = "";
  char* server = DEFAULT_LOG_HOST;
  query = "id=" + String(chipMAC) + "&" + query;
  String url = "https://" + String(server) + path + "?" + query;
  if (!webclient.connect(server, port))
    addLog(LOG_LEVEL_INFO, "WEBCL: Connection to " + String(server) + " failed!");
  else {
    addLog(LOG_LEVEL_DEBUG, "WEBCL: Connected to " + String(server));
    webclient.println("GET " + url + " HTTP/1.1");
    webclient.println("Host: " + String(server));
    webclient.println("Connection: close");
    webclient.println();
    String line;
    while (webclient.connected()) {
      line = webclient.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
      if (line.startsWith("Content-Length: ")) {
        contentLength = line.substring(16).toInt();
      }
      if (line.startsWith("HTTP/")) {
        responseCode = line.substring(9, 12);
      }
    }
    line = "";
    while (webclient.available() && contentPos < contentLength) {
      char c = webclient.read();
      contentPos++;
      response += String(c);
    }
    webclient.stop();
  }
  if (responseCode == "200") {
    addLog(LOG_LEVEL_DEBUG, "WEBCL: Request done ");
    return (response);
  } else {
    addLog(LOG_LEVEL_ERROR, "WEBCL: Response code: " + String(responseCode));
    return responseCode;
  }
}

String httpGet(String path, String query) {
  if (WiFi.status() != WL_CONNECTED) {
    addLog(LOG_LEVEL_ERROR, "WEBCL: Unable to connect: Not connected to WiFi");
  }
  WiFiClient webclient;
  int port = 80;
  addLog(LOG_LEVEL_DEBUG, "WEBCL: Starting https request (" + path + ")" );
  unsigned int contentLength = 0;
  unsigned int contentPos = 0;
  String response = "";
  String responseCode = "";
  char* server = DEFAULT_LOG_HOST;
  query = "id=" + String(chipMAC) + "&" + query;
  String url = "http://" + String(server) + path + "?" + query;
  if (!webclient.connect(server, port))
    addLog(LOG_LEVEL_INFO, "WEBCL: Connection to " + String(server) + " failed!");
  else {
    addLog(LOG_LEVEL_DEBUG, "WEBCL: Connected to " + String(server));
    webclient.println("GET " + url + " HTTP/1.1");
    webclient.println("Host: " + String(server));
    webclient.println("Connection: close");
    webclient.println();
    String line;
    while (webclient.connected()) {
      line = webclient.readStringUntil('\n');
      if (line == "\r") {
        break;
      }
      if (line.startsWith("Content-Length: ")) {
        contentLength = line.substring(16).toInt();
      }
      if (line.startsWith("HTTP/")) {
        responseCode = line.substring(9, 12);
      }
    }
    line = "";
    while (webclient.available() && contentPos < contentLength) {
      char c = webclient.read();
      contentPos++;
      response += String(c);
    }
    webclient.stop();
  }
  if (responseCode == "200") {
    return (response);
  } else {
    addLog(LOG_LEVEL_ERROR, "WEBCL: Response code: " + String(responseCode));
    return responseCode;
  }
}

