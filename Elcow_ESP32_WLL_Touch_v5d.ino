// Screen size = 800 x 480
// CPU Type: ESP32S3-DevModule
// PSRAM: OPI PSRAM Enabled
// Partitiion Shceme: Huge Apa
////////////////////////////////////////////////////////////////////////////////////
String version_num = "Elcrow Davis WLL v5d";
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "credentials.h"  // Contains const char* ssid "YourSSID" and const char* password "YourPassword" or type the two lines below
#include "symbols.h"      // Weather symbols
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
// https://lang-ship.com/blog/files/LovyanGFX/ // To see basic drawing functions
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <driver/i2c.h>

#define screenWidth 800
#define screenHeight 480

float SoC = 0;
WiFiClientSecure client;
HTTPClient http;

String response;
int httpResponseCode;

// Define a class named LGFX, inheriting from the LGFX_Device class.

class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_GT911 _touch_instance;
  LGFX(void) {
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width = screenWidth;
      cfg.memory_height = screenHeight;
      cfg.panel_width = screenWidth;
      cfg.panel_height = screenHeight;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;

      cfg.pin_d0 = GPIO_NUM_15;  // B0
      cfg.pin_d1 = GPIO_NUM_7;   // B1
      cfg.pin_d2 = GPIO_NUM_6;   // B2
      cfg.pin_d3 = GPIO_NUM_5;   // B3
      cfg.pin_d4 = GPIO_NUM_4;   // B4

      cfg.pin_d5 = GPIO_NUM_9;   // G0
      cfg.pin_d6 = GPIO_NUM_46;  // G1
      cfg.pin_d7 = GPIO_NUM_3;   // G2
      cfg.pin_d8 = GPIO_NUM_8;   // G3
      cfg.pin_d9 = GPIO_NUM_16;  // G4
      cfg.pin_d10 = GPIO_NUM_1;  // G5

      cfg.pin_d11 = GPIO_NUM_14;  // R0
      cfg.pin_d12 = GPIO_NUM_21;  // R1
      cfg.pin_d13 = GPIO_NUM_47;  // R2
      cfg.pin_d14 = GPIO_NUM_48;  // R3
      cfg.pin_d15 = GPIO_NUM_45;  // R4

      cfg.pin_henable = GPIO_NUM_41;
      cfg.pin_vsync = GPIO_NUM_40;
      cfg.pin_hsync = GPIO_NUM_39;
      cfg.pin_pclk = GPIO_NUM_0;
      cfg.freq_write = 14000000;

      cfg.hsync_polarity = 0;
      cfg.hsync_front_porch = 40;
      cfg.hsync_pulse_width = 48;
      cfg.hsync_back_porch = 40;

      cfg.vsync_polarity = 0;
      cfg.vsync_front_porch = 1;
      cfg.vsync_pulse_width = 31;
      cfg.vsync_back_porch = 13;

      cfg.pclk_active_neg = 1;
      cfg.de_idle_high = 0;
      cfg.pclk_idle_high = 0;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _light_instance.config();
      cfg.pin_bl = GPIO_NUM_2;
      _light_instance.config(cfg);
      _panel_instance.light(&_light_instance);
    }

    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = 799;
      cfg.y_min = 0;
      cfg.y_max = 479;
      cfg.pin_int = -1;
      cfg.pin_rst = -1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port = I2C_NUM_1;
      cfg.pin_sda = GPIO_NUM_19;
      cfg.pin_scl = GPIO_NUM_20;
      cfg.freq = 400000;
      cfg.i2c_addr = 0x14;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }
    setPanel(&_panel_instance);
  }
};

LGFX lcd;  // LGFX

#define degcode 247
#include <HTTPClient.h>
#include <ArduinoJson.h>  // https://github.com/bblanchon/ArduinoJson

const char* ntpServer = "uk.pool.ntp.org";
const char* Timezone = "GMT0BST,M3.5.0/01,M10.5.0/02";
const long gmtOffset_sec = 0;      // Set your offset in seconds from GMT e.g. if Europe then 3600
const int daylightOffset_sec = 0;  // Set your offset in seconds for Daylight saving

const char* host = "http://192.168.0.41";  // Local address of the Davis WLL 6100
// http://192.168.0.41/v1/current_conditions

enum alignment { LEFT,
                 RIGHT,
                 CENTER };

enum display_mode { _temperature,
                    _humidity,
                    _dewpoint,
                    _windchill,
                    _battery };

float Temperature, Humidity, Dewpoint, AirPressure, Trend, RainFall, Windchill, Windspeed, Winddirection, RainRate;
int TimeStamp, solarradiation, z_month, message;
String UpdateTime, Icon, z_code, forecast;
uint32_t previousMillis = 0;
int loopDelay = 15 * 60000;  // 15-mins
bool Refresh, drawn;

String PressureToCode(int pressure, String trend);
String calc_zambretti(int zpressure, int month, float windDirection, float windSpeed, float Trend);

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(__FILE__);
  lcd.init();
  lcd.setRotation(0);      // 0-3
  lcd.setBrightness(128);  // 0-255
  lcd.setColorDepth(16);   // RGB565
  lcd.fillScreen(lcd.color565(0, 255, 0));
  lcd.setRotation(0);  // 0-3
  lcd.setTextFont(1);  // 1 is the default, 2 is OK but lacks extended chars. 3 is the same as 1, but 4 is OK
  clear_screen();      // Clear screen
  DisplayStatus(0, "Starting WiFi...");
  StartWiFi();  // Start the WiFi service
  DisplayStatus(1, "Started WiFi...");
  DisplayStatus(2, "Starting Time Services...");
  StartTime();
  DisplayStatus(3, "Started Time Services...");
  DisplayStatus(4, "Getting Wx Data...");
  Get_Data();
  DisplayStatus(5, "Got Wx Data...");
  DisplayStatus(6, "Getting Battery Data...");
  GetBatteryData();
  DisplayStatus(7, "Got Battery Data...");
  delay(1000);
  clear_screen();
  lcd.startWrite();
  Refresh = true;  // To get fresh data
  drawn = false;
}

