#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include "dsmr.h"
#include "config.h"
/* 
python ~/.arduino15/packages/esp8266/hardware/esp8266/2.3.0/tools/espota.py -i smartmeter.local -r -f ~/Dropbox/Arduino/Sketches/SmartMeterESP/SmartMeterESP.ino.generic.bin
*/

WiFiUDP udp;
IPAddress myIP;

unsigned long previousMillis = 0;
const long interval = 500;
char rawbuf[1024];
uint16_t buf_idx = 0;

enum states {
  WAITING,
  READING,
  PROCESSING
} state = WAITING;

/**
   Define the data we're interested in, as well as the datastructure to
   hold the parsed data. This list shows all supported fields, remove
   any fields you are not using from the below list to make the parsing
   and printing code smaller.
   Each template argument below results in a field of the same name.
*/
using MyData = ParsedData <
               /* String */ identification,
               /* String */ p1_version,
               /* String */ timestamp,
               /* String */ equipment_id,
               /* FixedValue */ energy_delivered_tariff1,
               /* FixedValue */ energy_delivered_tariff2,
               /* FixedValue */ energy_returned_tariff1,
               /* FixedValue */ energy_returned_tariff2,
               /* String */ electricity_tariff,
               /* FixedValue */ power_delivered,
               /* FixedValue */ power_returned,
               /* FixedValue */ electricity_threshold,
               /* uint8_t */ electricity_switch_position,
               /* uint32_t */ electricity_failures,
               /* uint32_t */ electricity_long_failures,
               /* String */ electricity_failure_log,
               /* uint32_t */ electricity_sags_l1,
               /* uint32_t */ electricity_sags_l2,
               /* uint32_t */ electricity_sags_l3,
               /* uint32_t */ electricity_swells_l1,
               /* uint32_t */ electricity_swells_l2,
               /* uint32_t */ electricity_swells_l3,
               /* String */ message_short,
               /* String */ message_long,
               /* FixedValue */ voltage_l1,
               /* FixedValue */ voltage_l2,
               /* FixedValue */ voltage_l3,
               /* FixedValue */ current_l1,
               /* FixedValue */ current_l2,
               /* FixedValue */ current_l3,
               /* FixedValue */ power_delivered_l1,
               /* FixedValue */ power_delivered_l2,
               /* FixedValue */ power_delivered_l3,
               /* FixedValue */ power_returned_l1,
               /* FixedValue */ power_returned_l2,
               /* FixedValue */ power_returned_l3,
               /* uint16_t */ gas_device_type,
               /* String */ gas_equipment_id,
               /* uint8_t */ gas_valve_position,
               /* TimestampedFixedValue */ gas_delivered,
               /* uint16_t */ thermal_device_type,
               /* String */ thermal_equipment_id,
               /* uint8_t */ thermal_valve_position,
               /* TimestampedFixedValue */ thermal_delivered,
               /* uint16_t */ water_device_type,
               /* String */ water_equipment_id,
               /* uint8_t */ water_valve_position,
               /* TimestampedFixedValue */ water_delivered,
               /* uint16_t */ slave_device_type,
               /* String */ slave_equipment_id,
               /* uint8_t */ slave_valve_position,
               /* TimestampedFixedValue */ slave_delivered
               >;

/**
   This illustrates looping over all parsed fields using the
   ParsedData::applyEach method.

   When passed an instance of this Printer object, applyEach will loop
   over each field and call Printer::apply, passing a reference to each
   field in turn. This passes the actual field object, not the field
   value, so each call to Printer::apply will have a differently typed
   parameter.

   For this reason, Printer::apply is a template, resulting in one
   distinct apply method for each field used. This allows looking up
   things like Item::name, which is different for every field type,
   without having to resort to virtual method calls (which result in
   extra storage usage). The tradeoff is here that there is more code
   generated (but due to compiler inlining, it's pretty much the same as
   if you just manually printed all field names and values (with no
   cost at all if you don't use the Printer).
*/
struct Printer {
  template<typename Item>
  void apply(Item &i) {
    if (i.present()) {
      udp.print(Item::name);
      udp.print(F(": "));
      udp.print(i.val());
      udp.print(Item::unit());
      udp.println();
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("smartmeter");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  myIP = WiFi.localIP();

  udp.begin(1234);
}

void sendDataToHost(MyData data) {
  String power = String(data.power_delivered*1000);
  String url = e_url;
  url += "{";
  url += "power:";
  url += power.c_str();
  url += ",tariff_one:";
  url += String(data.energy_delivered_tariff1).c_str();
  url += ",tariff_two:";
  url += String(data.energy_delivered_tariff2).c_str();
  url += ",gas:";
  url += String(data.gas_delivered).c_str();
  url += "}&apikey=";
  url += apikey.c_str();

  //HTTPClient http;
  //http.begin(url);
  //int httpCode = http.GET();
  //http.end();
  
  // This will send the request to the server
  WiFiClientSecure client;
  if (!client.connect(host, httpsPort)) {
    udp.print("FAIL");
    return;
  }
  if (client.verify(fingerprint, host)) {
    //udp.println("certificate matches");
    client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
     // Handle wait for reply and timeout
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        udp.println(">>> Client Timeout !");
        client.stop();
        return;
      }
    }
    // Handle message receive
    while(client.available()){
      String line = client.readStringUntil('\r');
      if (line.startsWith("HTTP/1.1 200 OK")) {
        udp.print("HTTP/1.1 200 OK");
        //packets_success++;
      }
    }
    udp.println();
  }
  else {
    udp.println("certificate doesn't match");
  }
}

void loop() {
  ArduinoOTA.handle();

  unsigned long currentMillis = millis();
  switch (state) {
    case WAITING:
      if (Serial.available() > 0 ) {
        buf_idx = 0;
        state = READING;
      }
      break;
    case READING:
      while (Serial.available() > 0 ) {
        rawbuf[buf_idx] = Serial.read();
        buf_idx++;
        previousMillis = currentMillis;
      }
      // Check if end of msg
      // The method for detecting a completed message is really neat, it assumes it is finished when it has not received any data for 10 ms
      if (currentMillis - previousMillis >= interval) {
        state = PROCESSING;
      }
      break;
    case PROCESSING:
      {
        MyData data;
        ParseResult<void> res = P1Parser::parse(&data, rawbuf, buf_idx, true);
        udp.beginPacketMulticast(broadcastIP, txPort, myIP);
        if (res.err) {
          udp.println(res.fullError(rawbuf, rawbuf + buf_idx));
        }
        else {
          //data.applyEach(Printer());
          sendDataToHost(data);
          //udp.print(data.power_delivered);
        }
        udp.endPacket();
        state = WAITING;
        break;
      }
    default:
      break;
  }
}
