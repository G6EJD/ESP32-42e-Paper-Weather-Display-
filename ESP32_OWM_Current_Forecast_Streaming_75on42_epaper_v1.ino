/* ESP32 Weather Display using an EPD 4.2" Display, obtains data from Open Weather Map, decodes it and then displays it.
  ####################################################################################################################################
  This software, the ideas and concepts is Copyright (c) David Bird 2018. All rights to this software are reserved.

  Any redistribution or reproduction of any part or all of the contents in any form is prohibited other than the following:
  1. You may print or download to a local hard disk extracts for your personal and non-commercial use only.
  2. You may copy the content to individual third parties for their personal use, but only if you acknowledge the author David Bird as the source of the material.
  3. You may not, except with my express written permission, distribute or commercially exploit the content.
  4. You may not transmit it or store it in any other website or other form of electronic retrieval system for commercial purposes.

  The above copyright ('as annotated') notice and this permission notice shall be included in all copies or substantial portions of the Software and where the
  software use is visible to an end-user.

  THE SOFTWARE IS PROVIDED "AS IS" FOR PRIVATE USE ONLY, IT IS NOT FOR COMMERCIAL USE IN WHOLE OR PART OR CONCEPT. FOR PERSONAL USE IT IS SUPPLIED WITHOUT WARRANTY
  OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
  IN NO EVENT SHALL THE AUTHOR OR COPYRIGHT HOLDER BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  See more at http://www.dsbird.org.uk
*/
#include "owm_credentials2.h"  // See 'owm_credentials' tab and enter your OWM API key and set the Wifi SSID and PASSWORD
#include <ArduinoJson.h>       // https://github.com/bblanchon/ArduinoJson NOTE: *** MUST BE Version-6 or above ***
#include <WiFi.h>              // Built-in
#include "time.h"              // Built-in
#include <SPI.h>               // Built-in 
#include "EPD_WaveShare.h"     // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "EPD_WaveShare_42.h"  // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "MiniGrafx.h"         // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "DisplayDriver.h"     // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx
#include "ArialRounded.h"      // Copyright (c) 2017 by Daniel Eichhorn https://github.com/ThingPulse/minigrafx

#define SCREEN_WIDTH  400.0    // Set for landscape mode, don't remove the decimal place!
#define SCREEN_HEIGHT 300.0
#define BITS_PER_PIXEL 1
#define EPD_BLACK 0
#define EPD_WHITE 1
uint16_t palette[] = { 0, 1 };

// pins_arduino.h, e.g. LOLIN32 LITE
static const uint8_t EPD_BUSY = 4;
static const uint8_t EPD_SS   = 5;
static const uint8_t EPD_RST  = 16;
static const uint8_t EPD_DC   = 17;
static const uint8_t EPD_SCK  = 18;
static const uint8_t EPD_MISO = 19; // Master-In Slave-Out not used, as no data from display
static const uint8_t EPD_MOSI = 23;

EPD_WaveShare42 epd(EPD_SS, EPD_RST, EPD_DC, EPD_BUSY);
MiniGrafx gfx = MiniGrafx(&epd, BITS_PER_PIXEL, palette);

//################  VERSION  ##########################
String version = "1";        // Version of this program
//################ VARIABLES ###########################

const unsigned long UpdateInterval = (30L * 60L - 03) * 1000000L; // Update delay in microseconds, 13-secs is the time to update so compensate for that
bool LargeIcon =  true;
bool SmallIcon =  false;
#define Large  6
#define Small  4
String time_str, Day_time_str; // strings to hold time and received weather data;
int    wifi_signal,wifisection, displaysection, MoonDay, MoonMonth, MoonYear, start_time;

//################ PROGRAM VARIABLES and OBJECTS ################

typedef struct { // For current Day and Day 1, 2, 3, etc
  String   Dt;
  String   Period;
  String   Icon;
  String   Trend;
  String   Main0;
  String   Forecast0;
  String   Forecast1;
  String   Forecast2;
  String   Description;
  String   Time;
  String   Country;
  float    lat;
  float    lon;
  float    Temperature;
  float    Humidity;
  float    High;
  float    Low;
  float    Winddir;
  float    Windspeed;
  float    Rainfall;
  float    Snowfall;
  float    Pressure;
  int      Cloudcover;
  int      Visibility;
  int      Sunrise;
  int      Sunset;
} Forecast_record_type;

#define max_readings 24

Forecast_record_type  WxConditions[1];
Forecast_record_type  WxForecast[max_readings];

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

float pressure_readings[max_readings]    = {0};
float temperature_readings[max_readings] = {0};
float humidity_readings[max_readings]    = {0};
float rain_readings[max_readings]        = {0};

WiFiClient client; // wifi client object