void loop() {
  if (!drawn) {
    lcd.fillRoundRect(700, 0, 100, 50, 12, TFT_CYAN);
    lcd.setTextColor(TFT_BLACK, TFT_CYAN);
    lcd.setTextSize(2);
    lcd.drawString("Refresh", 710, 15);
    drawn = true;
  }
  int32_t x, y;
  lcd.getTouch(&x, &y);
  if (x >= 700 && y <= 50) {
    Refresh = true;
    drawn = false;
  }
  lcd.startWrite();
  if (millis() > previousMillis + loopDelay || Refresh == true || drawn == false) {
    if (Refresh == false) {
      Get_Data();
      GetBatteryData();
    } 
    Refresh = false;
    drawn = false;
    previousMillis = millis();
    clear_screen();  // Clear screen
    drawString(110, 10, UpdateTime, CENTER, TFT_YELLOW, 2);
    drawString(300, 10, version_num, CENTER, TFT_YELLOW, 1);
    display_text(10, 38, String(Temperature, 1) + char(degcode), TFT_GREEN, 3);  // char(247) is ° symbol or 96 is the newfont °
    display_text(125, 42, String(Humidity, 0) + "%", TFT_GREEN, 2);              // char(247) is ° symbol
    display_text(10, 80,  "Dew Point    = " + String(Dewpoint, 1) + char(degcode), TFT_CYAN, 2);
    display_text(10, 103, "Wind Chill   = " + String(Windchill, 1) + char(degcode), TFT_CYAN, 2);
    display_text(10, 126, "Solar rad.   = " + String(solarradiation) + "W/m2", TFT_CYAN, 2);
    display_text(10, 149, "Solar kWhGen = " + String(solarradiation * 1500 / 135 / 1000.0, 2) + "kWh", TFT_CYAN, 2);
    display_text(10, 172, "Rainfall     = " + String(RainFall, 1) + "mm", TFT_CYAN, 2);
    display_text(10, 195, "Rain-Rate    = " + String(RainRate, 1) + "mm/hr", TFT_CYAN, 2);
    String PressureTrend_Str = "Steady";  // Either steady, climbing or falling
    if (Trend > 0.05) PressureTrend_Str = "Rising";
    if (Trend < -0.05) PressureTrend_Str = "Falling";
    display_text(10, 218, "Pressure     = " + String(AirPressure, 0) + "hPa", TFT_CYAN, 2);
    z_code = calc_zambretti(AirPressure, z_month, Winddirection, Windspeed, Trend);
    forecast = ZCode(z_code);
    Serial.println("Forecast         = " + forecast + " (" + z_code + ")");
    display_text(270, 223, " (" + PressureTrend_Str + " " + String(Trend, 2) + "), " + z_code, TFT_CYAN, 1);
    lcd.drawRoundRect(0, 245, 500, 100, 12, TFT_YELLOW);
    display_text(10, 265, forecast, TFT_RED, 2);
    DisplayWindDirection(410, 120, Winddirection, Windspeed, 75, TFT_YELLOW, TFT_RED, 2);
    gauge(50, 420, Temperature, -10, 40, 20, 70, "Temperature", 0.8, _temperature); // Low temperature starts at 20% when below 0° and high at 80% and above 28°
    gauge(175, 420, Humidity, 0, 100, 40, 60, "Humidity", 0.8, _humidity); // Low humidity at 40% and high at 60%
    gauge(300, 420, Dewpoint, -10, 40, 20, 70, "Dew Point", 0.8, _temperature); // Low temperature starts at 20% when below 0° and high at 80% and above 28°
    gauge(425, 420, Windchill, -10, 40, 20, 70, "WindChill", 0.8, _windchill);  // Low temperature starts at 20% when below 0° and high at 80% and above 28°
    gauge(550, 420, SoC, 0, 100, 20, 80, "Battery SoC", 0.8, _battery); // Battery getting low at 20% and fully charged at 80%
    if (WiFi.status() == WL_CONNECTED) display_text(5, 12, "Wi-Fi", TFT_RED, 1);
    else StartWiFi();
  }
}

//----------------------------------------------------------------------------------------------------
void Get_Data() {  //client function to send/receive GET request data.
  String uri = "/v1/current_conditions";
  String Response;
  Serial.println("Connected,\nRequesting data");
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(1000);
    http.begin(host + uri);     // Specify the URL
    int httpCode = http.GET();  // Start connection and send HTTP header
    Serial.println(httpCode);
    if (httpCode > 0) {  // HTTP header has been sent and Server response header has been handled
      if (httpCode == HTTP_CODE_OK) Response = http.getString();
      http.end();
      Serial.println(Response);
    } else {
      http.end();
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      Response = "Station off-air";
    }
    http.end();
  }
  if (Response != "") Decode_Response(Response);
}

