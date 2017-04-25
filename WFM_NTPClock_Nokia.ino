/*
  WiFi Signal Strength
  Shows RSSI of a predefined network with a graph

  RSSI is a percentage in the range -120db to 0db.
  The closer to 0 the better.

  Connections:
  WeMos D1 Mini   Nokia 5110    Description
  (ESP8266)       PCD8544 LCD

  D2 (GPIO4)      0 RST         Output from ESP to reset display
  D1 (GPIO5)      1 CE          Output from ESP to chip select/enable display
  D6 (GPIO12)     2 DC          Output from display data/command to ESP
  D7 (GPIO13)     3 Din         Output from ESP SPI MOSI to display data input
  D5 (GPIO14)     4 Clk         Output from ESP SPI clock
  3V3             5 Vcc         3.3V from ESP to display
  D0 (GPIO16)     6 BL          3.3V to turn backlight on, or PWM
  G               7 Gnd         Ground

  Dependencies:
  https://github.com/adafruit/Adafruit-GFX-Library
  https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library
  - This pull request adds ESP8266 support:
  - https://github.com/adafruit/Adafruit-PCD8544-Nokia-5110-LCD-library/pull/27
*/


#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <WiFiUdp.h>              //UDP Socket for NTP timekeeping

// Display Libraries
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_PCD8544.h>
#include <Arduino.h>

//Font inclusion
#include <Fonts/FreeSerif9pt7b.h>


// pins
const int8_t RST_PIN = D2;
const int8_t CE_PIN = D1;
const int8_t DC_PIN = D6;
//const int8_t DIN_PIN = D7;  // Uncomment for Software SPI
//const int8_t CLK_PIN = D5;  // Uncomment for Software SPI
const int8_t BL_PIN = D0;

//Nonblocking Millis timer setup
unsigned long previousMillis = 0; 
const long interval = 60000;           // interval at which to check NTP (milliseconds)



//for LED status
#include <Ticker.h>
Ticker ticker;

//UDP funcionality
unsigned int localPort = 2390;      // local port to listen for UDP packets

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(98, 175, 203, 200 ); // time.nist.gov NTP server
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "time.nist.gov";

const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message

byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

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

/*   RSSI of wifiMgr connection attempt
*/
long rssi;
int8_t graph[83];
uint8_t i, col, pos = 0;
bool scroll = false;

// Software SPI with explicit CS pin.
//Adafruit_PCD8544(int8_t CLK_PIN, int8_t DIN_PIN, int8_t DC_PIN, int8_t CE_PIN, int8_t RST_PIN);

// Software SPI with CS tied to ground.  Saves a pin but other pins can't be shared with other hardware.
//Adafruit_PCD8544(int8_t CLK_PIN, int8_t DIN_PIN, int8_t DC_PIN, int8_t RST_PIN);

// Hardware SPI based on hardware controlled SCK (SCLK) and MOSI (DIN) pins. CS is still controlled by any IO pin.
// NOTE: MISO and SS will be set as an input and output respectively, so be careful sharing those pins!
Adafruit_PCD8544 display = Adafruit_PCD8544(DC_PIN, CE_PIN, RST_PIN);







void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //set led pin as output
  pinMode(BUILTIN_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
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
  digitalWrite(BUILTIN_LED, LOW);

  // Display Initialization
  // Turn LCD backlight on
  pinMode(BL_PIN, OUTPUT);
  digitalWrite(BL_PIN, HIGH);

  // Configure LCD
  display.begin();
  display.setContrast(60);  // Adjust for your display higher is less contrast
  display.setTextSize(1);
  //  display.setFont();
  display.setTextColor(BLACK);
  display.setCursor(0, 0);
  display.clearDisplay();

  display.clearDisplay();
  display.println("Connected to: ");
  display.print(WiFi.SSID());
  display.display();




  /*UDP LOOP SETUP

  */
  Serial.println("WiFi connected for NTP Service");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());


}

void loop() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you blinked the LED
    previousMillis = currentMillis;
    
  //get a random server from the pool
  WiFi.hostByName(ntpServerName, timeServerIP);

  sendNTPpacket(timeServerIP); // send an NTP packet to a time server
  // wait to see if a reply is available
  delay(1000);

  int cb = udp.parsePacket();
  if (!cb) {
    Serial.println("no packet yet");
  }
  else {
    Serial.print("packet received, length=");
    Serial.println(cb);
    // We've received a packet, read the data from it
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer

    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:

    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    unsigned long secsSince1900 = highWord << 16 | lowWord;
    Serial.print("Seconds since Jan 1 1900 = " );
    Serial.println(secsSince1900);

    // now convert NTP time into everyday time:
    Serial.print("Unix time = ");
    // Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
    const unsigned long seventyYears = 2208988800UL;
    // subtract seventy years:
    unsigned long epoch = secsSince1900 - seventyYears;
    // print Unix time:
    Serial.println(epoch);


    // print the hour, minute and second:
    Serial.print("The UTC time is ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print((epoch  % 86400L) / 3600); // print the hour (86400 equals secs per day)
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second


    //Now lets print the local time for EST
    Serial.print("The EST time is ");
    byte hourGMT = ((epoch  % 86400L) / 3600);  // print the hour (86400 equals secs per day)
    int hourEST = (hourGMT - 4);                //  adjust UTC to EST with DST
    if (hourEST < 0) {                          // if EST hour is negative then...
      hourEST = (24 + hourEST);                 // add it to 24 to get legit EST hour
    }

    Serial.print(hourEST);
    Serial.print(':');
    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.print((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)
    Serial.print(':');
    if ( (epoch % 60) < 10 ) {
      // In the first 10 seconds of each minute, we'll want a leading '0'
      Serial.print('0');
    }
    Serial.println(epoch % 60); // print the second



    //Print the signal strength
    //    rssi = WiFi.RSSI();  // eg. -63
    display.clearDisplay();
    display.setTextSize(2);
    //    display.print(WiFi.SSID());
    //    display.print(": ");
    //    display.printf("%ddb\n", rssi);
    //    display.println();


        display.println("EST:");
    if ((hourEST) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      display.print('0');
    }
    display.print(hourEST);


    display.print(':');

    if ( ((epoch % 3600) / 60) < 10 ) {
      // In the first 10 minutes of each hour, we'll want a leading '0'
      display.print('0');
    }
    display.println((epoch  % 3600) / 60); // print the minute (3600 equals secs per minute)

    //Seconds went here but we don't want seconds  because of blocking delay so leaving it out.


    display.display();
  }
  }
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
  Serial.println("sending NTP packet...");
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
  udp.beginPacket(address, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();

}


