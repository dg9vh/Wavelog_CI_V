#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>

const int numParams = 4; // number of  parameters
String params[numParams] = {"", "", "", ""}; // initialization of parameters
WebServer server(80);
WebServer XMLRPCserver(12345);

unsigned long time_current_baseloop;
unsigned long time_last_baseloop;
#define BASELOOP_TICK 5000 // = 5 seconds

unsigned long frequency = 0;
float power = 0.0;
String mode_str = "";

unsigned long old_frequency = 0;
float old_power = 0.0;
String old_mode_str = "";

// ICOM coms variables
const byte TERM_address(0xE0); // Identifies the terminal (ESP32)
const byte startMarker = 0xFE;  // Indicates where the icom signal string starts
const byte endMarker = 0xFD;    // Indicates where the icom signal string ends

 const char* civ_options[][2] = { // List of names for select-options in config-page
    {"0", "IC-7300 (94h)"},
    {"1", "IC-7200 (76h)"},
    {"2", "IC-7100 (88h)"},
    {"3", "IC-7000 (70h)"},
    {"4", "IC-7410 (80h)"}
  };

const byte civ_addresses[] = { // corresponding list of CI-V-Adresses to list above
    0x94,
    0x76,
    0x88,
    0x70,
    0x80
  };

const int maxDataLength = 16;
byte receivedData[maxDataLength];  // Array for the recieved data bytes
int dataIndex = 0;                 // Pointer into the data array
bool newData2 = false;             // Set to true when a new data-array has been received
const uint8_t qrg_query[] = {0x03};
const uint8_t mode_query[] = {0x04};
const uint8_t power_query[] = {0x14, 0x0A};
size_t qrg_query_length = sizeof(qrg_query) / sizeof(qrg_query[0]);
size_t mode_query_length = sizeof(mode_query) / sizeof(mode_query[0]);
size_t power_query_length = sizeof(power_query) / sizeof(power_query[0]);

String buffer;
DynamicJsonDocument jsonDoc(512);

void connectToWifi() {
  // Create WiFiManager object
  WiFiManager wfm;
 
  // Supress Debug information
  wfm.setDebugOutput(false);
 
  // Remove any previous network settings
  //wfm.resetSettings();
 
  if (!wfm.autoConnect("Wavelog_CI_V_AP", "12345678")) { // Ad-Hoc-WLAN as fallback to configure production WLAN
    // Did not connect, print error message
    Serial.println("failed to connect and hit timeout");
 
    // Reset and try again
    ESP.restart();
    delay(1000);
  }
}

void sendCIVQuery(const uint8_t *commands, size_t length) {
  newData2 = false;
  Serial2.write(startMarker);  // always twice 0xFE in the beginning
  Serial2.write(startMarker);
  Serial2.write(civ_addresses[params[3].toInt()]);
  Serial2.write(TERM_address);
  Serial2.write(commands, length);
  Serial2.write(endMarker);

  Serial.println(F("Query sent"));
}

void getpower() {
  sendCIVQuery(power_query, power_query_length);
}

void getqrg() {
  sendCIVQuery(qrg_query, qrg_query_length);
}

void getmode() {
  sendCIVQuery(mode_query, mode_query_length);
}

void geticomdata() {
  while (Serial2.available() > 0 && newData2 == false) {
    byte currentByte = Serial2.read();
    if (currentByte == startMarker) {
      dataIndex = 0;
    } else if (currentByte == endMarker) {
      newData2 = true;
    } else {
      if (dataIndex < maxDataLength) {
        receivedData[dataIndex] = currentByte;
        dataIndex++;
      }
    }  
  }
}

int bcd2Dec(int bcdValue) {
    int units = bcdValue & 0xF;  // Untere 4 Bits
    int tens = (bcdValue >> 4) & 0xF;  // Obere 4 Bits

    return tens * 10 + units;
}

void calculateQRG() {
  uint8_t GHZ = 0, MHZ = 0, KHZ = 0, HZ = 0, mHZ = 0;
  uint32_t t_QRG = 0;
  GHZ = bcd2Dec(receivedData[7]);           // 1GHz & 100Mhz
  MHZ = bcd2Dec(receivedData[6]);           // 10Mhz & 1Mhz
  KHZ = bcd2Dec(receivedData[5]);           // 100Khz & 10KHz
  HZ = bcd2Dec(receivedData[4]);         // 1Khz & 100Hz
  
  t_QRG = ((GHZ * 1000000) + (MHZ * 10000) + (KHZ * 100) + (HZ * 1));  // QRG variable stores frequency in GMMMkkkH format - Frequ divided by 100
  frequency = t_QRG*100;
}