void Decode_Response(String input) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, input);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }
  JsonObject data = doc["data"];
  // Get all the Davis WLL 6100 data
  //  const char* data_did = data["did"];  // "001D0A71475C"
  long reading_time = data["ts"];  // 1665396684
  JsonArray data_conditions = data["conditions"];
  JsonObject data_conditions_0 = data_conditions[0];
  //  long data_conditions_0_lsid = data_conditions_0["lsid"];                               // 414281
  //  int data_conditions_0_data_structure_type = data_conditions_0["data_structure_type"];  // 1
  //  int data_conditions_0_txid = data_conditions_0["txid"];                                // 1
  float data_conditions_0_temp = data_conditions_0["temp"];                      // 55.5
  float data_conditions_0_hum = data_conditions_0["hum"];                        // 73.2
  float data_conditions_0_dew_point = data_conditions_0["dew_point"];            // 47.1
                                                                                 //  float data_conditions_0_wet_bulb = data_conditions_0["wet_bulb"];                      // 50.4
                                                                                 //  float data_conditions_0_heat_index = data_conditions_0["heat_index"];                  // 54.8
  float data_conditions_0_wind_chill = data_conditions_0["wind_chill"];          // 55.1
                                                                                 //  float data_conditions_0_thw_index = data_conditions_0["thw_index"];                    // 54.4
                                                                                 //  float data_conditions_0_thsw_index = data_conditions_0["thsw_index"];                  // 54.2
  int data_conditions_0_wind_speed_last = data_conditions_0["wind_speed_last"];  // 6
  int data_conditions_0_wind_dir_last = data_conditions_0["wind_dir_last"];      // 360
                                                                                 //  float data_conditions_0_wind_speed_avg_last_1_min = data_conditions_0["wind_speed_avg_last_1_min"];
                                                                                 //  int data_conditions_0_wind_dir_scalar_avg_last_1_min = data_conditions_0["wind_dir_scalar_avg_last_1_min"];
                                                                                 //  float data_conditions_0_wind_speed_avg_last_2_min = data_conditions_0["wind_speed_avg_last_2_min"];
                                                                                 //  int data_conditions_0_wind_dir_scalar_avg_last_2_min = data_conditions_0["wind_dir_scalar_avg_last_2_min"];
                                                                                 //  int data_conditions_0_wind_speed_hi_last_2_min = data_conditions_0["wind_speed_hi_last_2_min"];  // 10
                                                                                 //  int data_conditions_0_wind_dir_at_hi_speed_last_2_min = data_conditions_0["wind_dir_at_hi_speed_last_2_min"];
                                                                                 //  float data_conditions_0_wind_speed_avg_last_10_min = data_conditions_0["wind_speed_avg_last_10_min"];
                                                                                 //  data_conditions_0["wind_dir_scalar_avg_last_10_min"] is null
                                                                                 //  int data_conditions_0_wind_speed_hi_last_10_min = data_conditions_0["wind_speed_hi_last_10_min"];  // 12
                                                                                 //  int data_conditions_0_wind_dir_at_hi_speed_last_10_min = data_conditions_0["wind_dir_at_hi_speed_last_10_min"];
                                                                                 //  int data_conditions_0_rain_size = data_conditions_0["rain_size"];                                // 2
  int data_conditions_0_rain_rate_last = data_conditions_0["rain_rate_last"];    // 0
                                                                                 //  int data_conditions_0_rain_rate_hi = data_conditions_0["rain_rate_hi"];                          // 0
                                                                                 //  int data_conditions_0_rainfall_last_15_min = data_conditions_0["rainfall_last_15_min"];          // 0
                                                                                 //  int data_conditions_0_rain_rate_hi_last_15_min = data_conditions_0["rain_rate_hi_last_15_min"];  // 0
                                                                                 //  int data_conditions_0_rainfall_last_60_min = data_conditions_0["rainfall_last_60_min"];          // 0
                                                                                 //  int data_conditions_0_rainfall_last_24_hr = data_conditions_0["rainfall_last_24_hr"];            // 13
                                                                                 //  int data_conditions_0_rain_storm = data_conditions_0["rain_storm"];                              // 13
                                                                                 //  long data_conditions_0_rain_storm_start_at = data_conditions_0["rain_storm_start_at"];           // 1665371400
  int data_conditions_0_solar_rad = data_conditions_0["solar_rad"];              // 91
                                                                                 //  float data_conditions_0_uv_index = data_conditions_0["uv_index"];
                                                                                 //  int data_conditions_0_rx_state = data_conditions_0["rx_state"];                      // 0
                                                                                 //  int data_conditions_0_trans_battery_flag = data_conditions_0["trans_battery_flag"];  // 0
  int data_conditions_0_rainfall_daily = data_conditions_0["rainfall_daily"];    // 13
                                                                                 //  int data_conditions_0_rainfall_monthly = data_conditions_0["rainfall_monthly"];      // 72
                                                                                 //  int data_conditions_0_rainfall_year = data_conditions_0["rainfall_year"];            // 1673
                                                                                 //  int data_conditions_0_rain_storm_last = data_conditions_0["rain_storm_last"];        // 13
                                                                                 //  long data_conditions_0_rain_storm_last_start_at = data_conditions_0["rain_storm_last_start_at"];
                                                                                 //  long data_conditions_0_rain_storm_last_end_at = data_conditions_0["rain_storm_last_end_at"];
                                                                                 //  JsonObject data_conditions_1 = data_conditions[1];
                                                                                 //  long data_conditions_1_lsid = data_conditions_1["lsid"];                               // 414278
                                                                                 //  int data_conditions_1_data_structure_type = data_conditions_1["data_structure_type"];  // 4
                                                                                 //  float data_conditions_1_temp_in = data_conditions_1["temp_in"];                        // 70.8
                                                                                 //  float data_conditions_1_hum_in = data_conditions_1["hum_in"];                          // 49.9
                                                                                 //  float data_conditions_1_dew_point_in = data_conditions_1["dew_point_in"];              // 51.2
                                                                                 //  float data_conditions_1_heat_index_in = data_conditions_1["heat_index_in"];            // 69.2
  JsonObject data_conditions_2 = data_conditions[2];
  // long data_conditions_2_lsid = data_conditions_2["lsid"];                               // 414277
  //  int data_conditions_2_data_structure_type = data_conditions_2["data_structure_type"];  // 3
  float data_conditions_2_bar_sea_level = data_conditions_2["bar_sea_level"];  // 30.106
  float data_conditions_2_bar_trend = data_conditions_2["bar_trend"];          // 0.106
                                                                               //  float data_conditions_2_bar_absolute = data_conditions_2["bar_absolute"];              // 29.961

  // doc["error"] is null
  TimeStamp = reading_time;
  Temperature = FtoC(data_conditions_0_temp);
  Humidity = data_conditions_0_hum;
  Dewpoint = FtoC(data_conditions_0_dew_point);
  AirPressure = InchesToHPA(data_conditions_2_bar_sea_level);
  Trend = InchesToHPA(data_conditions_2_bar_trend);  // From inch to hpa
  Windchill = FtoC(data_conditions_0_wind_chill);
  Windspeed = data_conditions_0_wind_speed_last;
  Winddirection = data_conditions_0_wind_dir_last;
  solarradiation = data_conditions_0_solar_rad;
  RainFall = data_conditions_0_rainfall_daily * 0.2;  // Units are 0.2mm
  RainRate = data_conditions_0_rain_rate_last * 0.2;  // Units are 0.2mm
  GetTimeDate();
  Serial.println("Time Stamp       = " + String(TimeStamp));
  Serial.println("Update Time      = " + String(UpdateTime));
  Serial.println("Temperature      = " + String(Temperature));
  Serial.println("Humidity         = " + String(Humidity));
  Serial.println("Dewpoint         = " + String(Dewpoint));
  Serial.println("Air Pressure     = " + String(AirPressure));
  Serial.println("Air Pres. Trend  = " + String(Trend));
  Serial.println("Windchill        = " + String(Windchill));
  Serial.println("Windspeed        = " + String(Windspeed));
  Serial.println("Wind Direction   = " + String(Winddirection));
  Serial.println("Ordinal Wind Dir = " + WindDegToOrdDirection(Winddirection));
  Serial.println("Solar Rad        = " + String(solarradiation));
  Serial.println("Rainfall         = " + String(RainFall));
  Serial.println("Rain Rate        = " + String(RainRate));
}
void DisplayWindDirection(int x, int y, float angle, float windspeed, int Cradius, int color, int arrow_color, int Size) {
  arrow(x, y, Cradius - 21, angle, 15, 25, arrow_color);  // Show wind direction on outer circle of width and length
  int dxo, dyo, dxi, dyi;
  lcd.drawCircle(x, y, Cradius, color);         // Draw compass circle
  lcd.drawCircle(x, y, Cradius + 1, color);     // Draw compass circle
  lcd.drawCircle(x, y, Cradius * 0.75, color);  // Draw compass inner circle
  for (float a = 0; a < 360; a = a + 22.5) {
    dxo = Cradius * cos((a - 90) * PI / 180);
    dyo = Cradius * sin((a - 90) * PI / 180);
    if (a == 45) drawString(dxo + x + 15, dyo + y - 10, "NE", CENTER, color, Size);
    if (a == 135) drawString(dxo + x + 10, dyo + y + 5, "SE", CENTER, color, Size);
    if (a == 225) drawString(dxo + x - 18, dyo + y + 5, "SW", CENTER, color, Size);
    if (a == 315) drawString(dxo + x - 20, dyo + y - 10, "NW", CENTER, color, Size);
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    lcd.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, color);
    dxo = dxo * 0.75;
    dyo = dyo * 0.75;
    dxi = dxo * 0.9;
    dyi = dyo * 0.9;
    lcd.drawLine(dxo + x, dyo + y, dxi + x, dyi + y, color);
  }
  drawString(x - 0, y - Cradius - 18, "N", CENTER, color, Size);
  drawString(x - 0, y + Cradius + 5, "S", CENTER, color, Size);
  drawString(x - Cradius - 12, y - 8, "W", CENTER, color, Size);
  drawString(x + Cradius + 10, y - 8, "E", CENTER, color, Size);
  drawString(x - 2, y - 40, WindDegToOrdDirection(angle), CENTER, color, Size);
  drawString(x - 5, y - 15, String(windspeed, 1), CENTER, color, Size);
  drawString(x - 4, y + 5, "mph", CENTER, color, Size);
  drawString(x - 7, y + 30, String(angle, 0) + char(degcode), CENTER, color, Size);
}
//#########################################################################################
String WindDegToOrdDirection(float winddirection) {
  int dir = int((winddirection / 22.5) + 0.5);
  String Ord_direction[16] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
  return Ord_direction[(dir % 16)];
}

