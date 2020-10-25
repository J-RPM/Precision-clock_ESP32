/*
        A part for this code was created from the information of D L Bird 2019
                          See more at http://dsbird.org.uk
           https://www.youtube.com/watch?v=RTKQMuWPL5A&ab_channel=G6EJD-David

          Some routines were created from the information of Andreas Spiess
                        https://www.sensorsiot.org/
                  https://www.youtube.com/c/AndreasSpiess/

                                 --- oOo ---
                                
                              Modified by: J_RPM
                               http://j-rpm.com/
                        https://www.youtube.com/c/JRPM
                              October of 2020

               An OLED display is added to show the Time an Date,
                adding some new routines and modifying others.

                              >>> HARDWARE <<<
                  LIVE D1 mini ESP32 ESP-32 WiFi + Bluetooth
                https://es.aliexpress.com/item/32816065152.html
                
            HW-699 0.66 inch OLED display module, for D1 Mini (64x48)
               https://es.aliexpress.com/item/4000504485892.html

               4x Módulos de Control de pantalla Led 8x8 MAX7219
                https://es.aliexpress.com/item/33038259447.html

                           >>> IDE Arduino <<<
                        Model: WEMOS MINI D1 ESP32
       Add URLs: https://dl.espressif.com/dl/package_esp32_index.json
                     https://github.com/espressif/arduino-esp32

 ____________________________________________________________________________________
*/
String HWversion = "(v1.3)";
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <time.h>

#include <DNSServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager
#include <Adafruit_GFX.h>
//*************************************************IMPORTANT******************************************************************
#include "Adafruit_SSD1306.h" // Copy the supplied version of Adafruit_SSD1306.cpp and Adafruit_ssd1306.h to the sketch folder
#define  OLED_RESET 0         // GPIO0
Adafruit_SSD1306 display(OLED_RESET);

String CurrentTime, CurrentDate, nDay, webpage = "";
bool display_EU = true;

#define NUM_MAX 4

// ESP32 -> Matrix
#define DIN_PIN 27 
#define CS_PIN  25  
#define CLK_PIN 32 

#include "max7219.h"
#include "fonts.h"

// Define the number of bytes you want to access
#define EEPROM_SIZE 5

////////////////////////// MATRIX //////////////////////////////////////////////
#define MAX_DIGITS 20
bool display_date = true;
bool animated_time = true;
bool show_seconds = false;
byte dig[MAX_DIGITS]={0};
byte digold[MAX_DIGITS]={0};
byte digtrans[MAX_DIGITS]={0};
int dots = 1;
long dotTime = 0;
long clkTime = 0;
int dx=0;
int dy=0;
byte del=0;
int h,m,s;
int dualChar = 0;
int brightness = 5;  //DUTY CYCLE: 11/32
String mDay;
char myButton[100];
long timeConnect;
//////////////////////////////////////////////////////////////////////////////

WiFiClient client;
const char* Timezone    = "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00";  // Choose your time zone from: https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv 
                                                           // See below for examples
const char* ntpServer   = "es.pool.ntp.org";               // Or, choose a time server close to you, but in most cases it's best to use pool.ntp.org to find an NTP server
                                                           // then the NTP system decides e.g. 0.pool.ntp.org, 1.pool.ntp.org as the NTP syem tries to find  the closest available servers
                                                           // EU "0.europe.pool.ntp.org"
                                                           // US "0.north-america.pool.ntp.org"
                                                           // See: https://www.ntppool.org/en/                                                           
int  gmtOffset_sec      = 0;    // UK normal time is GMT, so GMT Offset is 0, for US (-5Hrs) is typically -18000, AU is typically (+8hrs) 28800
int  daylightOffset_sec = 7200; // In the UK DST is +1hr or 3600-secs, other countries may use 2hrs 7200 or 30-mins 1800 or 5.5hrs 19800 Ahead of GMT use + offset behind - offset