void calculateMode() {
  uint8_t mode_int = receivedData[3];
  switch (mode_int) {
    case 0:
      mode_str = "LSB";
      break;
    case 1:
      mode_str = "USB";
      break;
    case 2:
      mode_str = "AM";
      break;
    case 3:
      mode_str = "CW";
      break;
    case 4:
      mode_str = "RTTY";
      break;
    case 5:
      mode_str = "FM";
      break;
    case 6:
      mode_str = "WFM";
      break;
    case 7:
      mode_str = "CW";
      break;
    case 8:
      mode_str = "RTTY";
      break;
  }
}

void processReceivedData(void) {
  // first of all starting with the received command as debug on serial console output (e.g. Arduino Serial Monitor)
  Serial.print(F("RX: "));
  for (int i =0; i < maxDataLength; ++i) {
    if (receivedData[i] < 0X0F)
      Serial.print(F("0"));
    Serial.print(receivedData[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
    
  if ((receivedData[0] == 0x00 || receivedData[0] == 0xE0)) { // broadcast to all (0x00), reply to terminal (0xE0)
    switch(receivedData[2]) {
      case 0x0: { // reply at broadcast
        calculateQRG();
        break;
      }
      case 0x1: { // reply at broadcast
        calculateMode();
        break;
      }
      case 0x3: { // reply to terminal on query
        calculateQRG();
        break;
      }
      case 0x4: { // reply to terminal on query
        calculateMode();
        break;
      }
      case 0x14: {
        power = ((receivedData[4]) * 100 + bcd2Dec(receivedData[5])) / 2.55;
        break;
      }
    }
  }
}

void create_json(unsigned long frequency, String mode, float power) {  
  jsonDoc.clear();  
  jsonDoc["radio"] = String(civ_options[params[3].toInt()][1]) + " Wavelog CI-V";
  jsonDoc["frequency"] = frequency;
  jsonDoc["mode"] = mode;
  jsonDoc["power"] = power;
  jsonDoc["downlink_freq"] = 0;
  jsonDoc["uplink_freq"] = 0;
  jsonDoc["downlink_mode"] = 0;
  jsonDoc["uplink_mode"] = 0;
  jsonDoc["key"] = params[2];
  serializeJson(jsonDoc, buffer);
}

void post_json() {
  HTTPClient http;
    http.begin(params[0] + params[1]); // Endpoint of REST-API on the Wavelog-webserver

    // set content-type as JSON
    http.addHeader("Content-Type", "application/json");
    // send post-request with data
    Serial.println(buffer); // debug output of json-string
    int httpResponseCode = http.POST(buffer);

    if (httpResponseCode > 0) {
      Serial.print(F("HTTP-state: "));
      Serial.println(httpResponseCode);

      String response = http.getString();
      Serial.println(response);
      old_mode_str = mode_str;
      old_frequency = frequency;
      old_power = power;
    } else {
      Serial.print(F("Error on sending the post-request. HTTP-state: "));
      Serial.println(httpResponseCode);
    }
    http.end(); 
}

void handleRoot() {
  String html = "<!DOCTYPE html>";
  html += "<html>";
  html += "<head>";
  html += "<meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>ESP32 Configuration</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; }";
  html += ".container { max-width: 400px; margin: 0 auto; padding: 20px; }";
  html += "input[type='text'] { width: 100%; padding: 10px; margin: 5px 0; }";
  html += "input[type='submit'] { width: 100%; padding: 10px; margin-top: 10px; background-color: #4CAF50; color: white; border: none; }";
  html += ".btn-reboot { width: 100%; padding: 10px; margin-top: 10px; background-color: #FF0000; color: white; border: none; }";
  html += "input[type='submit']:hover { background-color: #45a049; }";
  html += "</style>";
  html += "</head>";
  html += "<body>";
  html += "<div class=\"container\">";
  html += "<h1>ESP32 Wavelog CI-V Configuration</h1>";
  html += "<form action='/save' method='post'>";
  html += "<label for='wavelogUrl'>Wavelog URL:</label><br>";
  html += "<input type='text' id='wavelogUrl' name='wavelogUrl' value='" + params[0] + "'><br>";
  html += "<label for='wavelogApiEndpoint'>Wavelog API Endpoint:</label><br>";
  html += "<input type='text' id='wavelogApiEndpoint' name='wavelogApiEndpoint' value='" + params[1] + "'><br>";
  html += "<label for='wavelogApiKey'>Wavelog API Key:</label><br>";
  html += "<input type='text' id='wavelogApiKey' name='wavelogApiKey' value='" + params[2] + "'><br>";
  html += "<label for='TrxAddress'>Trx Address:</label><br>";
  html += "<select id='TrxAddress' name='TrxAddress'>";

  for (int i = 0; i < sizeof(civ_options) / sizeof(civ_options[0]); ++i) {
    String optionHTML = "<option value='" + String(civ_options[i][0]) + "'";
    optionHTML += (params[3].toInt() == atoi(civ_options[i][0]) ? " selected" : "");
    optionHTML += ">" + String(civ_options[i][1]) + "</option>";
    html += optionHTML;
  }

  html += "</select><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form>";
  html += "<form action='/reboot' method='post'>";
  html += "<button type='submit' class='btn-reboot'>Reboot ESP32</button>";
  html += "</form>";
  html += "</div>";
  html += "</body>";
  html += "</html>";

  server.send(200, "text/html", html);
}

void loadParametersFromSPIFFS() {
  for (int i = 0; i < numParams; ++i) {
    File file = SPIFFS.open("/param" + String(i) + ".txt", "r");
    if (!file) {
      Serial.println("Failed to open file for reading");
      return;
    }
    params[i] = file.readStringUntil('\n');
    file.close();
  }
}

void saveParametersToSPIFFS() {
  for (int i = 0; i < numParams; ++i) {
    File file = SPIFFS.open("/param" + String(i) + ".txt", "w");
    if (!file) {
      Serial.println("Failed to open file for writing");
      return;
    }
    file.print(params[i]);
    file.close();
  }
}

void handleSave() {
  params[0] = server.arg("wavelogUrl");
  params[1] = server.arg("wavelogApiEndpoint");
  params[2] = server.arg("wavelogApiKey");
  params[3] = server.arg("TrxAddress");

  saveParametersToSPIFFS();

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void handleReboot() {
    ESP.restart();
}

String rig_get_vfo() {
  return String(frequency);
}

String rig_get_mode() {
  return mode_str;
}

void handleRPC() {
  // processing XML-RPC-request
  if (XMLRPCserver.arg(0).indexOf("<methodName>rig.get_vfo</methodName>") != -1) {
    String response = rig_get_vfo();
    XMLRPCserver.send(200, "text/xml", "<methodResponse><params><param><value>" + response + "</value></param></params></methodResponse>");
  } else if (XMLRPCserver.arg(0).indexOf("<methodName>rig.get_mode</methodName>") != -1) {
    String response = rig_get_mode();
    XMLRPCserver.send(200, "text/xml", "<methodResponse><params><param><value>" + response + "</value></param></params></methodResponse>");
  } else {
    XMLRPCserver.send(400, "text/plain", "Unknown method");
  }
}

void setup() {
  Serial.begin(115200);  // Serial console
  Serial2.begin(19200, SERIAL_8N1, 16, 17);  // ICOM CI-V
  delay(1000);
  Serial.println(F(""));
  Serial.println(F("Booting Sketch..."));
  MDNS.begin("esp32-ci-v");

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS initialization failed!");
    return;
  }
  
  loadParametersFromSPIFFS();

  Serial.print(F("URL: "));
  Serial.println(params[0]);
  Serial.print(F("Endpoint: "));
  Serial.println(params[1]);
  Serial.print(F("API-Key: "));
  Serial.println(params[2]);
  Serial.print(F("CI-V-Address: "));
  Serial.println(params[3]);
  connectToWifi();

  Serial.print(F("IP-Address: "));
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.on("/reboot", handleReboot);
  server.begin();
  Serial.println("HTTP server started");

  // Routing for XML-RPC-requests
  XMLRPCserver.on("/", handleRPC);
  XMLRPCserver.begin();
  Serial.println("XMLRPCserver started");

  delay(1000);
  time_current_baseloop = millis();
  time_last_baseloop = time_current_baseloop;

  if (params[0].length() > 0) {
    getmode();
    if (params[3].toInt() >= 0) 
      geticomdata();
    if (newData2) {
      processReceivedData();
    }
    newData2 = false;
    delay(1000);
    getqrg();
    if (params[3].toInt() >= 0) 
      geticomdata();
    if (newData2) {
      processReceivedData();
    }
    newData2 = false;
    create_json(frequency, mode_str, power);
    post_json();
  }
}

void loop() {
  server.handleClient();
  XMLRPCserver.handleClient();
  if (WiFi.status() == WL_CONNECTED) {
    time_current_baseloop = millis();

    //query tx-power every 5 secs
    if ((time_current_baseloop - time_last_baseloop) > BASELOOP_TICK) {
      getpower();
      time_last_baseloop = time_current_baseloop;
    }
    
    if (params[3].toInt() >= 0) 
      geticomdata();
    if (newData2) {
      processReceivedData();
    }
    newData2 = false;
  
    delay(10); 
    if ( mode_str != old_mode_str || frequency != old_frequency || power != old_power) {
      // send json to server
      create_json(frequency, mode_str, power);
      post_json();
    }
  } else {
    Serial.println(F("No connection to your WiFi-network."));
    connectToWifi();
  }
} // end loop