//#########################################################################################
void arrow(int x, int y, int asize, float aangle, int pwidth, int plength, int color) {
  float dx = (asize + 28) * cos((aangle - 90) * PI / 180) + x;  // calculate X position
  float dy = (asize + 28) * sin((aangle - 90) * PI / 180) + y;  // calculate Y position
  float x1 = 0;
  float y1 = plength;
  float x2 = pwidth / 2;
  float y2 = pwidth / 2;
  float x3 = -pwidth / 2;
  float y3 = pwidth / 2;
  float angle = aangle * PI / 180;
  float xx1 = x1 * cos(angle) - y1 * sin(angle) + dx;
  float yy1 = y1 * cos(angle) + x1 * sin(angle) + dy;
  float xx2 = x2 * cos(angle) - y2 * sin(angle) + dx;
  float yy2 = y2 * cos(angle) + x2 * sin(angle) + dy;
  float xx3 = x3 * cos(angle) - y3 * sin(angle) + dx;
  float yy3 = y3 * cos(angle) + x3 * sin(angle) + dy;
  lcd.fillTriangle(xx1, yy1, xx3, yy3, xx2, yy2, color);
}

float FtoC(float Value) {
  return (Value - 32) * 5.0 / 9.0;
}

float InchesToHPA(float Value) {
  return Value * 33.863886666667;
}