//#########################################################################################
void setup() {
  start_time = millis();
  Serial.begin(115200);
  StartWiFi();
  wifi_signal = WiFi_Signal();
  SetupTime();
  bool Received_WxData_OK = false;
  Received_WxData_OK = (obtain_wx_data("weather") && obtain_wx_data("forecast"));
  // Now only refresh the screen if all the data was received OK, otherwise wait until the next timed check otherwise wait until the next timed check
  if (Received_WxData_OK) {
    StopWiFi(); // Reduces power consumption
    gfx.init();
    gfx.setRotation(0);
    gfx.setColor(EPD_BLACK);
    gfx.fillBuffer(EPD_WHITE);
    gfx.setTextAlignment(TEXT_ALIGN_LEFT);
    Display_Weather();
    gfx.commit();
    delay(2000);
    Serial.println("total time to update = "+String(millis()-start_time));
  }
  begin_sleep();
}
//#########################################################################################
void loop() { // this will never run!
}
//#########################################################################################
void begin_sleep(){
  esp_sleep_enable_timer_wakeup(UpdateInterval);
  Serial.println(F("Starting deep-sleep period..."));
  #ifdef BUILTIN_LED
  pinMode(BUILTIN_LED,INPUT);     // In case it's on, turn output off, sometimes PIN-5 on some boards is used for SPI-SS
  digitalWrite(BUILTIN_LED,HIGH); // In case it's on, turn LED off, as sometimes PIN-5 on some boards is used for SPI-SS
  #endif
  esp_deep_sleep_start(); // Sleep for e.g. 30 minutes
}
//#########################################################################################
void Display_Weather() {                         // 7.5" e-paper display is 640x384 resolution
  Display_Heading_Section();                     // Top line of the display
  Display_Wind_Section(75,92, WxConditions[0].Winddir, WxConditions[0].Windspeed, 47);
  Display_Main_Weather_Section(206, 80);         // Centre section of display for Location, temperature, Weather report, current Wx Symbol and wind direction
  Display_Forecast_Section(148, 87);             // 3hr forecast boxes
  Display_Astronomy_Section(-55, 87);            // Astronomy section Sun rise/set, Moon phase and Moon icon
  Display_Status_Section(390, 177, wifi_signal); // Wi-Fi signal strength and Battery voltage
}
//#########################################################################################
void Display_Heading_Section() {
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(SCREEN_WIDTH / 2, -2, City);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  gfx.drawString(SCREEN_WIDTH-3, 0, Day_time_str);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawString(5, 0, time_str);
  gfx.drawLine(0, 15, SCREEN_WIDTH, 15);
}
//#########################################################################################
void Display_Main_Weather_Section(int x, int y) {
  gfx.drawLine(0,30,SCREEN_WIDTH,30);
  Display_Conditions_Section(x-5, y-15, WxConditions[0].Icon, LargeIcon);
  Display_Temperature_Section(x+50,y-50);
  Display_Pressure_Section(x+50, y-31, WxConditions[0].Pressure, WxConditions[0].Trend);
  Display_Rain_Section(x+50, y-12);
  Display_ForecastText_Section(x-55, y+75);
}
//#########################################################################################
String TitleCase(String text) { // Not currently used
  if (text.length() > 0) {
    String temp_text = text.substring(0, 1);
    temp_text.toUpperCase();
    return temp_text + text.substring(1); // Title-case the string
  }
  return "";
}
//#########################################################################################
void Display_Temperature_Section(int x, int y){
  int b1width = 30;
  int b2width = 110;
  gfx.drawRect(x,y,b1width,20);
  gfx.drawRect(x+b1width+1,y,b2width,20);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+b1width/2,y+2, "Temp:");
  gfx.drawString(x+(b1width+b2width-24),y+4, String(WxConditions[0].High,0) + "° | " + String(WxConditions[0].Low,0) + "°"); // Show forecast high and Low
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x+(b1width+28),y, String(WxConditions[0].Temperature,1)+"°"+(Units=="M"?"C":"F")); // Show current Temperature
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Display_Pressure_Section(int x, int y, float pressure, String slope) {
  int b1width = 30;
  int b2width = 110;
  gfx.drawRect(x,y,b1width,20);
  gfx.drawRect(x+b1width+1,y,b2width,20);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+b1width/2,y+2,"Pres:");
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.drawString(x+(b1width+b2width/2-20),y+2, String(pressure,1) + (Units == "M" ? "mb" : "in"));
  gfx.setFont(ArialMT_Plain_10);
       if (slope == "+") gfx.drawString(x+(b1width+b2width/2+36), y+0, "Rising");
  else if (slope == "0") gfx.drawString(x+(b1width+b2width/2+36), y+5, "Steady");
  else if (slope == "-") gfx.drawString(x+(b1width+b2width/2+36), y+8, "Falling");
}
//#########################################################################################
void Display_Rain_Section(int x, int y) {
  int b1width = 30;
  int b2width = 110;
  gfx.drawRect(x,y,b1width,20);
  gfx.drawRect(x+b1width+1,y,b2width,20);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x+b1width/2,y+2,"Rain:");
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialRoundedMTBold_14);
  if (WxForecast[1].Rainfall > 0) gfx.drawString(x+(b1width+4), y+2, String(WxForecast[1].Rainfall,3) + (Units == "M" ? "mm" : "in")); // Only display rainfall total today if > 0
  else 
  {
    gfx.setTextAlignment(TEXT_ALIGN_CENTER);
    gfx.drawString(x+(b1width+b2width/2), y+2, "= ="); // If no rain forecast or has occurred
  }
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Display_Forecast_Section(int x, int y) {
  gfx.setFont(ArialMT_Plain_10);
  Display_Forecast_Weather(x, y, 0);
  Display_Forecast_Weather(x, y, 1);
  Display_Forecast_Weather(x, y, 2);
  Display_Forecast_Weather(x, y, 3);
  Display_Forecast_Weather(x, y, 4);
//  Display_Forecast_Weather(x, y, 5);
//  Display_Forecast_Weather(x, y, 6);
//  Display_Forecast_Weather(x, y, 7);
  // (x,y,width,height,MinValue, MaxValue, Title, Data Array, AutoScale, ChartMode)
  // Pre-load temporary arrays with with data - becuase C parses by reference
  for (int r = 1; r <= max_readings; r++) {
   if (Units == "I") pressure_readings[r] = WxForecast[r].Pressure * 0.02953;  
   else              pressure_readings[r] = WxForecast[r].Pressure;
   temperature_readings[r] = WxForecast[r].Temperature;
   if (Units == "I") rain_readings[r]     = WxForecast[r].Rainfall * 0.0393701;
   else rain_readings[r]   = WxForecast[r].Rainfall;
   humidity_readings[r]    = WxForecast[r].Humidity;
  }
  int gx = 28;   // Baseline graph x position on screen
  int gy = 225;  // Baseline graph y position on screen
  int gap = (SCREEN_WIDTH - gx*2)/3.5;
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawLine(0, gy-20, SCREEN_WIDTH, gy-20);
  //DrawGraph(x_pos,y_pos,gwidth,gheight,Y1Min,Y1Max,title,DataArray[],readings,auto_scale,barchart_mode)
  DrawGraph(gx+0*gap, SCREEN_HEIGHT/300*gy, 70, SCREEN_HEIGHT/300*50,900,1050,"Pressure", pressure_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx+1*gap, SCREEN_HEIGHT/300*gy, 70, SCREEN_HEIGHT/300*50,10,30, "Temperature", temperature_readings, max_readings, autoscale_on, barchart_off);
  DrawGraph(gx+2*gap, SCREEN_HEIGHT/300*gy, 70, SCREEN_HEIGHT/300*50,0,100, "Humidity", humidity_readings, max_readings, autoscale_off, barchart_off);
  DrawGraph(gx+3*gap, SCREEN_HEIGHT/300*gy, 70, SCREEN_HEIGHT/300*50,0,30, "Rainfall", rain_readings, max_readings, autoscale_on, barchart_on);
}
//#########################################################################################
void Display_ForecastText_Section(int x, int y){
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  String Wx_Description = WxConditions[0].Main0;
  if (WxConditions[0].Forecast0 != "") {
    Wx_Description += " (" +  WxConditions[0].Forecast0;
    if (WxConditions[0].Forecast1 != "" && WxConditions[0].Forecast1 != WxConditions[0].Forecast2) Wx_Description += " & " +  WxConditions[0].Forecast1 + ")";
    else Wx_Description += ")";
  }
  gfx.drawStringMaxWidth(x, y, 200, TitleCase(Wx_Description)); // Limit text display to the 200 pixel wide section, then wrap
  gfx.setFont(ArialMT_Plain_10);
}
//#########################################################################################
void Display_Forecast_Weather(int x, int y, int index) {
  int fwidth = 50;
  x = x + fwidth * index;
  gfx.setFont(ArialMT_Plain_10);
  gfx.setColor(EPD_BLACK); // Sometimes gets set to WHITE, so change back
  gfx.drawRect(x, y, fwidth-1, 65);
  gfx.drawLine(x, y + 13, x + fwidth, y + 13);
  Display_Conditions_Section(x + fwidth/2, y + 35, WxForecast[index].Icon, SmallIcon);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawString(x + fwidth/2, y, String(WxForecast[index].Period.substring(11,16)));
  gfx.drawString(x + fwidth/2, y + 50, String(WxForecast[index].High,0) + "° / " + String(WxForecast[index].Low,0) + "°");
}
//#########################################################################################
void Display_Wind_Section(int x, int y, float angle, float windspeed, int Cradius) {
  arrow(x, y, Cradius - 10, angle, 10, 15); // Show wind direction on outer circle width,length
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x,y-Cradius-29,"Wind Speed & Direction");
  int dxo, dyo, dxi, dyi;
  gfx.drawCircle(x,y,Cradius);     // Draw compass circle
  gfx.drawCircle(x,y,Cradius+1);   // Draw compass circle
  gfx.drawCircle(x,y,Cradius*0.7); // Draw compass inner circle
  for (float a = 0; a <360; a = a + 22.5) {
    dxo = Cradius * cos((a-90)*PI/180);
    dyo = Cradius * sin((a-90)*PI/180);
    if (a == 45)  gfx.drawString(dxo+x+10,dyo+y-10,"NE");
    if (a == 135) gfx.drawString(dxo+x+5,dyo+y+5,"SE");
    if (a == 225) gfx.drawString(dxo+x-10,dyo+y,"SW");
    if (a == 315) gfx.drawString(dxo+x-10,dyo+y-10,"NW");
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    gfx.drawLine(dxo+x,dyo+y,dxi+x,dyi+y);   
    dxo = dxo * 0.7;
    dyo = dyo * 0.7;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    gfx.drawLine(dxo+x,dyo+y,dxi+x,dyi+y);   
  }
  gfx.drawString(x,y-Cradius-13,"N");
  gfx.drawString(x,y+Cradius,"S");
  gfx.drawString(x-Cradius-7,y-5,"W");
  gfx.drawString(x+Cradius+7,y-5,"E");
  gfx.drawString(x, y-25, WindDegToDirection(angle));
  gfx.drawString(x, y+12, String(angle,0) + "°");
  gfx.setFont(ArialMT_Plain_16);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x, y-10, String(windspeed,1) + (Units == "M" ? " m/s" : " mph"));
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.drawLine(x+73,15,x+73,150);
}
//#########################################################################################
String WindDegToDirection(float winddirection) {
  if (winddirection >= 348.75 || winddirection < 11.25)  return "N";
  if (winddirection >=  11.25 && winddirection < 33.75)  return "NNE";
  if (winddirection >=  33.75 && winddirection < 56.25)  return "NE";
  if (winddirection >=  56.25 && winddirection < 78.75)  return "ENE";
  if (winddirection >=  78.75 && winddirection < 101.25) return "E";
  if (winddirection >= 101.25 && winddirection < 123.75) return "ESE";
  if (winddirection >= 123.75 && winddirection < 146.25) return "SE";
  if (winddirection >= 146.25 && winddirection < 168.75) return "SSE";
  if (winddirection >= 168.75 && winddirection < 191.25) return "S";
  if (winddirection >= 191.25 && winddirection < 213.75) return "SSW";
  if (winddirection >= 213.75 && winddirection < 236.25) return "SW";
  if (winddirection >= 236.25 && winddirection < 258.75) return "WSW";
  if (winddirection >= 258.75 && winddirection < 281.25) return "W";
  if (winddirection >= 281.25 && winddirection < 303.75) return "WNW";
  if (winddirection >= 303.75 && winddirection < 326.25) return "NW";
  if (winddirection >= 326.25 && winddirection < 348.75) return "NNW";
  return "?";
}
//#########################################################################################
void Display_Astronomy_Section(int x, int y) {
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
  gfx.setFont(ArialMT_Plain_10);
  gfx.drawRect(x+55, y + 64, 147, 55);
  gfx.drawString(x + 60, y + 65, "Sun Rise: "+ ConvertUnixTime(WxConditions[0].Sunrise).substring(0,5));
  gfx.drawString(x + 71, y + 77, "     Set: "+ ConvertUnixTime(WxConditions[0].Sunset).substring(0,5));
  gfx.drawString(x + 60, y + 100, "Moon: "+MoonPhase(MoonDay, MoonMonth, MoonYear, Hemisphere));
  DrawMoon(x + 140, y + 51, MoonDay, MoonMonth, MoonYear, Hemisphere);
}
//#########################################################################################
void DrawMoon(int x, int y, int dd, int mm, int yy, String hemisphere) {
  int diameter = 38;
  float Xpos, Ypos, Rpos, Xpos1, Xpos2, ip, ag;
  gfx.setColor(EPD_BLACK);
  for (Ypos = 0; Ypos <= 45; Ypos++) {
    Xpos = sqrt(45 * 45 - Ypos * Ypos);
    // Draw dark part of moon
    double pB1x = (90   - Xpos) / 90 * diameter + x;
    double pB1y = (Ypos + 90) / 90   * diameter + y;
    double pB2x = (Xpos + 90) / 90   * diameter + x;
    double pB2y = (Ypos + 90) / 90   * diameter + y;
    double pB3x = (90   - Xpos) / 90 * diameter + x;
    double pB3y = (90   - Ypos) / 90 * diameter + y;
    double pB4x = (Xpos + 90) / 90   * diameter + x;
    double pB4y = (90   - Ypos) / 90 * diameter + y;
    gfx.setColor(EPD_BLACK);
    gfx.drawLine(pB1x, pB1y, pB2x, pB2y);
    gfx.drawLine(pB3x, pB3y, pB4x, pB4y);
    // Determine the edges of the lighted part of the moon
    int j = JulianDate(dd, mm, yy);
    //Calculate the approximate phase of the moon
    double Phase = (j + 4.867) / 29.53059;
    Phase = Phase - (int)Phase;
    if (Phase < 0.5) ag = Phase * 29.53059 + 29.53059 / 2; else ag = Phase * 29.53059 - 29.53059 / 2; // Moon's age in days
    if (hemisphere == "south") Phase = 1 - Phase;
    Rpos = 2 * Xpos;
    if (Phase < 0.5) {
      Xpos1 = - Xpos;
      Xpos2 = Rpos - 2 * Phase * Rpos - Xpos;
    }
    else {
      Xpos1 = Xpos;
      Xpos2 = Xpos - 2 * Phase * Rpos + Rpos;
    }
    // Draw light part of moon
    double pW1x = (Xpos1 + 90) / 90 * diameter + x;
    double pW1y = (90 - Ypos) / 90  * diameter + y;
    double pW2x = (Xpos2 + 90) / 90 * diameter + x;
    double pW2y = (90 - Ypos) / 90  * diameter + y;
    double pW3x = (Xpos1 + 90) / 90 * diameter + x;
    double pW3y = (Ypos + 90) / 90  * diameter + y;
    double pW4x = (Xpos2 + 90) / 90 * diameter + x;
    double pW4y = (Ypos + 90) / 90  * diameter + y;
    gfx.setColor(EPD_WHITE);
    gfx.drawLine(pW1x, pW1y, pW2x, pW2y);
    gfx.drawLine(pW3x, pW3y, pW4x, pW4y);
  }
  gfx.setColor(EPD_BLACK);
  gfx.drawCircle(x + diameter - 1, y + diameter, diameter / 2 + 1);
}
//#########################################################################################
int JulianDate(int d, int m, int y) {
  int mm, yy, k1, k2, k3, j;
  yy = y - (int)((12 - m) / 10);
  mm = m + 9;
  if (mm >= 12) mm = mm - 12;
  k1 = (int)(365.25 * (yy + 4712));
  k2 = (int)(30.6001 * mm + 0.5);
  k3 = (int)((int)((yy / 100) + 49) * 0.75) - 38;
  // 'j' for dates in Julian calendar:
  j = k1 + k2 + d + 59 + 1;
  if (j > 2299160) j = j - k3; // 'j' is the Julian date at 12h UT (Universal Time) For Gregorian calendar:
  return j;
}
//#########################################################################################
String MoonPhase(int d, int m, int y, String hemisphere) {
  int c, e;
  double jd;
  int b;
  if (m < 3) {
    y--;
    m += 12;
  }
  ++m;
  c   = 365.25 * y;
  e   = 30.6   * m;
  jd  = c + e + d - 694039.09;           /* jd is total days elapsed */
  jd /= 29.53059;                        /* divide by the moon cycle (29.53 days) */
  b   = jd;                              /* int(jd) -> b, take integer part of jd */
  jd -= b;                               /* subtract integer part to leave fractional part of original jd */
  b   = jd * 8 + 0.5;                    /* scale fraction from 0-8 and round by adding 0.5 */
  b   = b & 7;                           /* 0 and 8 are the same phase so modulo 8 for 0 */
  if (hemisphere == "south") b = 7 - b;
  if (b == 0) return "New";              // New; 0% illuminated
  if (b == 1) return "Waxing Crescent";  // Waxing crescent; 25% illuminated
  if (b == 2) return "First Quarter";    // First quarter;   50% illuminated
  if (b == 3) return "Waxing Gibbous";   // Waxing gibbous;  75% illuminated
  if (b == 4) return "Full";             // Full;            100% illuminated
  if (b == 5) return "Waning Gibbous";   // Waning gibbous;  75% illuminated
  if (b == 6) return "Third Quarter";    // Third quarter;   50% illuminated
  if (b == 7) return "Waning Crescent";  // Waning crescent; 25% illuminated
  return "";
}
//#########################################################################################
void DrawCircle(int x, int y, int Cstart, int Cend, int Cradius, int Coffset_radius, int Coffset) {
  gfx.setColor(EPD_BLACK);
  float dx, dy;
  for (int i = Cstart; i < Cend; i++) {
    dx = (Cradius + Coffset_radius) * cos((i - 90) * PI / 180) + x + Coffset / 2; // calculate X position
    dy = (Cradius + Coffset_radius) * sin((i - 90) * PI / 180) + y; // calculate Y position
    gfx.setPixel(dx, dy);
  }
}
//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength) {
  float dx = (asize - 10) * cos((aangle - 90) * PI / 180) + x; // calculate X position
  float dy = (asize - 10) * sin((aangle - 90) * PI / 180) + y; // calculate Y position
  float x1 = 0;         float y1 = plength;
  float x2 = pwidth / 2;  float y2 = pwidth / 2;
  float x3 = -pwidth / 2; float y3 = pwidth / 2;
  float angle = aangle * PI / 180 - 135;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  gfx.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2);
}
//#########################################################################################
void Display_Conditions_Section(int x, int y, String IconName, bool IconSize) {
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  if (IconSize == LargeIcon) gfx.drawString(x+70, y-50, "Conditions");
  Serial.println(IconName);
  if      (IconName == "01d" || IconName == "01n")  Sunny(x, y, IconSize, IconName);
  else if (IconName == "02d" || IconName == "02n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "03d" || IconName == "03n")  Cloudy(x, y, IconSize, IconName);
  else if (IconName == "04d" || IconName == "04n")  MostlySunny(x, y, IconSize, IconName);
  else if (IconName == "09d" || IconName == "09n")  ChanceRain(x, y, IconSize, IconName);
  else if (IconName == "10d" || IconName == "10n")  Rain(x, y, IconSize, IconName);
  else if (IconName == "11d" || IconName == "11n")  Tstorms(x, y, IconSize, IconName);
  else if (IconName == "13d" || IconName == "13n")  Snow(x, y, IconSize, IconName);
  else if (IconName == "50d")                       Haze(x, y, IconSize, IconName);
  else if (IconName == "50n")                       Fog(x, y, IconSize, IconName);
  else                                              Nodata(x, y, IconSize);
}
//#########################################################################################
bool obtain_wx_data(String RequestType) {
  String units = (Units == "M"?"metric":"imperial");
  client.stop(); // close connection before sending a new request
  if (client.connect(server, 80)) { // if the connection succeeds
    // Serial.println("connecting...");
    // send the HTTP PUT request:
    if (RequestType == "weather")
      client.println("GET /data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units="+units+"&lang="+Language+" HTTP/1.1");
    else
      client.println("GET /data/2.5/" + RequestType + "?q=" + City + "," + Country + "&APPID=" + apikey + "&mode=json&units="+units+"&lang="+Language+"&cnt=24 HTTP/1.1");
    client.println("Host: api.openweathermap.org");
    client.println("User-Agent: ESP OWM Receiver/1.1");
    client.println("Connection: close");
    client.println();
    // Check HTTP status
    char status[32] = {0};
    client.readBytesUntil('\r', status, sizeof(status));
    if (strcmp(status, "HTTP/1.1 200 OK") != 0) {
      Serial.print(F("Unexpected response: "));
      Serial.println(status);
      return false;
    }
    // Skip HTTP headers
    char endOfHeaders[] = "\r\n\r\n";
    if (!client.find(endOfHeaders)) {
      Serial.println(F("Invalid response"));
      return false;
    }
    DecodeWeather(client, RequestType);
    client.stop();
    return true;
  }
  else
  { // if no connection was made
    Serial.println("connection failed");
    return false;
  }
  return true;
}
//#########################################################################################
// Problems with stucturing JSON decodes, see here: https://arduinojson.org/assistant/
bool DecodeWeather(Stream &json_stream, String Type) {
  Serial.print("Creating object and ");
  DynamicJsonDocument doc(50*1024);
  DeserializationError error = deserializeJson(doc, json_stream);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  // Extract values
  JsonObject& root = doc.as<JsonObject>();
  Serial.println(" Decoding " + Type + " data");
  if (Type == "weather") {
    // All Serial.println statements are for diagnostic purposes and not required, remove if not needed 
    WxConditions[0].lon         = root["coord"]["lon"].as<float>();              Serial.println(WxConditions[0].lon);
    WxConditions[0].lat         = root["coord"]["lat"].as<float>();              Serial.println(WxConditions[0].lat);
    WxConditions[0].Main0       = root["weather"][0]["main"].as<char*>();        Serial.println(WxConditions[0].Main0);
    WxConditions[0].Forecast0   = root["weather"][0]["description"].as<char*>(); Serial.println(WxConditions[0].Forecast0);
    WxConditions[0].Icon        = root["weather"][0]["icon"].as<char*>();        Serial.println(WxConditions[0].Icon);
    WxConditions[0].Forecast1   = root["weather"][1]["main"].as<char*>();        Serial.println(WxConditions[0].Forecast1);
    WxConditions[0].Forecast2   = root["weather"][2]["main"].as<char*>();        Serial.println(WxConditions[0].Forecast2);
    WxConditions[0].Temperature = root["main"]["temp"].as<float>();              Serial.println(WxConditions[0].Temperature);
    WxConditions[0].Pressure    = root["main"]["pressure"].as<float>();          Serial.println(WxConditions[0].Pressure);
    WxConditions[0].Humidity    = root["main"]["humidity"].as<float>();          Serial.println(WxConditions[0].Humidity);
    WxConditions[0].Low         = root["main"]["temp_min"].as<float>();          Serial.println(WxConditions[0].Low);
    WxConditions[0].High        = root["main"]["temp_max"].as<float>();          Serial.println(WxConditions[0].High);
    WxConditions[0].Windspeed   = root["wind"]["speed"].as<float>();             Serial.println(WxConditions[0].Windspeed);
    WxConditions[0].Winddir     = root["wind"]["deg"].as<float>();               Serial.println(WxConditions[0].Winddir);
    WxConditions[0].Cloudcover  = root["clouds"]["all"].as<int>();               Serial.println(WxConditions[0].Cloudcover); // in % of cloud cover
    WxConditions[0].Visibility  = root["visibility"].as<int>();                  Serial.println(WxConditions[0].Visibility); // in metres
    WxConditions[0].Country     = root["sys"]["country"].as<char*>();            Serial.println(WxConditions[0].Country);
    WxConditions[0].Sunrise     = root["sys"]["sunrise"].as<int>();              Serial.println(WxConditions[0].Sunrise);
    WxConditions[0].Sunset      = root["sys"]["sunset"].as<int>();               Serial.println(WxConditions[0].Sunset);
  }
  if (Type == "forecast") {
    //Serial.println(json);
    const char* cod                 = root["cod"]; // "200"
    float message                   = root["message"]; 
    int cnt                         = root["cnt"]; 
    JsonArray& list                 = root["list"];
    Serial.print("\nReceiving Forecast period - "); //------------------------------------------------
    for (byte r=0; r < max_readings; r++) {
      Serial.println("\nPeriod-"+String(r)+"--------------"); 
      WxForecast[r].Dt                = list[r]["dt"].as<char*>(); 
      WxForecast[r].Temperature       = list[r]["main"]["temp"].as<float>();              Serial.println(WxForecast[r].Temperature);
      WxForecast[r].Low               = list[r]["main"]["temp_min"].as<float>();          Serial.println(WxForecast[r].Low);
      WxForecast[r].High              = list[r]["main"]["temp_max"].as<float>();          Serial.println(WxForecast[r].High);
      WxForecast[r].Pressure          = list[r]["main"]["pressure"].as<float>();          Serial.println(WxForecast[r].Pressure);
      WxForecast[r].Humidity          = list[r]["main"]["humidity"].as<float>();          Serial.println(WxForecast[r].Humidity);
      WxForecast[r].Forecast0         = list[r]["weather"][0]["main"].as<char*>();        Serial.println(WxForecast[r].Forecast0);
      WxForecast[r].Forecast0         = list[r]["weather"][1]["main"].as<char*>();        Serial.println(WxForecast[r].Forecast1);
      WxForecast[r].Forecast0         = list[r]["weather"][2]["main"].as<char*>();        Serial.println(WxForecast[r].Forecast2);
      WxForecast[r].Description       = list[r]["weather"][0]["description"].as<char*>(); Serial.println(WxForecast[r].Description);
      WxForecast[r].Icon              = list[r]["weather"][0]["icon"].as<char*>();        Serial.println(WxForecast[r].Icon);
      WxForecast[r].Cloudcover        = list[r]["clouds"]["all"].as<int>();               Serial.println(WxForecast[0].Cloudcover); // in % of cloud cover
      WxForecast[r].Windspeed         = list[r]["wind"]["speed"].as<float>();             Serial.println(WxForecast[r].Windspeed);
      WxForecast[r].Winddir           = list[r]["wind"]["deg"].as<float>();               Serial.println(WxForecast[r].Winddir);
      WxForecast[r].Rainfall          = list[r]["rain"]["3h"].as<float>();                Serial.println(WxForecast[r].Rainfall);
      WxForecast[r].Snowfall          = list[r]["snow"]["3h"].as<float>();                Serial.println(WxForecast[r].Rainfall);
      WxForecast[r].Period            = list[r]["dt_txt"].as<char*>();                    Serial.println(WxForecast[r].Period);
    }
    //------------------------------------------
    float pressure_trend = WxForecast[0].Pressure - WxForecast[1].Pressure; // Measure pressure slope between ~now and later
    pressure_trend = ((int)(pressure_trend * 10)) / 10.0; // Remove any small variations less than 0.1
    WxConditions[0].Trend = "0";
    if (pressure_trend > 0)  WxConditions[0].Trend = "+";
    if (pressure_trend < 0)  WxConditions[0].Trend = "-";
    if (pressure_trend == 0) WxConditions[0].Trend = "0";

    if (Units == "I") Convert_Readings_to_Imperial();
  }
  return true;
}
//#########################################################################################
void Convert_Readings_to_Imperial() {
  WxConditions[0].Pressure = WxConditions[0].Pressure * 0.02953;   //  hPa to ins
  WxForecast[1].Rainfall   = WxForecast[1].Rainfall   * 0.0393701; // mm to inches of rainfall
}
//#########################################################################################
int StartWiFi() {
  int connAttempts = 0;
  Serial.print(F("\r\nConnecting to: ")); Serial.println(String(ssid1));
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid1, password1);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500); Serial.print(".");
    if (connAttempts > 20) {
      WiFi.disconnect();
      begin_sleep();
    }
    connAttempts++;
  }
  Serial.println("WiFi connected at: " + String(WiFi.localIP()));
  return 1;
}
//#########################################################################################
void StopWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  wifisection    = millis() - wifisection;
}
//#########################################################################################
int WiFi_Signal() {
  return WiFi.RSSI();
}
//#########################################################################################
void Display_Status_Section(int x, int y, int rssi){
  Draw_RSSI(x-25,y+25,rssi);
  DrawBattery(x, y-15);
  gfx.drawRect(x-32,y-26,40,55);
}
//#########################################################################################
void Draw_RSSI(int x, int y, int rssi){
  int WIFIsignal = 0;
  for (int _rssi = -100; _rssi <= rssi; _rssi = _rssi + 20) {
    if (_rssi <= -20)  WIFIsignal = 14; //            <20dbm displays 5-bars
    if (_rssi <= -40)  WIFIsignal = 11;  //  -40dbm to -21dbm displays 4-bars
    if (_rssi <= -60)  WIFIsignal = 9;  //  -60dbm to -41dbm displays 3-bars
    if (_rssi <= -80)  WIFIsignal = 6;  //  -80dbm to -61dbm displays 2-bars
    if (_rssi <= -100) WIFIsignal = 3;  // -100dbm to -81dbm displays 1-bar
    gfx.fillRect(x+WIFIsignal*2,y-WIFIsignal,3,WIFIsignal);
  }
  gfx.fillCircle(x,y-1,1);
}
//#########################################################################################
void DrawBattery(int x, int y) {
  uint8_t percentage = 100;
  float voltage = analogRead(35) / 4096.0 * 7.485;
  voltage = 4;
  if (voltage >= 0 ) { // Only display is there is a valid reading
    if (voltage >= 4.19) percentage = 100;
    else if (voltage < 3.20) percentage = 0;
    else percentage = (voltage - 3.20) * 100 / (4.20 - 3.20);
    gfx.setColor(EPD_BLACK);
    gfx.drawRect(x-22, y+5, 19, 10);
    gfx.fillRect(x-2, y+7, 3, 6);
    gfx.fillRect(x-20, y+7, 17 * percentage / 100.0, 6);
    gfx.setFont(ArialMT_Plain_10);
    gfx.drawString(x-20,y-8, String(percentage) + "%");
  }
}
//#########################################################################################
void SetupTime() {
  configTime(0, 0, "0.uk.pool.ntp.org", "time.nist.gov");
  setenv("TZ", Timezone, 1);
  delay(500);
  UpdateLocalTime();
}
//#########################################################################################
void UpdateLocalTime() {
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.println(F("Failed to obtain time"));
  }
  //See http://www.cplusplus.com/reference/ctime/strftime/
  //Serial.println(&timeinfo, "%a %b %d %Y   %H:%M:%S");     // Displays: Saturday, June 24 2017 14:05:49
  Serial.println(&timeinfo, "%H:%M:%S");                     // Displays: 14:05:49
  char output[30], day_output[30];
  if (Units == "M") {
    strftime(day_output, 30, "%a  %d-%b-%y", &timeinfo);     // Displays: Sat 24/Jun/17
    strftime(output, 30, "(Updated: %H:%M:%S )", &timeinfo); // Creates: '@ 14:05:49'
  }
  else {
    strftime(day_output, 30, "%a  %b-%d-%y", &timeinfo);     // Creates: Sat Jun/24/17
    strftime(output, 30, "(Updated: %r )", &timeinfo);       // Creates: '@ 2:05:49pm'
  }
  Day_time_str = day_output;
  time_str     = output;
}
//#########################################################################################
String ConvertUnixTime(int unix_time) {
  struct tm *now_tm;
  int hour, min, second, day, month, year, wday;
  // timeval tv = {unix_time,0};
  time_t tm = unix_time;
  now_tm = localtime(&tm);
  hour   = now_tm->tm_hour;
  min    = now_tm->tm_min;
  second = now_tm->tm_sec;
  wday   = now_tm->tm_wday;
  day    = now_tm->tm_mday;
  month  = now_tm->tm_mon + 1;
  year   = 1900 + now_tm->tm_year; // To get just YY information
  MoonDay   = day;
  MoonMonth = month;
  MoonYear  = year;
  if (Units == "M") {
    time_str =  (hour < 10 ? "0" + String(hour) : String(hour)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ":" + "  ";                     // HH:MM   05/07/17
    time_str += (day < 10 ? "0" + String(day) : String(day)) + "/" + (month < 10 ? "0" + String(month) : String(month)) + "/" + (year < 10 ? "0" + String(year) : String(year)); // HH:MM   05/07/17
  }
  else {
    String ampm = "am";
    if (hour > 11) ampm = "pm";
    hour = hour % 12; if (hour == 0) hour = 12;
    time_str =  (hour % 12 < 10 ? "0" + String(hour % 12) : String(hour % 12)) + ":" + (min < 10 ? "0" + String(min) : String(min)) + ampm + " ";      // HH:MMam 07/05/17
    time_str += (month < 10 ? "0" + String(month) : String(month)) + "/" + (day < 10 ? "0" + String(day) : String(day)) + "/" + "/" + (year < 10 ? "0" + String(year) : String(year)); // HH:MMpm 07/05/17
  }
  // Returns either '21:12  ' or ' 09:12pm' depending on Units
  //Serial.println(time_str);
  return time_str;
}
//#########################################################################################
// Symbols are drawn on a relative 10x10grid and 1 scale unit = 1 drawing unit
void addcloud(int x, int y, int scale, int linesize) {
  //Draw cloud outer
  gfx.fillCircle(x - scale * 3, y, scale);                       // Left most circle
  gfx.fillCircle(x + scale * 3, y, scale);                       // Right most circle
  gfx.fillCircle(x - scale, y - scale, scale * 1.4);             // left middle upper circle
  gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75); // Right middle upper circle
  gfx.fillRect(x - scale * 3 - 1, y - scale, scale * 6, scale * 2 + 1); // Upper and lower lines
  //Clear cloud inner
  gfx.setColor(EPD_WHITE);
  gfx.fillCircle(x - scale * 3, y, scale - linesize);            // Clear left most circle
  gfx.fillCircle(x + scale * 3, y, scale - linesize);            // Clear right most circle
  gfx.fillCircle(x - scale, y - scale, scale * 1.4 - linesize);  // left middle upper circle
  gfx.fillCircle(x + scale * 1.5, y - scale * 1.3, scale * 1.75 - linesize); // Right middle upper circle
  gfx.fillRect(x - scale * 3 + 2, y - scale + linesize - 1, scale * 5.9, scale * 2 - linesize * 2 + 2); // Upper and lower lines
  gfx.setColor(EPD_BLACK);
}
//#########################################################################################
void addrain(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 6; i++) {
    gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 0, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 0, y + scale);
    if (scale != Small) {
      gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 1, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 1, y + scale);
      gfx.drawLine(x - scale * 4 + scale * i * 1.3 + 2, y + scale * 1.9, x - scale * 3.5 + scale * i * 1.3 + 2, y + scale);
    }
  }
}
//#########################################################################################
void addsnow(int x, int y, int scale) {
  int dxo, dyo, dxi, dyi;
  for (int flakes = 0; flakes < 5; flakes++) {
    for (int i = 0; i < 360; i = i + 45) {
      dxo = 0.5 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.1;
      dyo = 0.5 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.1;
      gfx.drawLine(dxo + x + 0 + flakes * 1.5 * scale - scale * 3, dyo + y + scale * 2, dxi + x + 0 + flakes * 1.5 * scale - scale * 3, dyi + y + scale * 2);
    }
  }
}
//#########################################################################################
void addtstorm(int x, int y, int scale) {
  y = y + scale / 2;
  for (int i = 0; i < 5; i++) {
    gfx.drawLine(x - scale * 4 + scale * i * 1.5 + 0, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 0, y + scale);
    if (scale != Small) {
      gfx.drawLine(x - scale * 4 + scale * i * 1.5 + 1, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 1, y + scale);
      gfx.drawLine(x - scale * 4 + scale * i * 1.5 + 2, y + scale * 1.5, x - scale * 3.5 + scale * i * 1.5 + 2, y + scale);
    }
    gfx.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 0, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 0);
    if (scale != Small) {
      gfx.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 1, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 1);
      gfx.drawLine(x - scale * 4 + scale * i * 1.5, y + scale * 1.5 + 2, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5 + 2);
    }
    gfx.drawLine(x - scale * 3.5 + scale * i * 1.4 + 0, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 0, y + scale * 1.5);
    if (scale != Small) {
      gfx.drawLine(x - scale * 3.5 + scale * i * 1.4 + 1, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 1, y + scale * 1.5);
      gfx.drawLine(x - scale * 3.5 + scale * i * 1.4 + 2, y + scale * 2.5, x - scale * 3 + scale * i * 1.5 + 2, y + scale * 1.5);
    }
  }
}
//#########################################################################################
void addsun(int x, int y, int scale) {
  int linesize = 3;
  if (scale == Small) linesize = 1;
  int dxo, dyo, dxi, dyi;
  gfx.fillCircle(x, y, scale);
  gfx.setColor(EPD_WHITE);
  gfx.fillCircle(x, y, scale - linesize);
  gfx.setColor(EPD_BLACK);
  for (float i = 0; i < 360; i = i + 45) {
    dxo = 2.2 * scale * cos((i - 90) * 3.14 / 180); dxi = dxo * 0.6;
    dyo = 2.2 * scale * sin((i - 90) * 3.14 / 180); dyi = dyo * 0.6;
    if (i == 0   || i == 180) {
      gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y);
      if (scale != Small) {
        gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y);
        gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y);
      }
    }
    if (i == 90  || i == 270) {
      gfx.drawLine(dxo + x, dyo + y - 1, dxi + x, dyi + y - 1);
      if (scale != Small) {
        gfx.drawLine(dxo + x, dyo + y + 0, dxi + x, dyi + y + 0);
        gfx.drawLine(dxo + x, dyo + y + 1, dxi + x, dyi + y + 1);
      }
    }
    if (i == 45  || i == 135 || i == 225 || i == 315) {
      gfx.drawLine(dxo + x - 1, dyo + y, dxi + x - 1, dyi + y);
      if (scale != Small) {
        gfx.drawLine(dxo + x + 0, dyo + y, dxi + x + 0, dyi + y);
        gfx.drawLine(dxo + x + 1, dyo + y, dxi + x + 1, dyi + y);
      }
    }
  }
}
//#########################################################################################
void addfog(int x, int y, int scale, int linesize) {
  if (scale == Small) linesize = 1;
  for (int i = 0; i < 6; i++) {
    gfx.fillRect(x - scale * 3, y + scale * 1.5, scale * 6, linesize);
    gfx.fillRect(x - scale * 3, y + scale * 2.0, scale * 6, linesize);
    gfx.fillRect(x - scale * 3, y + scale * 2.5, scale * 6, linesize);
  }
}
//#########################################################################################
void MostlyCloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
}
//#########################################################################################
void MostlySunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
}
//#########################################################################################
void Rain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void Cloudy(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 2;
  if (scale == Small) {
    if (IconName.endsWith("n")) addmoon(x,y,scale);
    linesize = 1;
    addcloud(x, y, scale, linesize);
  }
  else {
    y += 8;
    if (IconName.endsWith("n")) addmoon(x,y,scale);
    addcloud(x+20, y-30, 3, linesize); // Cloud top right
    addcloud(x-13, y-20, 5, linesize); // Cloud top left
    addcloud(x, y, scale, linesize);   // Main cloud
  }
}
//#########################################################################################
void Sunny(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) { scale = Large; y-=8; };
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  scale = scale * 1.6;
  addsun(x, y, scale);
}
//#########################################################################################
void ExpectRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void ChanceRain(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addsun(x - scale * 1.8, y - scale * 1.8, scale);
  addcloud(x, y, scale, linesize);
  addrain(x, y, scale);
}
//#########################################################################################
void Tstorms(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addtstorm(x, y, scale);
}
//#########################################################################################
void Snow(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addsnow(x, y, scale);
}
//#########################################################################################
void Fog(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) scale = Large;
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addcloud(x, y, scale, linesize);
  addfog(x, y, scale, linesize-1);
}
//#########################################################################################
void Haze(int x, int y, bool IconSize, String IconName) {
  int scale = Small;
  if (IconSize == LargeIcon) {scale = Large; y-= 10;}
  int linesize = 3;
  if (scale == Small) linesize = 1;
  if (IconName.endsWith("n")) addmoon(x,y,scale);
  addsun(x, y, scale*1.4);
  addfog(x, y, scale*1.4, linesize);
}
//#########################################################################################
void addmoon(int x, int y, int scale){
  if (scale == Large) {
    gfx.fillCircle(x-40,y-20,scale*1.3);
    gfx.setColor(EPD_WHITE);
    gfx.fillCircle(x-33,y-20,scale*1.7);
    gfx.setColor(EPD_BLACK);
  }
  else
  {
    gfx.fillCircle(x-18,y-15,scale);
    gfx.setColor(EPD_WHITE);
    gfx.fillCircle(x-13,y-15,scale*1.6);
    gfx.setColor(EPD_BLACK);
  }
}
//#########################################################################################
void Nodata(int x, int y, int scale) {
  if (scale == Large) gfx.setFont(ArialMT_Plain_24); else gfx.setFont(ArialMT_Plain_16);
  gfx.drawString(x, y-10, "N/A");
}
//#########################################################################################
/* (C) D L BIRD
    This function will draw a graph on a ePaper/TFT/LCD display using data from an array containing data to be graphed.
    The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
    x_pos - the x axis top-left position of the graph
    y_pos - the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
    width - the width of the graph in pixels
    height - height of the graph in pixels
    Y1_Max - sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
    data_array1 is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
    auto_scale - a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
    barchart_on - a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
    barchart_colour - a sets the title and graph plotting colour
    If called with Y!_Max value of 500 and the data never goes above 500, then autoscale will retain a 0-500 Y scale, if on, the scale increases/decreases to match the data.
    auto_scale_margin, e.g. if set to 1000 then autoscale increments the scale by 1000 steps.
*/
void DrawGraph(int x_pos, int y_pos, int gwidth, int gheight, float Y1Min, float Y1Max, String title, float DataArray[], int readings, boolean auto_scale, boolean barchart_mode) {
#define auto_scale_margin 0 // Sets the autoscale increment, so axis steps up in units of e.g. 3
#define y_minor_axis 5      // 5 y-axis division markers
  int maxYscale = -10000;
  int minYscale =  10000;
  int last_x, last_y;
  float x1, y1, x2, y2;
  if (auto_scale == true) {
    for (int i = 1; i < readings; i++ ) {
      if (DataArray[i] >= maxYscale) maxYscale = DataArray[i];
      if (DataArray[i] <= minYscale) minYscale = DataArray[i];
    }
    maxYscale = round(maxYscale + auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Max
    Y1Max = round(maxYscale+0.5);
    if (minYscale != 0) minYscale = round(minYscale - auto_scale_margin); // Auto scale the graph and round to the nearest value defined, default was Y1Min
    Y1Min = round(minYscale);
  }
  // Draw the graph
  last_x = x_pos + 1;
  last_y = y_pos + (Y1Max - constrain(DataArray[1], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight;
  gfx.setColor(EPD_BLACK);
  gfx.drawRect(x_pos, y_pos, gwidth + 3, gheight + 2);
  gfx.setFont(ArialRoundedMTBold_14);
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos + gwidth / 2, y_pos - 17, title);
  gfx.setFont(ArialMT_Plain_10);
  gfx.setTextAlignment(TEXT_ALIGN_RIGHT);
  // Draw the data
  for (int gx = 1; gx < readings; gx++) {
      x1 = last_x;
      y1 = last_y;
      x2 = x_pos + gx * gwidth/(readings-1)-1 ; // max_readings is the global variable that sets the maximum data that can be plotted
      y2 = y_pos + (Y1Max - constrain(DataArray[gx], Y1Min, Y1Max)) / (Y1Max - Y1Min) * gheight + 1;
      if (barchart_mode) {
        gfx.fillRect(x2, y2, (gwidth/readings)-1, y_pos + gheight - y2 + 1);
      } else {
        gfx.drawLine(last_x, last_y, x2, y2);
      }
      last_x = x2;
      last_y = y2;
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= y_minor_axis; spacing++) {
  #define number_of_dashes 20
    for (int j = 0; j < number_of_dashes; j++) { // Draw dashed graph grid lines
      if (spacing < y_minor_axis) gfx.drawHorizontalLine((x_pos + 3 + j * gwidth / number_of_dashes), y_pos + (gheight * spacing / y_minor_axis), gwidth / (2 * number_of_dashes));
    }
    if ( (Y1Max-(float)(Y1Max-Y1Min)/y_minor_axis*spacing) < 10) {gfx.drawString(x_pos-2, y_pos+gheight*spacing/y_minor_axis-5, String((Y1Max-(float)(Y1Max-Y1Min)/y_minor_axis*spacing+0.01), 1));}
    else {
      if (Y1Min < 1 && Y1Max < 10) gfx.drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing+0.01), 1));
      else gfx.drawString(x_pos - 2, y_pos + gheight * spacing / y_minor_axis - 5, String((Y1Max - (float)(Y1Max - Y1Min) / y_minor_axis * spacing + 0.01), 0)); // +0.01 prevents -0.00 occurring
    }
  }
  for (int i = 0; i <= 3; i++) {
    gfx.drawString(5 + x_pos + gwidth / 3 * i, y_pos + gheight + 3, String(i));
  }
  gfx.setTextAlignment(TEXT_ALIGN_CENTER);
  gfx.drawString(x_pos+gwidth/2,y_pos+gheight+12,"Days");
  gfx.setTextAlignment(TEXT_ALIGN_LEFT);
}