WebServer server(80);  // Set the port you wish to use, a browser default is 80, but any port can be used, if you set it to 5555 then connect with http://nn.nn.nn.nn:5555/

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void setup() {
  
  Serial.begin(115200);
  timeConnect = millis();
 
  initMAX7219();
  sendCmdAll(CMD_SHUTDOWN,1);
  sendCmdAll(CMD_INTENSITY,brightness);

  // Test MATRIX
  sendCmdAll(CMD_DISPLAYTEST, 1);
  delay(500);
  sendCmdAll(CMD_DISPLAYTEST, 0);
 
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15,0);   
  display.println(F("NTP"));
  display.setCursor(6,16);   
  display.println(F("TIME"));

  display.setTextSize(1);   
  display.setCursor(12,32);   
  display.println(HWversion);
  display.setCursor(10,40);   
  display.println(F("Sync..."));
  display.display();
  
  printStringWithShift((String("  NTP Time  ")+ HWversion + " ").c_str(), 20);
  delay(2000);
  
  //------------------------------
  //WiFiManager intialisation. Once completed there is no need to repeat the process on the current board
  WiFiManager wifiManager;
  display_AP_wifi();

  // A new OOB ESP32 will have no credentials, so will connect and not need this to be uncommented and compiled in, a used one will, try it to see how it works
  // Uncomment the next line for a new device or one that has not connected to your Wi-Fi before or you want to reset the Wi-Fi connection
  // wifiManager.resetSettings();
  // Then restart the ESP32 and connect your PC to the wireless access point called 'ESP32_AP' or whatever you call it below
  // Next connect to http://192.168.4.1/ and follow instructions to make the WiFi connection

  // Set a timeout until configuration is turned off, useful to retry or go to sleep in n-seconds
  wifiManager.setTimeout(180);
  
  //fetches ssid and password and tries to connect, if connections succeeds it starts an access point with the name called "ESP32_AP" and waits in a blocking loop for configuration
  if (!wifiManager.autoConnect("ESP32_AP")) {
    Serial.println(F("Failed to connect and timeout occurred"));
    display_AP_wifi();
    display_flash();
    reset_ESP32();
  }
  // At this stage the WiFi manager will have successfully connected to a network, or if not will try again in 180-seconds
  //------------------------------
  // 
  Serial.print(F(">>> Connection Delay(ms): "));
  Serial.println((millis()-timeConnect));
  if(millis()-timeConnect > 30000) {
    Serial.println(F("TimeOut connection, restarting!!!"));
    reset_ESP32();
  }
  
  // Print the IP address
  Serial.print(F("Use this URL to connect: "));
  Serial.print(F("http://")); Serial.print(WiFi.localIP()); Serial.println("/");
  display_ip();
  SetupTime();
  UpdateLocalTime();
  server.begin();  // Start the webserver
  Serial.println(F("Webserver started..."));
  
  // Define what happens when a client requests attention
  server.on("/", NTP_Clock_home_page);
  server.on("/DISPLAY_MODE_TOGGLE", display_mode_toggle); 
  server.on("/TIME_MODE", display_time_mode); 
  server.on("/TIME_VIEW", display_time_view); 
  server.on("/DISPLAY_DATE_TOGGLE", display_date_toggle);
  server.on("/LOW", brightness_low);
  server.on("/MEDIUM", brightness_medium); 
  server.on("/HIGH", brightness_high); 
  server.on("/HOME", NTP_Clock_home_page);
  server.on("/RESTART", web_reset_ESP32);  
  server.on("/RESET_WIFI", reset_wifi);

  // Initialize EEPROM with predefined size
  EEPROM.begin(EEPROM_SIZE);

  // 0 - Display Status 
  display_EU = EEPROM.read(0);
  Serial.print(F("display_EU: "));
  Serial.println(display_EU);

  // 1 - Display Date  
  display_date = EEPROM.read(1);
  Serial.print(F("display_date: "));
  Serial.println(display_date);
  
  // 2 - Matrix Brightness  
  brightness = EEPROM.read(2);
  Serial.print(F("brightness: "));
  Serial.println(brightness);
  sendCmdAll(CMD_INTENSITY,brightness);
  
  // 3 - Animated Time  
  animated_time = EEPROM.read(3);
  Serial.print(F("animated_time: "));
  Serial.println(animated_time);

  // 4 - Show Seconds  
  show_seconds = EEPROM.read(4);
  Serial.print(F("show_seconds: "));
  Serial.println(show_seconds);

  // Close EEPROM    
  EEPROM.commit();
  EEPROM.end();
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void loop() {
  // Update and refresh of the date and time on the displays
  if (millis() % 60000) UpdateLocalTime();
  display_time();
  
  // Wait for a client to connect and when they do process their requests
  server.handleClient();   

  // Serial display time and date & dots flashing
  if(millis()-dotTime > 500) {
    Serial.println(CurrentTime);
    Serial.println(mDay);
    dotTime = millis();
    dots = !dots;
  }
  
  // Show date on matrix display
  if (display_date == true) {
    if(millis()-clkTime > 30000 && !del && dots) { // clock for 30s, then scrolls for about 5s
      printStringWithShift((String("     ")+ mDay + "           ").c_str(), 25);
      delay(200);
      clkTime = millis();
    }
  }
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
boolean SetupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  setenv("TZ", Timezone, 1);                                                  // setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(2000);
  bool TimeStatus = UpdateLocalTime();
  return TimeStatus;
}
//////////////////////////////////////////////////////////////////////////////
boolean UpdateLocalTime() {
  struct tm timeinfo;
  time_t now;
  time(&now);

  //See http://www.cplusplus.com/reference/ctime/strftime/
  char output[30];
  strftime(output, 30, "%A", localtime(&now));
  nDay = output;
  mDay = nDay;
  nDay = (nDay.substring(0,3)) + ". ";
  if (display_EU == true) {
    strftime(output, 30,"%d-%m", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30,", %d %B %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%H:%M:%S", localtime(&now));
    CurrentTime = output;
  }
  else { 
    strftime(output, 30, "%m-%d", localtime(&now));
    CurrentDate = nDay + output;
    strftime(output, 30, ", %B %d, %Y", localtime(&now));
    mDay = mDay + output;
    strftime(output, 30, "%r", localtime(&now));
    CurrentTime = output;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////
void display_time() { 
  display.clearDisplay();
  display.setCursor(2,0);   // center date display
  display.setTextSize(1);   
  display.println(CurrentDate);

  display.setTextSize(2);   
  display.setCursor(8,16);  // center Time display
  if (CurrentTime.startsWith("0")){
    display.println(CurrentTime.substring(1,5));
  }else {
    display.setCursor(0,16);
    display.println(CurrentTime.substring(0,5));
  }
  
  if (display_EU == true) {
    display.setCursor(7,33); // center Time display
    display.print("(" + CurrentTime.substring(6,8) + ")");
  }else {
    display.print("(" + CurrentTime.substring(6,8) + ")");
    display.setTextSize(1);
    display.setCursor(40,33); // center Time display
    display.print(CurrentTime.substring(8,11));
  }
  display.display();

  //------------------ MATRIX DISPLAY ---------------//
  h = (CurrentTime.substring(0,2)).toInt();
  m = (CurrentTime.substring(3,5)).toInt();
  s = (CurrentTime.substring(6,8)).toInt();

  if (show_seconds == true) {
    if (animated_time == true) {
      showAnimSecClock();
      showAnimSecClock();
      showAnimSecClock();
    }else {
      showSecondsClock();
    }
  } else {
    if (animated_time == true) {
      showAnimClock();
      showAnimClock();
      showAnimClock();
    }else {
      showSimpleClock();
    }
  }
  //-------------------------------------------------//
}
//////////////////////////////////////////////////////////////////////////////
// A short method of adding the same web page header to some text
//////////////////////////////////////////////////////////////////////////////
void append_webpage_header() {
  // webpage is a global variable
  webpage = ""; // A blank string variable to hold the web page
  webpage += "<!DOCTYPE html><html>"; 
  webpage += "<style>html { font-family: tahoma; display: inline-block; margin: 0px auto; text-align: center;}";
  webpage += "#mark    {border: 5px solid #316573 ; width: 1120px;} "; 
  webpage += "#header  {background-color:#C3E0E8; font-family:tahoma; width:1100px; padding:10px; color:#13414E; font-size:36px; text-align:center; }";
  webpage += "#body    {background-color:#C3E0E8; font-family:tahoma; width:1100px; padding:10px; color:black; font-size:42px; text-align:center; }";
  webpage += "#section {background-color:#E6F5F9; font-family:tahoma; width:1100px; padding:10px; color:black; font-size:36px; text-align:center;}";
  webpage += "#footer  {background-color:#C3E0E8; font-family:tahoma; width:1100px; padding:10px; color:#13414E; font-size:24px; clear:both; text-align:center;}";
 
  webpage += ".line {border: 3px solid #666; border-radius: 300px/10px; height: 0px; text-align: center; width: 60%;}";

  webpage += ".button { box-shadow: 0px 10px 14px -7px #276873; background:linear-gradient(to bottom, #599bb3 5%, #408c99 100%);";
  webpage += "background-color: #599bb3; border-radius:8px; color: white; padding: 13px 32px; display:inline-block; cursor: pointer;";
  webpage += "text-decoration: none;text-shadow:0px 1px 0px #3d768a; font-size: 36px; font-weight:bold; margin: 2px; }";
  webpage += ".button:hover { background:linear-gradient(to bottom, #408c99 5%, #599bb3 100%); background-color:#408c99;}";
  webpage += ".button:active { position:relative; top:1px;}";
 
  webpage += ".button2 { box-shadow: 0px 10px 14px -7px #8a2a21; background:linear-gradient(to bottom, #f24437 5%, #c62d1f 100%);";
  webpage += "background-color: #f24437; text-shadow:0px 1px 0px #810e05; }";
  webpage += ".button2:hover { background:linear-gradient(to bottom, #c62d1f 5%, #f24437 100%); background-color:#f24437;}</style>";
 
  webpage += "<html><head><title>ESP32 NTP Clock</title>";
  webpage += "</style></head><body>";
  webpage += "<div id=\"mark\">";
  webpage += "<div id=\"header\"><h1>NTP - Local Time Clock " + HWversion + "</h1>";
}
//////////////////////////////////////////////////////////////////////////////
void NTP_Clock_home_page() {
  append_webpage_header();
  webpage += "<p><h2>Display Mode is ";
  if (display_EU == true) webpage += "EU"; else webpage += "USA";
  webpage += "</h2></p>";
  webpage += "<h3>[";
  if (animated_time == true) webpage += "Animated "; else webpage += "Normal ";
  if (show_seconds == true) webpage += "(hh:mm:ss) "; else webpage += "(HH:MM) ";
  if (display_date == true) webpage += "& Date ";
  if (brightness < 5){
    webpage += "- LOW";      
  }else if (brightness > 5){
    webpage += "- HIGH";     
  }else {
    webpage += "- MEDIUM";   
  }
  webpage += "]</h3></div>";

  webpage += "<div id=\"section\">";
  webpage += "<p><a href=\"DISPLAY_MODE_TOGGLE\"><button class=\"button\">Display Mode (USA/EU)</button></a></p>";
  webpage += "<p><a href=\"TIME_MODE\"><button class=\"button\">Time Mode (Animated/Normal)</button></a></p>";
  webpage += "<p><a href=\"TIME_VIEW\"><button class=\"button\">Time View (HH:MM/hh:mm:ss)</button></a></p>";
  webpage += "<p><a href=\"DISPLAY_DATE_TOGGLE\"><button class=\"button\">Change Show Date</button></a></p>";
  webpage += "<p><a href=\"HIGH\"><button class=\"button\">HIGH brightness</button></a></p>";
  webpage += "<p><a href=\"MEDIUM\"><button class=\"button\">MEDIUM brightness</button></a></p>";
  webpage += "<p><a href=\"LOW\"><button class=\"button\">LOW brightness</button></a></p>";
  webpage += "<hr class='line'/>";
  webpage += "<p><a href=\"HOME\"><button class=\"button\">Refresh WEB</button></a></p>";
  webpage += "<p><a href=\"RESTART\"><button class=\"button\">Restart ESP32</button></a></p>";
  webpage += "<p><a href=\"RESET_WIFI\"><button class=\"button button2\">Reset WiFi</button></a></p>";
  webpage += "</div>";
  end_webpage();
}
//////////////////////////////////////////////////////////////////////////////
void end_webpage(){
  webpage += "<div id=\"footer\">Copyright &copy; J_RPM 2020</div></body></div></html>";
  server.send(200, "text/html", webpage);
}
//////////////////////////////////////////////////////////////////////////////
void brightness_low() {
  brightness = 1;     //DUTY CYCLE: 3/32 (MIN = 0)
  brightness_matrix();
}
//////////////////////////////////////////////////////////////////////////////
void brightness_medium() {
  brightness = 5;     //DUTY CYCLE: 11/32
  brightness_matrix();
}
//////////////////////////////////////////////////////////////////////////////
void brightness_high() {
  brightness = 10;    //DUTY CYCLE: 21/32  (MAX = 15)
  brightness_matrix();
}
//////////////////////////////////////////////////////////////////////////////
void display_mode_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  if (display_EU) display_EU = false; else display_EU = true;
  EEPROM.write(0, display_EU);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_date_toggle() {
  EEPROM.begin(EEPROM_SIZE);
  if (display_date) display_date = false; else display_date = true;
  EEPROM.write(1, display_date);
  end_Eprom();
}
//////////////////////////////////////////////////////////////////////////////
void brightness_matrix() {
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.write(2, brightness);
  sendCmdAll(CMD_INTENSITY,brightness);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_mode() {
  EEPROM.begin(EEPROM_SIZE);
  if (animated_time) animated_time = false; else animated_time = true;
  EEPROM.write(3, animated_time);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void display_time_view() {
  EEPROM.begin(EEPROM_SIZE);
  if (show_seconds) show_seconds = false; else show_seconds = true;
  EEPROM.write(4, show_seconds);
  end_Eprom();
}
//////////////////////////////////////////////////////////////
void end_Eprom() {
  del=0;
  dots=1;
  EEPROM.commit();
  EEPROM.end();
  NTP_Clock_home_page();
}
//////////////////////////////////////////////////////////////
void reset_wifi() {
  append_webpage_header();
  webpage += "<p><h2>New WiFi Connection</h2></p></div>";
  webpage += "<div id=\"section\">";
  webpage += "<p>&#149; Connect WiFi to SSID: <b>ESP32_AP</b></p>";
  webpage += "<p>&#149; Next connect to: <b><a href=http://192.168.4.1/>http://192.168.4.1/</a></b></p>";
  webpage += "<p>&#149; Make the WiFi connection</p>";
  webpage += "<p><a href=\"HOME\"><button class=\"button\">Refresh WEB</button></a></p>";
  webpage += "</div>";
  end_webpage();
  delay(1000);
  WiFiManager wifiManager;
  wifiManager.resetSettings();      // RESET WiFi in ESP32
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void web_reset_ESP32 () {
  append_webpage_header();
  webpage += "<p><h2>Restarting ESP32...</h2></p></div>";
  webpage += "<div id=\"section\">";
  webpage += "<p><a href=\"HOME\"><button class=\"button\">Refresh WEB</button></a></p>";
  webpage += "</div>";
  end_webpage();
  delay(1000);
  reset_ESP32();
}
//////////////////////////////////////////////////////////////
void reset_ESP32() {
  sendCmdAll(CMD_SHUTDOWN,0);
  ESP.restart();
  delay(5000);
}
//////////////////////////////////////////////////////////////
void display_AP_wifi () {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(4,0);   
  display.println(F("ENTRY"));
  display.setCursor(10,16);   
  display.println(F("WiFi"));
  display.setTextSize(1);   
  display.setCursor(8,32);   
  display.println("ESP32_AP");
  display.setCursor(0,40);   
  display.println(F("192168.4.1"));
  display.display();
}
//////////////////////////////////////////////////////////////
void display_flash() {
  for (int i=0; i<8; i++) {
    display.invertDisplay(true);
    display.display();
    sendCmdAll(CMD_SHUTDOWN,0);
    delay (150);
    display.invertDisplay(false);
    display.display();
    sendCmdAll(CMD_SHUTDOWN,1);
    delay (150);
  }
}
//////////////////////////////////////////////////////////////
void display_ip() {
  // Print the IP address MATRIX
  printStringWithShift((String("  http:// ")+ WiFi.localIP().toString()).c_str(), 20);

  // Display OLED & MATRIX
  display.clearDisplay();
  display.setTextSize(2);   
  display.setCursor(4,8);   
  display.print("ENTRY");
  display.setTextSize(1);   
  display.setCursor(0,26);   
  display.print("http://");
  display.print(WiFi.localIP());
  display.println("/");
  display.display();
  display_flash();
}
//////////////////////////////////////////////////////////////////////////////
void showSimpleClock(){
  dx=dy=0;
  clr();
   if(h/10==0) {
    showDigit(10,  0, dig6x8);
   }else {
    showDigit(h/10,  0, dig6x8);
   }
  showDigit(h%10,  8, dig6x8);
  showDigit(m/10, 17, dig6x8);
  showDigit(m%10, 25, dig6x8);
  showDigit(s/10, 34, dig6x8);
  showDigit(s%10, 42, dig6x8);
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
void showSecondsClock(){
  dx=dy=0;
  clr();
   if(h/10==0) {
    showDigit(10,  0, dig4x8r);
   }else {
    showDigit(h/10,  0, dig4x8r);
   }
  showDigit(h%10,  5, dig4x8r);
  showDigit(m/10, 11, dig4x8r);
  showDigit(m%10, 16, dig4x8r);
  showDigit(s/10, 23, dig4x8r);
  showDigit(s%10, 28, dig4x8r);
  setCol(9,1 ? B00100100 : 0);
  setCol(21,1 ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
void showAnimClock(){
  byte digPos[6]={0,8,17,25,34,42};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig6x8);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig6x8);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig6x8);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(15,dots ? B00100100 : 0);
  setCol(32,dots ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
void showAnimSecClock(){
  byte digPos[6]={0,5,11,16,23,28};
  int digHt = 12;
  int num = 6; 
  int i;
  if(del==0) {
    del = digHt;
    for(i=0; i<num; i++) digold[i] = dig[i];
    dig[0] = h/10 ? h/10 : 10;
    dig[1] = h%10;
    dig[2] = m/10;
    dig[3] = m%10;
    dig[4] = s/10;
    dig[5] = s%10;
    for(i=0; i<num; i++)  digtrans[i] = (dig[i]==digold[i]) ? 0 : digHt;
  } else
    del--;
  
  clr();
  for(i=0; i<num; i++) {
    if(digtrans[i]==0) {
      dy=0;
      showDigit(dig[i], digPos[i], dig4x8r);
    } else {
      dy = digHt-digtrans[i];
      showDigit(digold[i], digPos[i], dig4x8r);
      dy = -digtrans[i];
      showDigit(dig[i], digPos[i], dig4x8r);
      digtrans[i]--;
    }
  }
  dy=0;
  setCol(9,1 ? B00100100 : 0);
  setCol(21,1 ? B00100100 : 0);
  refreshAll();
}
//////////////////////////////////////////////////////////////////////////////
void showDigit(char ch, int col, const uint8_t *data){
  if(dy<-8 | dy>8) return;
  int len = pgm_read_byte(data);
  int w = pgm_read_byte(data + 1 + ch * len);
  col += dx;
  for (int i = 0; i < w; i++)
    if(col+i>=0 && col+i<8*NUM_MAX) {
      byte v = pgm_read_byte(data + 1 + ch * len + 1 + i);
      if(!dy) scr[col + i] = v; else scr[col + i] |= dy>0 ? v>>dy : v<<-dy;
    }
}
//////////////////////////////////////////////////////////////////////////////
void setCol(int col, byte v){
  if(dy<-8 | dy>8) return;
  col += dx;
  if(col>=0 && col<8*NUM_MAX)
    if(!dy) scr[col] = v; else scr[col] |= dy>0 ? v>>dy : v<<-dy;
}
//////////////////////////////////////////////////////////////////////////////
int showChar(char ch, const uint8_t *data){
  int len = pgm_read_byte(data);
  int i,w = pgm_read_byte(data + 1 + ch * len);
  for (i = 0; i < w; i++)
    scr[NUM_MAX*8 + i] = pgm_read_byte(data + 1 + ch * len + 1 + i);
  scr[NUM_MAX*8 + i] = 0;
  return w;
}
//////////////////////////////////////////////////////////////////////////////
unsigned char convertPolish(unsigned char _c){
  unsigned char c = _c;
  if(c==196 || c==197 || c==195) {
    dualChar = c;
    return 0;
  }
  if(dualChar) {
    switch(_c) {
      case 133: c = 1+'~'; break; // 'ą'
      case 135: c = 2+'~'; break; // 'ć'
      case 153: c = 3+'~'; break; // 'ę'
      case 130: c = 4+'~'; break; // 'ł'
      case 132: c = dualChar==197 ? 5+'~' : 10+'~'; break; // 'ń' and 'Ą'
      case 179: c = 6+'~'; break; // 'ó'
      case 155: c = 7+'~'; break; // 'ś'
      case 186: c = 8+'~'; break; // 'ź'
      case 188: c = 9+'~'; break; // 'ż'
      //case 132: c = 10+'~'; break; // 'Ą'
      case 134: c = 11+'~'; break; // 'Ć'
      case 152: c = 12+'~'; break; // 'Ę'
      case 129: c = 13+'~'; break; // 'Ł'
      case 131: c = 14+'~'; break; // 'Ń'
      case 147: c = 15+'~'; break; // 'Ó'
      case 154: c = 16+'~'; break; // 'Ś'
      case 185: c = 17+'~'; break; // 'Ź'
      case 187: c = 18+'~'; break; // 'Ż'
      default:  break;
    }
    dualChar = 0;
    return c;
  }    
  switch(_c) {
    case 185: c = 1+'~'; break;
    case 230: c = 2+'~'; break;
    case 234: c = 3+'~'; break;
    case 179: c = 4+'~'; break;
    case 241: c = 5+'~'; break;
    case 243: c = 6+'~'; break;
    case 156: c = 7+'~'; break;
    case 159: c = 8+'~'; break;
    case 191: c = 9+'~'; break;
    case 165: c = 10+'~'; break;
    case 198: c = 11+'~'; break;
    case 202: c = 12+'~'; break;
    case 163: c = 13+'~'; break;
    case 209: c = 14+'~'; break;
    case 211: c = 15+'~'; break;
    case 140: c = 16+'~'; break;
    case 143: c = 17+'~'; break;
    case 175: c = 18+'~'; break;
    default:  break;
  }
  return c;
}
//////////////////////////////////////////////////////////////////////////////
void printCharWithShift(unsigned char c, int shiftDelay) {
  c = convertPolish(c);
  if (c < ' ' || c > '~'+25) return;
  c -= 32;
  int w = showChar(c, font);
  for (int i=0; i<w+1; i++) {
    delay(shiftDelay);
    scrollLeft();
    refreshAll();
  }
}
//////////////////////////////////////////////////////////////////////////////
void printStringWithShift(const char* s, int shiftDelay){
  while (*s) {
    printCharWithShift(*s, shiftDelay);
    s++;
  }
}
////////////////////////// END ////////////////////////////////////////////////////