String ConvertUnixTime(int unix_time) {
  // http://www.cplusplus.com/reference/ctime/strftime/
  time_t tm = unix_time;
  struct tm* now_tm = gmtime(&tm);
  char output[40];
  strftime(output, sizeof(output), "%m", now_tm);
  z_month = String(output).toInt();
  strftime(output, sizeof(output), "%H:%M   %d/%m/%y", now_tm);
  return output;
}

void clear_screen() {
  lcd.fillScreen(TFT_BLACK);
  lcd.startWrite();
}

void display_text(int x, int y, String text_string, int txt_colour, int txt_size) {
  lcd.setTextColor(txt_colour, TFT_BLACK);
  lcd.setTextSize(txt_size);
  lcd.drawString(text_string, x, y);
  lcd.startWrite();
}

void drawString(int x, int y, String text_string, alignment align, int text_colour, int text_size) {
  int w = 2;  // The width of the font spacing
  lcd.setTextWrap(false);
  lcd.setTextColor(text_colour);
  lcd.setTextSize(text_size);
  if (text_size == 1) w = 4 * text_string.length();
  if (text_size == 2) w = 8 * text_string.length();
  if (text_size == 3) w = 12 * text_string.length();
  if (text_size == 4) w = 16 * text_string.length();
  if (text_size == 5) w = 20 * text_string.length();
  if (align == RIGHT) x = x - w;
  if (align == CENTER) x = x - (w / 2);
  lcd.drawString(text_string, x, y);
  lcd.setTextSize(1);  // Back to default text size
}

void DisplayStatus(int line, String message) {
  display_text(10, 20 + line * 25, message, TFT_GREEN, 2);
  delay(500);
}

void StartWiFi() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);  // switch off AP
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected at: " + WiFi.localIP().toString());
}

void StartTime() {
  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer, "time.nist.gov");  //(gmtOffset_sec, daylightOffset_sec, ntpServer)
  setenv("TZ", Timezone, 1);                                                  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset();                                                                    // Set the TZ environment variable
  delay(100);
}

void GetTimeDate() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 15000)) {
    Serial.println("Failed to obtain time");
  }
  char output[40];
  strftime(output, sizeof(output), "%m", &timeinfo);
  z_month = String(output).toInt();
  strftime(output, sizeof(output), "%H:%M  %d/%m/%y", &timeinfo);
  UpdateTime = output;
}

void DrawButton(int x, int y, int width, int height, int round, int buttonColour, int buttonTextColour, String buttonText) {
  lcd.fillRoundRect(x, y, width, height, round, buttonColour);
  lcd.setTextColor(buttonTextColour);
  lcd.setCursor(x + width / 2 - 10, y + height / 2 - 10);
  lcd.println(buttonText);
}
//##############################################
int CorrectForWind(int zpressure, String windDirection, float windSpeed) {
  if (windSpeed > 0) {
    if (windDirection == "WNW") return zpressure - 0.5;
    if (windDirection == "E") return zpressure - 0.5;
    if (windDirection == "ESE") return zpressure - 2;
    if (windDirection == "W") return zpressure - 3;
    if (windDirection == "WSW") return zpressure - 4.5;
    if (windDirection == "SE") return zpressure - 5;
    if (windDirection == "SW") return zpressure - 6;
    if (windDirection == "SSE") return zpressure - 8.5;
    if (windDirection == "SSW") return zpressure - 10;
    if (windDirection == "S") return zpressure - 12;
    if (windDirection == "NW") return zpressure + 1;
    if (windDirection == "ENE") return zpressure + 2;
    if (windDirection == "NNW") return zpressure + 3;
    if (windDirection == "NE") return zpressure + 5;
    if (windDirection == "NNE") return zpressure + 5;
    if (windDirection == "N") return zpressure + 6;
  }
  return zpressure;
}

String OrdinalWindDir(int dir) {
  int val = int((dir / 22.5) + 0.5);
  String arr[] = { "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW" };
  return arr[(val % 16)];
}

//                      1050 1045 1040 1035 1030 1025 1020 1015 1010 1005 1000 995  990  985  980  975  970  965  960  955
String risingCode[20] = { "A", "A", "A", "A", "A", "B", "B", "B", "C", "F", "G", "I", "J", "L", "M", "Q", "Q", "T", "Y", "Y" };
String fallingCode[20] = { "A", "A", "B", "B", "B", "D", "H", "O", "R", "R", "U", "V", "X", "X", "X", "Z", "Z", "Z", "Z", "Z" };
String steadyCode[20] = { "A", "A", "A", "A", "B", "B", "B", "E", "K", "N", "P", "S", "W", "W", "W", "X", "X", "Z", "Z", "Z" };

