#ifndef __wifiLink_h__
#define __wifiLink_h__

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>

/**
Class to communicate with a Web interface. There must be only one instance of the object.
WifiLink configures itselft as an access point:
 - with an HTTP server on port 80
 - a websocket on port 81
The Web socket is used for file transfer in both directions: upload/download.
To add callbacks to the websocket, simply use a callback with onEvent

Files are accessed using the LittleFS file system
*/

class wifiLink {
public:
  wifiLink();
  bool setup(const String& ssid, const String& psk = "");
  // main loop for the web server and the websocket
  void loop();
  // send a log message to the client (browser)
  void sendLog(String message);

  typedef std::function<void(uint8_t num, WStype_t type, uint8_t* payload, size_t length)> wifiLinkEvent;
  typedef std::function<void(uint8_t num, String filename)> wifiLinkUploadEvent;

  // Addition of a user defined event (not related to files) for the websocket.
  void onEvent(wifiLinkEvent cbEvent);
  // addition of a user defined event for the upload of a file
  void onUploadEvent(wifiLinkUploadEvent cbEvent);
  // delete a file (true in case of success)
  bool deleteFile(uint8_t num, String filename);
  // send a command to all client (browser)
  void sendTXT(String message);
private:
  File m_uploadFile;
  bool m_isUploading = false;
  String m_filename; //name of the file uploaded
  size_t m_uploadSize;
  wifiLinkEvent m_userEvent = NULL;
  wifiLinkUploadEvent m_userUploadEvent = NULL;
  wifiLink* m_instance;

  static String getContentType(String filename);
  static void handleFile(String path);

  void listFiles(uint8_t client);
  void sendFile(uint8_t client, String fileName);
  String getFilesystemInfo();

  void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
  void debugMsg(String msg);
};


#endif