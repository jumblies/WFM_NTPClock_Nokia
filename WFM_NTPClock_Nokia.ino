/*****************************************************************************
  The MIT License (MIT)

  Copyright (c) 2015 by bbx10node@gmail.com

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
 **************************************************************************/

/*
   ESP8266 Arduino IDE

   WiFi clock for PCD8544 LCD display periodically synchronized with Network
   Time Protocol servers.

   Synchronizes with time.nist.gov which randomly selects from a pool of
   NTP (Network Time Protocol) servers.

   The PCD8544 LCD driver is a fork of the Adafruit driver with changes for the ESP8266.
   Be sure to use the esp8266 branch!

   https://github.com/bbx10/Adafruit-PCD8544-Nokia-5110-LCD-library/tree/esp8266

   The Time library provides date and time with external date time sources. The library
   requests UTC date and time from a Network Time Protocol (NTP) server every 5 minutes.
   In between calls to the NTP server, the library uses the millis() function to update
   the date and time. The NTP part of this program is based on the Time_NTP example.

   https://github.com/PaulStoffregen/Time

   The Adafruit_GFX library should be installed using the Arduino IDE Library manager.
   No changes are needed for the ESP8266.

*/
// Libraries needed for WiFi Manager
#include <WiFiManager.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <DNSServer.h>

// Libraries used for NTP clock
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Time.h>

// Libraries needed for display
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>

// OTA updates
#include <ArduinoOTA.h>



//for LED status
#include <Ticker.h>
Ticker ticker;

// Timekeeping
/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
static const char ntpServerName[] = "time.nist.gov";
static const char tzName[] = "Eastern";
static const int timeZone = -4;  // Eastern Standard Time (USA)
// -4 for DST, -5 for non DST


void tick()
{
  //toggle state
  int state = digitalRead(BUILTIN_LED);  // get the current state of GPIO1 pin
  digitalWrite(BUILTIN_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}


WiFiUDP Udp;
uint16_t localPort;  // local port to listen for UDP packets

// pins
const int8_t RST_PIN = D2;
const int8_t CE_PIN = D1;
const int8_t DC_PIN = D6;
//const int8_t DIN_PIN = D7;  // Uncomment for Software SPI
//const int8_t CLK_PIN = D5;  // Uncomment for Software SPI
const int8_t BL_PIN = D0;
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_PIN, CE_PIN, RST_PIN);

/*   RSSI of wifiMgr connection attempt
*/
long rssi;
int8_t graph[83];
uint8_t i, col, pos = 0;
bool scroll = false;

void setup()
{
  Serial.begin(115200);


  //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //  wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(BUILTIN_LED, HIGH);

  // Initialize LCD
  display.begin();
  display.setContrast(55);
  display.setTextSize(2);
  display.setTextColor(BLACK);
  display.clearDisplay();
  display.display();

  Serial.print(F("IP number assigned by DHCP is "));
  Serial.println(WiFi.localIP());

  // Seed random with values unique to this device
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  uint32_t seed1 =
    (macAddr[5] << 24) | (macAddr[4] << 16) |
    (macAddr[3] << 8)  | macAddr[2];
  randomSeed(WiFi.localIP() + seed1 + micros());
  localPort = random(1024, 65535);

  Serial.println(F("Starting UDP"));
  Udp.begin(localPort);
  Serial.print(F("Local port: "));
  Serial.println(Udp.localPort());
  Serial.println(F("waiting for sync"));



  setSyncProvider(getNtpTime);
  setSyncInterval(5 * 60);

  //OTA section
  ArduinoOTA.setHostname("ESPOTAClock");

  // No authentication by default
  //   ArduinoOTA.setPassword((const char *)"ESP8266NET");

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

}


void loop()
{
  static time_t prevDisplay = 0; // when the digital clock was displayed
  timeStatus_t ts = timeStatus();

  switch (ts) {
    case timeNeedsSync:
    case timeSet:
      if (now() != prevDisplay) { //update the display only if time has changed
        prevDisplay = now();
        digitalClockDisplay();
        if (ts == timeNeedsSync) {
          Serial.println(F("time needs sync"));
        }
      }
      break;
    case timeNotSet:
      Serial.println(F("Time not set"));
      delay(1000);                      //Missing delay here to prevent flooding of serial comm
      now();
      break;
    default:
      break;
  }
  ArduinoOTA.handle();
}

const uint8_t SKINNY_COLON[] PROGMEM = {
  B00000000,
  B00000000,
  B01100000,
  B11110000,
  B11110000,
  B01100000,
  B00000000,
  B00000000,
  B01100000,
  B11110000,
  B11110000,
  B01100000,
  B00000000,
  B00000000,
};

void digitalClockDisplay() {
  tmElements_t tm;
  char *dayOfWeek;

  breakTime(now(), tm);
  dayOfWeek = dayShortStr(tm.Wday);
  // digital clock display of the time
  Serial.printf("%s %02d %02d %04d %02d:%02d:%02d\r\n",
                dayOfWeek, tm.Month, tm.Day, tm.Year + 1970,
                tm.Hour, tm.Minute, tm.Second);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextWrap(false);
  display.printf("%s %02d %02d %04d\n",
                 dayOfWeek, tm.Month, tm.Day, tm.Year + 1970);
  display.setTextSize(3);
  display.setCursor(0, 9);
  display.printf("%02d", tm.Hour);
  display.drawBitmap(36, 13, SKINNY_COLON, 4, 14, 1); //prev 37, 14
  display.setCursor(42, 9);
  display.printf("%02d", tm.Minute);
  //  display.drawBitmap(54, 16, SKINNY_COLON, 4, 14, 1);
  display.setCursor(72, 32);
  display.setTextSize(1);
  display.printf("%02d", tm.Second);

  display.setCursor(0, 40);
  display.setTextSize(1);
  display.print("IP:");
  display.println(WiFi.localIP());
  display.drawRect(0, 32, 62, 7, BLACK);
  display.fillRect(1, 32, (tm.Second), 7, BLACK);      // draw seconds graphic timer across bottom



  //  display.print(tzName);    //Didn't want the Timezone name to be displayed.
  display.display();
}

/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  IPAddress timeServerIP; // time.nist.gov NTP server address

  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print(F("Transmit NTP Request "));
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);
  Serial.println(timeServerIP);

  sendNTPpacket(timeServerIP);
  uint32_t beginWait = millis();
  while ((millis() - beginWait) < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println(F("Receive NTP Response"));
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + (timeZone * SECS_PER_HOUR);
    }
  }
  Serial.println(F("No NTP Response :-("));
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