String PressureToCode(int pressure, String trend) {
  if (pressure >= 1045) {
    if (trend == "Rising") return risingCode[0];
    if (trend == "Falling") return fallingCode[0];
    if (trend == "Steady") return steadyCode[0];
  }
  if (pressure >= 1040 && pressure < 1045) {
    if (trend == "Rising") return risingCode[1];
    if (trend == "Falling") return fallingCode[1];
    if (trend == "Steady") return steadyCode[1];
  }
  if (pressure >= 1035 && pressure < 1040) {
    if (trend == "Rising") return risingCode[2];
    if (trend == "Falling") return fallingCode[2];
    if (trend == "Steady") return steadyCode[2];
  }
  if (pressure >= 1030 && pressure < 1035) {
    if (trend == "Rising") return risingCode[3];
    if (trend == "Falling") return fallingCode[3];
    if (trend == "Steady") return steadyCode[3];
  }
  if (pressure >= 1025 && pressure < 1030) {
    if (trend == "Rising") return risingCode[4];
    if (trend == "Falling") return fallingCode[4];
    if (trend == "Steady") return steadyCode[4];
  }
  if (pressure >= 1020 && pressure < 1025) {
    if (trend == "Rising") return risingCode[5];
    if (trend == "Falling") return fallingCode[5];
    if (trend == "Steady") return steadyCode[5];
  }
  if (pressure >= 1015 && pressure < 1020) {
    if (trend == "Rising") return risingCode[6];
    if (trend == "Falling") return fallingCode[6];
    if (trend == "Steady") return steadyCode[6];
  }
  if (pressure >= 1010 && pressure < 1015) {
    if (trend == "Rising") return risingCode[7];
    if (trend == "Falling") return fallingCode[7];
    if (trend == "Steady") return steadyCode[7];
  }
  if (pressure >= 1005 && pressure < 1010) {
    if (trend == "Rising") return risingCode[8];
    if (trend == "Falling") return fallingCode[8];
    if (trend == "Steady") return steadyCode[8];
  }
  if (pressure >= 1000 && pressure < 1005) {
    if (trend == "Rising") return risingCode[9];
    if (trend == "Falling") return fallingCode[9];
    if (trend == "Steady") return steadyCode[9];
  }
  if (pressure >= 995 && pressure < 1000) {
    if (trend == "Rising") return risingCode[10];
    if (trend == "Falling") return fallingCode[10];
    if (trend == "Steady") return steadyCode[10];
  }
  if (pressure >= 990 && pressure < 995) {
    if (trend == "Rising") return risingCode[11];
    if (trend == "Falling") return fallingCode[11];
    if (trend == "Steady") return steadyCode[11];
  }
  if (pressure >= 985 && pressure < 990) {
    if (trend == "Rising") return risingCode[12];
    if (trend == "Falling") return fallingCode[12];
    if (trend == "Steady") return steadyCode[12];
  }
  if (pressure >= 980 && pressure < 985) {
    if (trend == "Rising") return risingCode[13];
    if (trend == "Falling") return fallingCode[13];
    if (trend == "Steady") return steadyCode[13];
  }
  if (pressure >= 975 && pressure < 980) {
    if (trend == "Rising") return risingCode[14];
    if (trend == "Falling") return fallingCode[14];
    if (trend == "Steady") return steadyCode[14];
  }
  if (pressure >= 970 && pressure < 975) {
    if (trend == "Rising") return risingCode[15];
    if (trend == "Falling") return fallingCode[15];
    if (trend == "Steady") return steadyCode[15];
  }
  if (pressure >= 965 && pressure < 970) {
    if (trend == "Rising") return risingCode[16];
    if (trend == "Falling") return fallingCode[16];
    if (trend == "Steady") return steadyCode[16];
  }
  if (pressure >= 960 && pressure < 965) {
    if (trend == "Rising") return risingCode[17];
    if (trend == "Falling") return fallingCode[17];
    if (trend == "Steady") return steadyCode[17];
  }
  if (pressure >= 955 && pressure < 960) {
    if (trend == "Rising") return risingCode[18];
    if (trend == "Falling") return fallingCode[18];
    if (trend == "Steady") return steadyCode[18];
  }
  if (pressure < 955) {
    if (trend == "Rising") return risingCode[19];
    if (trend == "Falling") return fallingCode[19];
    if (trend == "Steady") return steadyCode[19];
  }
  return "";
}
//####################################
//# A Winter falling generally results in a Z value lower by 1 unit compared with a Summer falling pressure.
//# Similarly a Summer rising, improves the prospects by 1 unit over a Winter rising.
//#          1050                           950
//#          |                               |
//# Rising   A A A A B B C F G I J L M Q T Y Y
//# Falling  A A A A B B D H O R U V X X Z Z Z
//# Steady   A A A A B B E K N P S W X X Z Z Z

String calc_zambretti(int zpressure, int month, float windDirection, float windSpeed, float Trend) {
  // Correct pressure for time of year
  zpressure = CorrectForWind(zpressure, OrdinalWindDir(windDirection), windSpeed);
  if (Trend <= -0.05) {              // FALLING
    if (month <= 3 || month >= 9) {  // Adjust for Winter
      zpressure -= 3.0;
    }
    return PressureToCode(zpressure, "Falling");
  }
  if (Trend >= 0.05) {               // RISING
    if (month <= 3 || month >= 9) {  // Adjust for Winter
      zpressure -= 3.0;
    }
    return PressureToCode(zpressure, "Rising");
  }
  if (Trend > -0.05 && Trend < 0.05) {  // STEADY
    return PressureToCode(zpressure, "Steady");
  }
  return "";
}

