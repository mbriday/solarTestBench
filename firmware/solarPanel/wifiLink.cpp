#include "LittleFS.h"
#include "wifiLink.h"
#include <functional>

WebServer server(80);
WebSocketsServer webSocket(81);

wifiLink::wifiLink() {
  m_instance = this;
}

String wifiLink::getContentType(String filename) {
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  return "text/plain";
}

void wifiLink::handleFile(String path) {
  if (LittleFS.exists(path)) {
    File file = LittleFS.open(path, "r");
    String contentType = wifiLink::getContentType(path);
    server.streamFile(file, contentType);
    file.close();
  } else {
    server.send(404, "text/plain", "File not found");
  }
}

void wifiLink::listFiles(uint8_t client) {
  String fileList = "FILES:[";
  File root = LittleFS.open("/", "r");
  File file = root.openNextFile();

  while (file) {
    if (fileList.length() > 7)
      fileList += ",";
    fileList += "{\"name\":\"" + String(file.name()) + "\",\"size\":" + String(file.size()) + "}";
    file = root.openNextFile();
  }

  fileList += "]";
  webSocket.sendTXT(client, fileList);
}

String wifiLink::getFilesystemInfo() {
  FSInfo info;
  LittleFS.info(info);

  size_t freeBytes = info.totalBytes - info.usedBytes;      // Free space

  String json = "{";
  json += "\"total\":" + String((size_t)info.totalBytes) + ",";
  json += "\"used\":" + String((size_t)info.usedBytes) + ",";
  json += "\"free\":" + String(freeBytes);
  json += "}";

  return json;
}

void wifiLink::sendLog(String message) {
  String logMessage = "LOG:" + message;
  webSocket.broadcastTXT(logMessage);
}

void wifiLink::sendFile(uint8_t client, String fileName) {
  File file = LittleFS.open("/" + fileName, "r");
  if (!file) {
    webSocket.sendTXT(client, "ERROR: File not found");
    return;
  }
  sendLog("start downloading " + fileName);
  webSocket.sendTXT(client, "START_DOWNLOAD:" + fileName);

  while (file.available()) {
    uint8_t buffer[512];
    size_t len = file.read(buffer, sizeof(buffer));
    webSocket.sendBIN(client, buffer, len);
    sendLog("sent " + String(len) + " bytes");
  }

  webSocket.sendTXT(client, "END_DOWNLOAD:" + fileName);
  sendLog("end downloading " + fileName);

  file.close();
}

void wifiLink::webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String command = (char *)payload;
    if (command == "LIST") {
      listFiles(num);
    } else if (command.startsWith("GET:")) {
      String fileName = command.substring(4);
      sendFile(num, fileName);
    } else if (command.startsWith("UPLOAD:")) {
      m_filename = command.substring(7);
      sendLog(String("start uploading ") + m_filename);
      if (m_uploadFile)
        m_uploadFile.close();
      m_uploadFile = LittleFS.open("/" + m_filename, "w");
      m_isUploading = true;
      m_uploadSize = 0;
    } else if (command.startsWith("UPLOAD_DONE")) {
      sendLog("uploading done ("+m_filename+")");
      if (m_uploadFile)
        m_uploadFile.close();
      m_isUploading = false;
      if (m_userUploadEvent) m_userUploadEvent(num, m_filename);

    } else if (command.startsWith("DELETE:")) {
      String fileName = command.substring(7);
      deleteFile(num, "/" + fileName);
    } else if (command.startsWith("GET_FS_INFO")) {
      webSocket.sendTXT(num, "FS_INFO:" + getFilesystemInfo());
    } else {
      if (m_userEvent)
        m_userEvent(num, type, payload, length);
      else {
        webSocket.sendTXT(num, "unknown:" + command);
      }
    }
  } else if (type == WStype_BIN) {
    if (m_isUploading && m_uploadFile) {
      m_uploadFile.write(payload, length);
      m_uploadSize += length;
      webSocket.sendTXT(num,"UPLOADED_SIZE:"+String(m_uploadSize));
      sendLog("UPLOADED_SIZE:"+String(m_uploadSize));
    }
  }
}

void wifiLink::sendTXT(String message){
  webSocket.broadcastTXT(message);
}


bool wifiLink::deleteFile(uint8_t num, String filename) {
  if (LittleFS.remove(filename)) {
    webSocket.sendTXT(num, "DELETED:" + filename);
    sendLog("file deleted : " + filename);
    return true;
  } else {
    sendLog("delete failed : " + filename);
    return false;
  }
}

void wifiLink::debugMsg(String msg) {
  if (Serial)
    Serial.print(msg);
}

bool wifiLink::setup(const String &ssid, const String &psk) {
  // Access point mode
  WiFi.softAP(ssid, psk);
  IPAddress IP = WiFi.softAPIP();
  debugMsg("AP IP address: " + IP.toString() + "\n");

  // Initialize LittleFS
  if (!LittleFS.begin()) {
    debugMsg("Error!! LittleFS\n");
    return -1;
  }
  debugMsg("LittleFS ok => ");
  debugMsg(getFilesystemInfo());
  debugMsg("\n");

  // Configure the HTTP server
  server.on("/", []() {
    //Serial.print("Server access to root (index.html)");
    wifiLink::handleFile("/index.html");
  });
  server.onNotFound([]() {
    //Serial.print("Server callback 'not found'!\n");
    String path = server.uri();
    wifiLink::handleFile(path);
    // TODO: security!
  });
  server.begin();
  debugMsg("HTTP server ok.\n");

  // Start the WebSocket server
  webSocket.begin();
  webSocket.onEvent(std::bind(&wifiLink::webSocketEvent, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

  debugMsg("Web socket ok.\n");
  return 0;
}

void wifiLink::onEvent(wifiLinkEvent cbEvent) {
  m_userEvent = cbEvent;
}

void wifiLink::onUploadEvent(wifiLinkUploadEvent cbEvent) {
  m_userUploadEvent = cbEvent;
}

void wifiLink::loop() {
  server.handleClient();
  webSocket.loop();
}