String ZCode(String msg) {
  Serial.println("MSG = " + msg);
  lcd.setSwapBytes(true);
  lcd.setSwapBytes(false);  // バイト順の変換を無効にする。
  int x_pos = 400;
  int y_pos = 255;
  int icon_x_size = 80;
  int icon_y_size = 80;
  String message = "";
  if (msg == "A") {
    //Icon = "https://openweathermap.org/img/wn/01d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_01d_2x);
    message = "Settled fine weather";
  }
  if (msg == "B") {
    //Icon = "https://openweathermap.org/img/wn/01d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_01d_2x);
    message = "Fine weather";
  }
  if (msg == "C") {
    //Icon = "https://openweathermap.org/img/wn/02d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Becoming fine";
  }
  if (msg == "D") {
    //Icon = "https://openweathermap.org/img/wn/02d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Fine, becoming less settled";
  }
  if (msg == "E") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fine, possible showers";
  }
  if (msg == "F") {
    //Icon = "https://openweathermap.org/img/wn/02d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Fairly fine, improving";
  }
  if (msg == "G") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fairly fine, showers early";
  }
  if (msg == "H") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fairly fine, showery later";
  }
  if (msg == "I") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Showery early, improving";
  }
  if (msg == "J") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_02d_2x);
    message = "Changeable, improving";
  }
  if (msg == "K") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Fairly fine, showers likely";
  }
  if (msg == "L") {
    //Icon = "https://openweathermap.org/img/wn/04d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_04d_2x);
    message = "Rather unsettled, clear later";
  }
  if (msg == "M") {
    //Icon = "https://openweathermap.org/img/wn/04d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_04d_2x);
    message = "Unsettled, probably improving";
  }
  if (msg == "N") {
    //Icon = "https://openweathermap.org/img/wn/10d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_10d_2x);
    message = "Showery, bright intervals";
  }
  if (msg == "O") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Showery, becoming unsettled";
  }
  if (msg == "P") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Changeable, some rain";
  }
  if (msg == "Q") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Unsettled, fine intervals";
  }
  if (msg == "R") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Unsettled, rain later";
  }
  if (msg == "S") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Unsettled, rain at times";
  }
  if (msg == "T") {
    //Icon = "https://openweathermap.org/img/wn/04d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_04d_2x);
    message = "Very unsettled, improving";
  }
  if (msg == "U") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Rain at times, worst later";
  }
  if (msg == "V") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Rain, becoming very unsettled";
  }
  if (msg == "W") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Rain at frequent intervals";
  }
  if (msg == "X") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Very unsettled, rain";
  }
  if (msg == "Y") {
    //Icon = "https://openweathermap.org/img/wn/11d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Stormy, may improve";
  }
  if (msg == "Z") {
    //Icon = "https://openweathermap.org/img/wn/09d@2x.png";
    lcd.pushImage(x_pos, y_pos, icon_x_size, icon_y_size, (lgfx::rgb565_t*)Img_09d_2x);
    message = "Stormy, much rain";
  }
  return message;
}

float fmap (float sensorValue, float sensorMin, float sensorMax, float outMin, float outMax) {
  return (sensorValue - sensorMin) * (outMax - outMin) / (sensorMax - sensorMin) + outMin;
}

// gauge(150, 420, temperature, RED, "Temperature", 0.8);
// gauge(150, 420, temperature, RED, "Temperature", 0.8);
void gauge(int x, int y, float value, int minValue, int maxValue, int lowStart, int highStart, String heading, float zoom, display_mode mode) {
  int low_colour, medium_colour, high_colour;
  float MinScale = 0;
  float MaxScale = 100;
  float start_angle = 120;
  float end_angle = 420;
  float Outer_diameter = 50;
  float Inner_diameter = 35;
  float old_value = value;
  String Unit = String(char(degcode));
  if (mode == _temperature || mode == _dewpoint || mode == _windchill) {
    low_colour = TFT_BLUE;
    medium_colour = TFT_GREEN;
    high_colour = TFT_RED;
    value = fmap(value, minValue, maxValue, MinScale, MaxScale);
  }
  if (mode == _humidity) {
    low_colour = TFT_RED;
    medium_colour = TFT_GREEN;
    high_colour = TFT_BLUE;
    value = fmap(value, minValue, maxValue, MinScale, MaxScale);
    Unit = "%";
  }
  if (mode == _battery) {
    low_colour = TFT_RED;
    medium_colour = TFT_GREEN;
    high_colour = TFT_BLUE;
    value = fmap(value, minValue, maxValue, MinScale, MaxScale);
    Unit = "%";
  }
  lcd.drawRoundRect(x - Outer_diameter * zoom - 10, y - Outer_diameter * zoom - 14, Outer_diameter * 2.5 * zoom, Outer_diameter * 2.5 * zoom, 12, TFT_YELLOW);
  lcd.drawArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle, end_angle, TFT_RED);
  if (value / MaxScale >= 0 && value / MaxScale <= lowStart / 100.0) {
    lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle, start_angle + (end_angle - start_angle) * value / MaxScale, low_colour);
  }
  if (value / MaxScale >= lowStart / 100.0) lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle, start_angle + (end_angle - start_angle) * lowStart / MaxScale, low_colour);
  if (value / MaxScale >= lowStart / 100.0 && value / MaxScale <= highStart / 100.0) {
    lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle + (end_angle - start_angle) * lowStart / MaxScale, start_angle + (end_angle - start_angle) * value / MaxScale, medium_colour);
  }
  if (value / MaxScale >= lowStart / 100.0 && value / MaxScale >= highStart / 100.0) lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle + (end_angle - start_angle) * lowStart / MaxScale, start_angle + (end_angle - start_angle) * highStart / MaxScale, medium_colour);
  if (value / MaxScale >= highStart / 100.0 && value / MaxScale <= MaxScale / 100.0) {
    lcd.fillArc(x, y, Outer_diameter * zoom, Inner_diameter * zoom, start_angle + (end_angle - start_angle) * highStart / MaxScale, start_angle + (end_angle - start_angle) * value / MaxScale, high_colour);
  }
  display_text(x - (heading.length() * 6.3) / 2, y - 70 * zoom, heading, TFT_WHITE, 1);
  display_text(x - (String(old_value, 1).length() * 7) / 2, y - 8 * zoom, String(old_value, 1) + Unit, TFT_WHITE, 1);
  if (mode == _temperature || mode == _dewpoint || mode == _windchill) {
    display_text(x + start_angle + (end_angle - start_angle) * lowStart / MaxScale - 228, y - start_angle + (end_angle - start_angle) * 0 / MaxScale + 117, "0", TFT_WHITE, 1);
    //display_text(x - Outer_diameter + 3, y - Outer_diameter + 48, "0", TFT_WHITE, 1);
    display_text(x - Outer_diameter + 8, y - Outer_diameter + 85, String(minValue), TFT_WHITE, 1);
    display_text(x - Outer_diameter + 75, y - Outer_diameter + 85, String(maxValue), TFT_WHITE, 1);
  }
  if (mode == _humidity || mode == _battery) {
    display_text(x - Outer_diameter + 20, y - Outer_diameter + 85, "0", TFT_WHITE, 1);
    display_text(x - Outer_diameter + 75, y - Outer_diameter + 85, "100",  TFT_WHITE, 1);
  }
}
// ###################### Get Battery Data #######################
// ### Start of GivEnergy decoder
void DecodePlant(String input) {
  Serial.println("Decoding Battery Data...");
  Serial.println(input);
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, input);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  JsonObject data = doc["data"];
  JsonObject data_battery = data["battery"];
  SoC = data_battery["percent"];  // 68
  Serial.println("SoC = " + String(SoC) + "%");
}

// ###################### Get Battery Data #######################
void GetBatteryData() {
  Serial.println("Getting Battery data...");
  String url = "https://api.givenergy.cloud/v1/inverter/CE2029G093/system-data/latest/";
  String Auth = BatteryAPIkey; 
  String Content = "application/json";
  String Accept = "application/json";
  String Payload = "inverter_serials?CE2029G093/setting_id?17";
  HTTPClient httpClient;
  httpClient.begin(url);
  int statusCode = httpClient.GET();
  Serial.println(statusCode);
  http.begin(url);  //Specify destination for HTTP request
  http.addHeader("Authorization", Auth);
  http.addHeader("Content-type", Content);
  http.addHeader("Accept", Accept);
  int httpResponseCode = http.GET();
  Serial.println("GOT: " + String(httpResponseCode));
  if (httpResponseCode >= 200) {
    String response = http.getString();  //Get the response to the request
    DecodePlant(response);
  }
  client.stop();
  http.end();
}

// Total of 919 lines

/* {"data":{"did":"001D0A71475C","ts":1665417350,
    "conditions":[{"lsid":414281,
    "data_structure_type":1,"txid":1,"temp": 64.7,"hum":50.0,"dew_point": 45.6,"wet_bulb": 51.8,"heat_index": 62.7,
    "wind_chill": 64.7,"thw_index": 62.7,"thsw_index": 69.6,
    "wind_speed_last":3.00,"wind_dir_last":344,"wind_speed_avg_last_1_min":3.75,"wind_dir_scalar_avg_last_1_min":341,"wind_speed_avg_last_2_min":4.00,
    "wind_dir_scalar_avg_last_2_min":338,"wind_speed_hi_last_2_min":9.00,"wind_dir_at_hi_speed_last_2_min":313,"wind_speed_avg_last_10_min":3.68,
    "wind_dir_scalar_avg_last_10_min":340,"wind_speed_hi_last_10_min":12.00,"wind_dir_at_hi_speed_last_10_min":352,
    "rain_size":2,"rain_rate_last":0,"rain_rate_hi":0,"rainfall_last_15_min":0,"rain_rate_hi_last_15_min":0,"rainfall_last_60_min":0,
    "rainfall_last_24_hr":13,"rain_storm":13,"rain_storm_start_at":1665371400,
    "solar_rad":199,"uv_index":null,"rx_state":0,"trans_battery_flag":0,
    "rainfall_daily":13,"rainfall_monthly":72,"rainfall_year":1673,"rain_storm_last":13,"rain_storm_last_start_at":1664965981,"rain_storm_last_end_at":1665082861},
    {"lsid":414278,
    "data_structure_type":4,"temp_in": 77.5,"hum_in":42.6,"dew_point_in": 53.0,"heat_index_in": 76.8},
    {"lsid":414277,"data_structure_type":3,"bar_sea_level":30.192,"bar_trend": 0.038,"bar_absolute":30.046}]},"error":null}
*/
