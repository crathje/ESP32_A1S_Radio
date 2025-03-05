#include <Arduino.h>

// based on
// https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/examples/ESP32_A1S/ESP32_A1S.ino

// default some DAC
#ifndef DAC2USE_ES8388
#ifndef DAC2USE_AC101
#define DAC2USE_ES8388 1
// #define DAC2USE_AC101 1
#endif
#endif

#ifdef DAC2USE_AC101
#include "AC101.h" // https://github.com/schreibfaul1/AC101
#define DACNAME "AC101"
#endif
#ifdef DAC2USE_ES8388
#define DACNAME "ES8388"
#include "ES8388.h" // https://github.com/maditnerd/es8388
#endif
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>   // https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESP32Encoder.h>        // https://github.com/madhephaestus/ESP32Encoder
#include <ElegantOTA.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <HTTPClient.h>
#include <SD.h>
#include <time.h>
// #include <SPIFFS.h>
#include "main.h"

#include <Wire.h>
TwoWire I2C_2 = TwoWire(1);
#define SSD1306_ADDRESS 0x3C

#include <Adafruit_SSD1306.h>
// #include <U8g2lib.h>

// #define USE_ARDUINO_AUDIO_TOOLS 1
#ifdef USE_ARDUINO_AUDIO_TOOLS
#include "AudioTools.h"
#include "AudioTools/AudioLibs/SPDIFOutput.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#else              // USE_ARDUINO_AUDIO_TOOLS
#include "Audio.h" //https://github.com/schreibfaul1/ESP32-audioI2S
#endif             // USE_ARDUINO_AUDIO_TOOLS

AsyncWebServer asyncWebServer(80);
AsyncWebSocket asyncWebSocket("/ws");
DNSServer dnsServer;
char hostname[32 + 1];
const char compile_date[] = __DATE__ " " __TIME__;
esp_reset_reason_t reason;
char lastPlayedUrl[2048];
char lastPlayedStreamTitle[2048];
char lastPlayedStation[2048];

JsonDocument configJSON;
const char *CONFIGFILENAME = "/radioconf.json";
#define _MAX_JSONBUFFERSIZE 3072

const char *ntpServer = "pool.ntp.org";
long gmtOffset_sec = 0;
const int daylightOffset_sec = 60000;
struct tm timeinfo;

RTC_NOINIT_ATTR int currentStationNum;

// #include <Button2.h>
// Button2 button3, button4;

// SPI GPIOs
#define SD_CS 13
#define SPI_MOSI 15
#define SPI_MISO 2
#define SPI_SCK 14

#ifdef DAC2USE_AC101
// I2S GPIOs, the names refer on AC101, AS1 Audio Kit V2.2 2379
#define I2S_DSIN 35 // pin not used
#define I2S_BCLK 27
#define I2S_LRC 26
#define I2S_MCLK 0
#define I2S_DOUT 25
#endif

#ifdef DAC2USE_ES8388
// I2S GPIOs, the names refer on ES8388, AS1 Audio Kit V2.2 3378
#define I2S_DSIN 35 // pin not used
#define I2S_BCLK 27
#define I2S_LRC 25
#define I2S_MCLK 0
#define I2S_DOUT 26
#endif

// I2C GPIOs
#define IIC_CLK 32
#define IIC_DATA 33

// buttons
// #define BUTTON_2_PIN 13             // shared with SPI_CS
#define BUTTON_3_PIN 19
// #define BUTTON_4_PIN 23
// #define BUTTON_5_PIN 18 // Stop
// #define BUTTON_6_PIN 5  // Play

int8_t IIC_EXTERNAL_CLK = 5;
int8_t IIC_EXTERNAL_DATA = 18;
int IIC_EXTERNAL_DATA_FREQ = 400000;

// amplifier enable
#define GPIO_PA_EN 21

int8_t ROTARY_ENCODER_A_CLK_PIN = 22;
int8_t ROTARY_ENCODER_B_DT_PIN = 19;
int8_t ROTARY_ENCODER_BUTTON_PIN = 23;
int8_t ROTARY_ENCODER_STEPS = 2;
ESP32Encoder rotaryEncoder;

// Switch S1: 1-OFF, 2-ON, 3-ON, 4-ON, 5-ON

#ifdef DAC2USE_AC101
static AC101 dac; // AC101
#endif

#ifdef DAC2USE_ES8388
static ES8388 dac; // ES8388 (new board)
#endif
RTC_NOINIT_ATTR int volume; // 0...100

int8_t GPIO_SPDIFF_OUTPUT = GPIO_NUM_NC;
int8_t GPIO_PA_EN_EXTERNAL = GPIO_NUM_NC;

#ifdef USE_ARDUINO_AUDIO_TOOLS

// const char *urls[] = {
//     "http://stream.srg-ssr.ch/m/rsj/mp3_128?12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"};

URLStream urlStream("ssid", "pass");
// URLStream urlStream(WiFi.cl);
// AudioSourceURL source(urlStream, urls, "audio/mp3");
// AudioSourceURL source(urlStream, "http://localhost/dummy.mp3", "audio/mp3");

MetaDataOutput outMetadata;
I2SStream i2s;
AudioInfo info(44100, 2, 16);
SPDIFOutput spdif;
// MP3DecoderHelix decoder;
// AudioPlayer player(source, i2s, decoder);
// AudioPlayer player(urlStream, i2s, decoder);
EncodedAudioStream outDeci2s(&i2s, new MP3DecoderHelix());      // Decoding stream
EncodedAudioStream out2decSpdif(&spdif, new MP3DecoderHelix()); // Decoding stream
MultiOutput multiOutput;
StreamCopy copier(multiOutput, urlStream); // copy url to decoders

// prototypes from old Audio.h
void audio_info(const char *info);
void audio_id3data(const char *info);
void audio_eof_mp3(const char *info);
void audio_showstation(const char *info);
void audio_showstreamtitle(const char *info);
void audio_bitrate(const char *info);
void audio_commercial(const char *info);
void audio_icyurl(const char *info);
void audio_lasthost(const char *info);
void audio_eof_speech(const char *info);
#else
Audio audio;
#endif

// #ifdef U8X8_HAVE_HW_SPI
// #include <SPI.h>
// #endif
// #ifdef U8X8_HAVE_HW_I2C
// #include <Wire.h>
// #endif/
// U8G2_SSD1306_128X32_UNIVISION_1_2ND_HW_I2C u8g2(U8G2_R0 /* rotation */, /* reset=*/U8X8_PIN_NONE, IIC_EXTERNAL_DATA, IIC_EXTERNAL_CLK);
// U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 4, 5);

uint16_t _screen_width = 128;
uint16_t _screen_height = 32;

Adafruit_SSD1306 *display_ssd1306; // (_screen_width, _screen_height, &I2C_2, -1);
volatile boolean foundSSD1306Display = false;

// #####################################################################

void loadConfigFromFlash()
{
  static const char *TAG = "loadConfigFromFlash";
  File file;
  size_t len = 0;

  bool configFSWorking = false, configLoaded = false;
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS - trying to format.");
    LittleFS.format();
    if (!LittleFS.begin(true))
    {
      Serial.println("..still failing to mount SPIFFS");
    }
    else
    {
      configFSWorking = true;
    }
  }
  else
  {
    configFSWorking = true;
  }
  Serial.printf("LittleFS: %d/%d\n", LittleFS.usedBytes(), LittleFS.totalBytes());
  DeserializationError derr;
  if (configFSWorking)
  {
    file = LittleFS.open(CONFIGFILENAME, FILE_READ);
    if (!file)
    {
      ESP_LOGI(TAG, "No saved config found.");
    }
    else
    {
      ESP_LOGI(TAG, "Found config (%d) in %s", file.size(), CONFIGFILENAME);

      unsigned char content[file.size()];
      len = file.readBytes((char *)content, file.size());
      file.close();
      ESP_LOGI(TAG, "Read %d bytes from file", len);
      ESP_LOG_BUFFER_HEXDUMP(TAG, content, len, ESP_LOG_DEBUG);
      if (len > 0)
      {
        derr = deserializeJson(configJSON, content);
        if (derr == DeserializationError::Ok)
        {
          configLoaded = true;
        }
      }
    }
    LittleFS.end();
  }
  if (!configLoaded)
  {
    derr = deserializeJson(configJSON, _DEFAULT_CONFIG);
    if (derr == DeserializationError::Ok)
    {
      configLoaded = true;
    }
  }
  Serial.printf("configLoaded: %d size: %d stations: %d\r\n", configLoaded, configJSON.size(), configJSON["stations"].size());
}

void setAmpEnabled(boolean enable)
{
  // Enable amplifier
  if (GPIO_PA_EN != GPIO_NUM_NC)
  {
    pinMode(GPIO_PA_EN, OUTPUT);
    digitalWrite(GPIO_PA_EN, enable ? HIGH : LOW);
  }
}

int getUTCOffsetMinutes()
{
  HTTPClient http;
  http.setTimeout(2000);
  int utcoffsetminutes = 0;
  // this includes DST!
  if (http.begin("http://iot.thje.net/time.php?utcoffset&tz=Europe%2FBerlin"))
  {
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      utcoffsetminutes = payload.toInt();
      // timeClient.setTimeOffset(utcoffsetminutes);
      ESP_LOGI("getUTCOffsetMinutes()", "UTC OFFSET: %dmin", utcoffsetminutes);
      http.end();
    }
    else
    {
      ESP_LOGE("getUTCOffsetMinutes()", "HTTP CODE: %d", httpCode);
      return utcoffsetminutes;
    }
  }
  else
  {
    ESP_LOGE("getUTCOffsetMinutes()", "Failed to get UTC Offset");
  }
  return utcoffsetminutes;
}

void drawDisplay()
{
  if (foundSSD1306Display)
  {
    display_ssd1306->clearDisplay();
    display_ssd1306->setTextColor(WHITE);
    display_ssd1306->setTextSize(1);
    display_ssd1306->setCursor(0, 0);
    String lastPlayedStationS = String(lastPlayedStation);
    char buff[100];
    int16_t x1, y1;
    uint16_t w = _screen_width + 1, h;
    display_ssd1306->setTextWrap(false);
    sprintf(buff, "%s%s", currentStationNum < 0 ? "" : "" + String(currentStationNum) + ": ", //
            lastPlayedStation);
    display_ssd1306->println(buff);
    display_ssd1306->setTextWrap(true);
    display_ssd1306->println(lastPlayedStreamTitle);

    display_ssd1306->setCursor(0, _screen_height - 7);
    getLocalTime(&timeinfo);
    strftime(buff, sizeof(buff), timeinfo.tm_sec % 2 == 0 ? "%H:%M" : "%H %M", &timeinfo);
    display_ssd1306->println(buff);

    display_ssd1306->getTextBounds(buff, 0, 0, &x1, &y1, &w, &h);

    int volBarX = w - x1 + 3;
    int mapped = map(volume, 0, 100, 0, _screen_width - volBarX);
    for (int y = _screen_height - 4; y < _screen_height - 1; y++)
    {
      display_ssd1306->drawLine(volBarX, y, volBarX + mapped, y, WHITE);
      display_ssd1306->drawLine(volBarX + mapped + 1, y, _screen_width - 1, y, BLACK);
    }
    display_ssd1306->drawLine(volBarX, _screen_height - 1, _screen_width, _screen_height - 1, WHITE);

    display_ssd1306->display();
  }
}

void setVolume(int value, boolean justTemporary = false)
{
  // Serial.printf("Volume set to: %d\r\n", value);
  if (value > 100)
  {
    value = 100;
  }
  if (value < 0)
  {
    value = 0;
  }

  if (!justTemporary)
  {
    volume = value;
  }
#ifdef DAC2USE_ES8388
  dac.volume(ES8388::ES_MAIN, value);
  dac.volume(ES8388::ES_OUT1, value);
  dac.volume(ES8388::ES_OUT2, value);
  dac.mute(ES8388::ES_OUT1, false);
  dac.mute(ES8388::ES_OUT2, false);
  dac.mute(ES8388::ES_MAIN, false);
#endif

#ifdef DAC2USE_AC101
  dac.SetVolumeSpeaker(value);
  dac.SetVolumeHeadphone(value);
#endif

  if (!justTemporary)
  {
    asyncWebSocket.textAll("V\t" + String(volume));
    drawDisplay();
  }
}

void actionPlayPause()
{
#ifdef USE_ARDUINO_AUDIO_TOOLS
  // TODO: implement
#else
  audio.pauseResume();
#endif
}
void actionVolumeUp() { setVolume(volume + 1); }
void actionVolumeDown() { setVolume(volume - 1); }

void actionUpdateStarting()
{
  setAmpEnabled(false);

// mute!
#ifdef DAC2USE_ES8388
  dac.mute(ES8388::ES_OUT1, true);
  dac.mute(ES8388::ES_OUT2, true);
  dac.mute(ES8388::ES_MAIN, true);
#endif

#ifdef USE_ARDUINO_AUDIO_TOOLS
  // TODO: implement
  // player.stop();
  copier.setActive(false);
#else
  audio.stopSong();
#endif

  if (foundSSD1306Display)
  {
    display_ssd1306->clearDisplay();
    display_ssd1306->setTextColor(WHITE);
    display_ssd1306->setTextSize(2);
    display_ssd1306->setCursor(0, 0);
    display_ssd1306->println("Updating");
    display_ssd1306->display();
  }
  asyncWebSocket.textAll("C\tOTA Updating...");
  asyncWebSocket.textAll("AINF\tOTA Updating...");
  asyncWebSocket.textAll("ASTT\tOTA Updating...");
}

// void buttonClick(Button2 &btn)
// {
//   if (btn == button4)
//   {
//   }
//   // else if (btn == button5)
//   // {
//   //   actionPlayPause();
//   // }
//   // else if (btn == button6)
//   // {
//   //   actionVolumeUp();
//   // }
// }

#ifdef USE_ARDUINO_AUDIO_TOOLS

// callback for meta data
void printMetaData(MetaDataType type, const char *str, int len)
{
  Serial.print("==> ");
  Serial.print(toStr(type));
  Serial.print(": ");
  Serial.println(str);

  // if (foundSSD1306Display)
  // {
  //   if (type == MetaDataType::StreamTitle)
  //   {
  //     display_ssd1306->clearDisplay();
  //     display_ssd1306->setTextColor(WHITE);
  //     display_ssd1306->setTextSize(1);
  //     display_ssd1306->setCursor(0, 0);
  //     display_ssd1306->println(toStr(type));
  //     display_ssd1306->println(str);
  //     drawDisplay();
  //   }
  // }
}
#endif

void playUrl(const char *url)
{
  int oldVol = volume;
  setVolume(0, true);
  // setAmpEnabled(false);
  audio_showstreamtitle("");
  lastPlayedStation[0] = 0;

  JsonArray array = configJSON["stations"].as<JsonArray>();
  int i = 0;
  currentStationNum = -1;
  for (JsonVariant v : array)
  {
    if (!strncmp(url, v["url"].as<String>().c_str(), strlen(url)))
    {
      // Serial.print("FOUND::: ");
      currentStationNum = i;
      sprintf(lastPlayedStation, "%s", v["name"].as<String>().c_str());
    }
    // Serial.println(v.as<string>().c_str());
    i++;
  }

  String surl = String(url);
  if (strlen(lastPlayedStation) == 0)
  {
    sprintf(lastPlayedStation, "%s", surl.substring(surl.lastIndexOf("/") + 1).c_str());
  }
  audio_showstation(lastPlayedStation);
  strcpy(lastPlayedUrl, url);
  asyncWebSocket.textAll("C\t" + String(url));

#ifdef USE_ARDUINO_AUDIO_TOOLS
  // urls[0] = url;
  // source = AudioSourceURL(urlStream, urls, "audio/mp3");
  // source.selectStream(0);
  // player.stop();
  // source.urlArray[0] = url;
  // urlStream.urlArray
  // source.selectStream(url);
  // player.play();
  // urlStream.begin(url,"audio/mp3");
  copier.setActive(false);

  if (surl.lastIndexOf(".m3u") == surl.length() - 4)
  {
    Serial.printf("playUrl:: playlist detected\r\n");

    HTTPClient http;
    if (http.begin(url))
    {
      int httpCode = http.GET();
      // Serial.printf("playUrl:: httpCode %d\r\n", httpCode);
      if (httpCode > 0)
      {
        // file found at server
        if (httpCode == HTTP_CODE_OK)
        {
          String payload = http.getString();
          // Serial.printf("playUrl:: payload %s\r\n", payload.c_str());

          // assume easiest format
          payload.trim();
          surl = payload;
        }
        http.end();
      }
    }
  }

  urlStream.begin(surl.c_str());
  copier.setActive(true);
#else
  audio.connecttohost(url);
#endif

  // setAmpEnabled(true);
  setVolume(oldVol, true);
}

void changeStationIndex(int newIndex)
{
  if (currentStationNum == newIndex)
  {
    return;
  }
  JsonArray array = configJSON["stations"].as<JsonArray>();
  int i = 0;
  for (JsonVariant v : array)
  {
    if (i == newIndex)
    {
      currentStationNum = newIndex;
      playUrl(v["url"].as<String>().c_str());
      return;
    }
    i++;
  }
  // not found, not changing anything
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  switch (type)
  {
  case WS_EVT_CONNECT:
    // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    client->text("C\t" + String(lastPlayedUrl));
    client->text("ASTT\t" + String(lastPlayedStreamTitle));
    client->text("ASTA\t" + String(lastPlayedStation));
    client->text("V\t" + String(volume));
    break;
  case WS_EVT_DISCONNECT:
    // Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    // handleWebSocketMessage(arg, data, len);
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
      data[len] = 0;
      if (strcmp((char *)data, "stationdown") == 0)
      {
        changeStationIndex(currentStationNum - 1);
      }
      else if (strcmp((char *)data, "stationup") == 0)
      {
        changeStationIndex(currentStationNum + 1);
      }
      else if (strcmp((char *)data, "playpause") == 0)
      {
        actionPlayPause();
      }
      else if (strcmp((char *)data, "voldown") == 0)
      {
        setVolume(volume - 1);
      }
      else if (strcmp((char *)data, "volup") == 0)
      {
        setVolume(volume + 1);
      }
    }
    break;
  case WS_EVT_PONG:
    break;
  case WS_EVT_ERROR:
    break;
  }
}

int loadIntFromConfig(const char *key, int defaultValue)
{
  if (configJSON[key].is<int>())
  {
    return configJSON[key].as<int>();
  }
  else
  {
    configJSON[key] = defaultValue;
  }
  return defaultValue;
}

void setup()
{
  reason = esp_reset_reason();

  if ((reason == ESP_RST_POWERON) || (reason == ESP_RST_UNKNOWN))
  {
    currentStationNum = -1;
    volume = 70;
  }

  Serial.begin(115200);
  Serial.println("\r\nReset");

  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC
                                       // address(length: 6 bytes).
  Serial.println(F("===== CHIP INFO ====="));
  Serial.printf_P(PSTR("ChipID:            %04X%08X\r\n"),
                  (uint16_t)(chipid >> 32), (uint32_t)chipid);
  Serial.printf_P(PSTR("ChipCores:         %d\r\n"), ESP.getChipCores());
  Serial.printf_P(PSTR("CpuFreqMHz:        %3d\r\n"), ESP.getCpuFreqMHz());
  Serial.printf_P(PSTR("ChipRevision:      %d\r\n"), ESP.getChipRevision());
  Serial.printf_P(PSTR("FreeHeap:          %3d\r\n"), ESP.getFreeHeap());
  Serial.printf_P(PSTR("CycleCount:        %3d\r\n"), ESP.getCycleCount());
  Serial.printf_P(PSTR("SdkVersion:        %s\r\n"), ESP.getSdkVersion());
  Serial.printf_P(PSTR("FlashChipSize:     %3d\r\n"), ESP.getFlashChipSize());
  // DebugHWSerial.printf("FlashChipRealSize: %3d\n"),
  // ESP.getFlashChipRealSize());
  Serial.printf_P(PSTR("FlashChipSpeed:    %3d\r\n"), ESP.getFlashChipSpeed());
  Serial.printf_P(PSTR("FlashChipMode:     %d\r\n"), ESP.getFlashChipMode());
  Serial.printf_P(PSTR("SketchSize:        %d\r\n"), ESP.getSketchSize());
  Serial.printf_P(PSTR("FreeSketchSpace:   %d\r\n"), ESP.getFreeSketchSpace());
  Serial.printf_P(PSTR("ArduinoStack:      %d\r\n"),
                  getArduinoLoopTaskStackSize());
  Serial.printf_P(PSTR("ArduinoVersion:    %d.%d.%d\r\n"),
                  ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR,
                  ESP_ARDUINO_VERSION_PATCH);

  // pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  // pinMode(BUTTON_4_PIN, INPUT_PULLUP);
  // pinMode(BUTTON_5_PIN, INPUT_PULLUP);
  // pinMode(BUTTON_6_PIN, INPUT_PULLUP);

  // button3.begin(BUTTON_3_PIN);
  // button3.setClickHandler(buttonClick);
  // button4.begin(BUTTON_4_PIN);
  // button4.setClickHandler(buttonClick);
  // button5.begin(BUTTON_5_PIN);
  // button5.setClickHandler(buttonClick);
  // button6.begin(BUTTON_6_PIN);
  // button6.setClickHandler(buttonClick);

  loadConfigFromFlash();

  // only load volume from conifg if not reset
  if ((reason == ESP_RST_POWERON) || (reason == ESP_RST_UNKNOWN))
  {
    volume = loadIntFromConfig("volume", volume);
  }
  _screen_width = loadIntFromConfig("SCREEN_WIDTH", _screen_width);
  _screen_height = loadIntFromConfig("SCREEN_HEIGHT", _screen_height);
  IIC_EXTERNAL_CLK = loadIntFromConfig("IIC_EXTERNAL_CLK", IIC_EXTERNAL_CLK);
  IIC_EXTERNAL_DATA = loadIntFromConfig("IIC_EXTERNAL_DATA", IIC_EXTERNAL_DATA);
  ROTARY_ENCODER_A_CLK_PIN = loadIntFromConfig("ROTARY_ENCODER_A_CLK_PIN", ROTARY_ENCODER_A_CLK_PIN);
  ROTARY_ENCODER_B_DT_PIN = loadIntFromConfig("ROTARY_ENCODER_B_DT_PIN", ROTARY_ENCODER_B_DT_PIN);
  ROTARY_ENCODER_BUTTON_PIN = loadIntFromConfig("ROTARY_ENCODER_BUTTON_PIN", ROTARY_ENCODER_BUTTON_PIN);
  ROTARY_ENCODER_STEPS = loadIntFromConfig("ROTARY_ENCODER_STEPS", ROTARY_ENCODER_STEPS);
  GPIO_SPDIFF_OUTPUT = loadIntFromConfig("GPIO_SPDIFF_OUTPUT", GPIO_SPDIFF_OUTPUT);
  GPIO_PA_EN_EXTERNAL = loadIntFromConfig("GPIO_PA_EN_EXTERNAL", GPIO_PA_EN_EXTERNAL);

  display_ssd1306 = new Adafruit_SSD1306(_screen_width, _screen_height, &I2C_2, -1);

  if (IIC_EXTERNAL_DATA != GPIO_NUM_NC && IIC_EXTERNAL_CLK != GPIO_NUM_NC)
  {
    Serial.printf("Starting 2nd I2C on data: %d clk: %d...", IIC_EXTERNAL_DATA, IIC_EXTERNAL_CLK);
    I2C_2.begin(IIC_EXTERNAL_DATA, IIC_EXTERNAL_CLK, IIC_EXTERNAL_DATA_FREQ);
    Serial.printf("OK\r\n");

    Serial.println();
    Serial.println("I2C scanner. Scanning ...");
    uint8_t count = 0;

#define MIN_I2C_DEVICE_ADDR 8
#define MAX_I2C_DEVICE_ADDR 120
    // for (uint8_t i = MIN_I2C_DEVICE_ADDR; i < MAX_I2C_DEVICE_ADDR; i++)
    for (uint8_t i = SSD1306_ADDRESS; i < SSD1306_ADDRESS + 1; i++)
    {
      I2C_2.beginTransmission(i);
      if (I2C_2.endTransmission() == 0)
      {
        Serial.printf("Found 0x%02x\r\n", i);
        if (i == SSD1306_ADDRESS)
        {
          foundSSD1306Display = true;
        }
      }
      else
      {
        // Serial.printf("Absent 0x%02x", i);
      }
    }
    Serial.printf("DONE\r\n");
  }

  if (foundSSD1306Display)
  {
    Serial.printf("SSD1306 display init (0x%02x)...", SSD1306_ADDRESS);
    // fonts see https://github.com/olikraus/u8g2/wiki/fntlist8
    // if (u8g2.begin())
    // {
    //   Serial.println(F("OK"));
    //   u8g2.setPowerSave(0);
    //   u8g2.clearBuffer();
    //   u8g2.setFont(u8g2_font_ncenB08_tr);
    //   u8g2.drawStr(0, 10, "Booting...");
    //   u8g2.setFont(u8g2_font_squeezed_r7_tr);
    //   u8g2.drawStr(0, 20, compile_date);
    //   u8g2.sendBuffer();
    //   u8g2.setFont(u8g2_font_ncenB08_tr);
    // }
    // else
    // {
    //   Serial.println(F("FAILED"));
    // }
    // #define SSD1306_EXTERNALVCC 0x01  ///< External display voltage source
    // #define SSD1306_SWITCHCAPVCC 0x02 ///< Gen. display voltage from 3.3V
    display_ssd1306->begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDRESS);
    display_ssd1306->dim(false);
    // display.ssd1306_command(SSD1306_SETCONTRAST);
    // display.ssd1306_command(0xFF);

    display_ssd1306->clearDisplay();
    display_ssd1306->display();

    display_ssd1306->setTextColor(WHITE);
    display_ssd1306->setTextSize(2);
    display_ssd1306->setCursor(0, 0);
    display_ssd1306->println("Booting...");
    display_ssd1306->setTextSize(1);
    display_ssd1306->setCursor(0, _screen_height - 7);
    display_ssd1306->println(compile_date);
    display_ssd1306->display();
  }

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);

  Serial.println(F("===== SD CARD INFO ====="));
  if (SD.begin(SD_CS))
  {
    Serial.printf_P(PSTR("cardType:   %d\r\n"), SD.cardType());
    Serial.printf_P(PSTR("cardSize:   %lld\r\n"), SD.cardSize());
    Serial.printf_P(PSTR("totalBytes: %lld\r\n"), SD.totalBytes());
    Serial.printf_P(PSTR("usedBytes:  %lld\r\n"), SD.usedBytes());
  }
  else
  {
    Serial.println(F("SD Init failed."));
  }

  AsyncWiFiManager wifiManager(&asyncWebServer, &dnsServer);
  sprintf(hostname, "A1S-Radio-%04X%08X", (uint16_t)(chipid >> 32),
          (uint32_t)chipid);
  WiFi.setHostname(hostname);
  wifiManager.autoConnect(hostname, NULL);

  // Init and get the time
  gmtOffset_sec = getUTCOffsetMinutes() * 60;
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  asyncWebServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                    { 
                      static const size_t htmlContentLength = strlen_P(index_html);

                      request->send(200, "text/html", (uint8_t *)index_html, htmlContentLength); });
  asyncWebServer.on("/config.json", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    // JsonObject root = configJSON.to<JsonObject>();
    serializeJsonPretty(configJSON, *response);
    request->send(response); });
  asyncWebServer.on("/config.json", HTTP_POST, [](AsyncWebServerRequest *request)
                    {
                int params = request->params();
                for (int i = 0; i < params; i++) {
                  const AsyncWebParameter* p = request->getParam(i);
                  Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
                  if (p->name() == "config") {
                    // if (validateJson(p->value())) {
                      DeserializationError dret = deserializeJson(configJSON, p->value());
                      if (dret == DeserializationError::Ok) {
                        if (LittleFS.begin(true)) {                            
                          File file = LittleFS.open(CONFIGFILENAME, "w", true);
                          if (file)
                          {
                            serializeJson(configJSON, file);
                            file.close();
                            Serial.println("Config has been saved");
                          }
                          request->redirect("/config");
                          LittleFS.end();
                          return;
                        } else {
                          request->send(500, "text/html", "Could not mount LittleFS");
                          return;
                        }
                      }
                    // } 
                  }
                }
                request->send(500, "text/html", "Failed"); });
  asyncWebServer.on("/config", HTTP_GET, [](AsyncWebServerRequest *request)
                    { 
                char json[_MAX_JSONBUFFERSIZE];
                serializeJsonPretty(configJSON, json);
                request->send(200, "text/html", String() + "<html><body><form action=\"/config.json\" method=\"POST\"><textarea name=\"config\" cols=\"100\" rows=\"35\">" + json + "</textarea><br /><button>Save</button></form></body></html>"); });

  asyncWebServer.on("/playpause", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
    request->send(200, "text/plain", "OK");
    actionPlayPause(); });
  asyncWebServer.on("/amp", HTTP_GET, [](AsyncWebServerRequest *request)
                    {  if (request->hasParam("enable")) {
                        int v = request->getParam("enable")->value().toInt();   
                        setAmpEnabled(v == 1);
                        request->send(200, "text/plain", "OK");
                        return;
                      }
                      request->send(200, "text/plain", "FAILED"); });
  asyncWebServer.on("/playurl", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
      if (request->hasParam("playurl")) {
        String url = request->getParam("playurl")->value();
        playUrl(url.c_str());
        request->send(200, "text/plain", "OK");
        return;
      }
      request->send(200, "text/plain", "FAILED"); });
  asyncWebServer.on("/volume", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
        if (request->hasParam("v")) {
          int v = request->getParam("v")->value().toInt();
          setVolume(v);
          request->send(200, "text/plain", "OK");   
          return;
        }
        request->send(200, "text/plain", "FAILED"); });
  asyncWebServer.on("/volup", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
    request->send(200, "text/plain", "OK");
    actionVolumeUp(); });
  asyncWebServer.on("/voldown", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
      request->send(200, "text/plain", "OK");
      actionVolumeDown(); });
  asyncWebServer.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
    request->send(200, "text/plain", "OK");
    ESP.restart(); });

  asyncWebServer.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
                      JsonDocument responseJson;
                      // responseJson["lastPlayedUrl"] = lastPlayedUrl;
                      responseJson["lastPlayedStation"] = lastPlayedStation;
                      responseJson["rssi"] = WiFi.RSSI();
                      responseJson["hostname"] = WiFi.getHostname();
                      responseJson["SSID"] = WiFi.SSID();
                      responseJson["compile_date"] = compile_date;
                      responseJson["sdk_version"] = ESP.getSdkVersion();
                      responseJson["FreeHeap"] = ESP.getFreeHeap();
                      responseJson["MinFreeHeap"] = ESP.getMinFreeHeap();
                      responseJson["FreeSketchSpace"] = ESP.getFreeSketchSpace();
#ifdef BOARD_HAS_PSRAM
                      responseJson["FreePsram"] = ESP.getFreePsram();
                      responseJson["MinFreePsram"] = ESP.getMinFreePsram();
#endif
                      if (LittleFS.begin(true)) {    
                        responseJson["LittleFSTotal"] = LittleFS.totalBytes();
                        responseJson["LittleFSUsed"] = LittleFS.usedBytes();
                        LittleFS.end();
                      }

                      responseJson["gmtOffset_sec"] = gmtOffset_sec;
                      responseJson["daylightOffset_sec"] = daylightOffset_sec;
                      
                      responseJson["DAC"] = DACNAME;
                      responseJson["PIO_ENV"] = PIO_ENV;
                      responseJson["uptime"] = millis();
                      responseJson["resetReason"] = reason;
                      AsyncResponseStream *response = request->beginResponseStream("application/json");
                      serializeJsonPretty(responseJson, *response);
                      request->send(response); });

  asyncWebSocket.onEvent(onWebSocketEvent);
  asyncWebServer.addHandler(&asyncWebSocket);

  asyncWebServer.begin();

  Serial.printf_P(PSTR("Connected\r\nRSSI: "));
  Serial.print(WiFi.RSSI());
  Serial.print(" IP: ");
  Serial.print(WiFi.localIP());
  Serial.print(" Hostname: ");
  Serial.println(WiFi.getHostname());

  ElegantOTA.begin(&asyncWebServer); // Start ElegantOTA
  ElegantOTA.onStart(actionUpdateStarting);

  ArduinoOTA
      .onStart([]()
               {
        actionUpdateStarting();

        String type;
        if (ArduinoOTA.getCommand() == U_FLASH)
          type = "sketch";
        else  // U_SPIFFS
          type = "filesystem";

        // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS
        // using SPIFFS.end()
        Serial.println("Start updating " + type); })
      .onError([](ota_error_t error)
               {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
          Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
          Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
          Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
          Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
          Serial.println("End Failed"); });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
                          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
                          if (foundSSD1306Display)
                          {
                            int mapped = map((progress / (total / 100)), 0, 100, 0, _screen_width);
                            for (int y = _screen_height - 3; y < _screen_height; y++)
                            {
                              display_ssd1306->drawLine(0, y, mapped, y, WHITE);
                              display_ssd1306->drawLine(mapped + 1, y, _screen_width - 1, y, BLACK);
                            }
                            display_ssd1306->display();
                          } });
  ArduinoOTA.setHostname(wifiManager.getConfiguredSTASSID().c_str());
  //  ArduinoOTA.setPassword("secretPass");
  ArduinoOTA.begin();

  Serial.printf("Connect to DAC codec... ");
  while (!dac.begin(IIC_DATA, IIC_CLK))
  {
    Serial.printf("Failed!\n");
    delay(1000);
  }
  Serial.printf("OK\r\n");

  setVolume(volume);

  //  ac.DumpRegisters();
#ifdef USE_ARDUINO_AUDIO_TOOLS

  AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);

  // // setup output
  if (GPIO_SPDIFF_OUTPUT != GPIO_NUM_NC)
  {
    // hardware similar to https://github.com/pschatzmann/ESP32-A2DP/discussions/137#discussioncomment-2261009
    auto spdifcfg = spdif.defaultConfig();
    spdifcfg.copyFrom(info);
    spdifcfg.buffer_size = 384;
    spdifcfg.buffer_count = 30;
    spdifcfg.pin_data = GPIO_SPDIFF_OUTPUT;
    spdif.begin(spdifcfg);
    multiOutput.add(out2decSpdif);
  }
  else
  {
    // setup output
    auto cfg = i2s.defaultConfig(TX_MODE);
    cfg.pin_bck = I2S_BCLK;
    cfg.pin_data = I2S_DOUT;
    cfg.pin_data_rx = I2S_GPIO_UNUSED;
    cfg.pin_mck = I2S_MCLK;
    cfg.pin_ws = I2S_LRC;
    i2s.begin(cfg);
    multiOutput.add(outDeci2s);
  }

  multiOutput.add(outMetadata);

  // setup input
  // urlStream.begin("https://pschatzmann.github.io/Resources/audio/audio.mp3","audio/mp3");
  // urlStream.begin("http://regiocast.streamabc.net/regc-boblivesh-mp3-128-1782603?sABC=67n9s8n2%230%23q6osp9r5o1372rpon5s444s16o34pn4q%23fgernzf.enqvbobo.qr&aw_0_1st.playerid=streams.radiobob.de&amsparams=playerid:streams.radiobob.de;skey:1739192482", "audio/mp3");

  // setup metadata
  outMetadata.setCallback(printMetaData);
  // outMetadata.begin(urlStream.httpRequest());
  outMetadata.begin();

  // set up source
  // source.setTimeout(800);
  // urlStream.setTimeout(800);

  // setup player
  // begin is called from changestation
  // player.begin();

  // setup I2S based on sampling rate provided by decoder

  if (GPIO_SPDIFF_OUTPUT != GPIO_NUM_NC)
  {
    out2decSpdif.begin();
  }
  else
  {
    outDeci2s.begin();
  }
#else // USE_ARDUINO_AUDIO_TOOLS
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(15); // 0...21
#endif

  // audio.connecttoFS(SD, "/320k_test.mp3");
  // audio.connecttoSD("/Banana Boat Song - Harry Belafonte.mp3");
  // audio.connecttohost("http://s1-webradio.antenne.de/oldie-antenne");
  // audio.connecttohost("https://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true");
  // audio.connecttohost("http://dg-rbb-http-dus-dtag-cdn.cast.addradio.de/rbb/antennebrandenburg/live/mp3/128/stream.mp3");
  // audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");

  if (ROTARY_ENCODER_A_CLK_PIN != GPIO_NUM_NC && ROTARY_ENCODER_A_CLK_PIN != GPIO_NUM_NC)
  {
    rotaryEncoder.attachHalfQuad(ROTARY_ENCODER_B_DT_PIN, ROTARY_ENCODER_A_CLK_PIN);
    rotaryEncoder.setCount(0);
  }

  if (ROTARY_ENCODER_BUTTON_PIN != GPIO_NUM_NC)
  {
    pinMode(ROTARY_ENCODER_BUTTON_PIN, INPUT_PULLUP);
  }

  if (GPIO_PA_EN_EXTERNAL != GPIO_NUM_NC)
  {
    pinMode(GPIO_PA_EN_EXTERNAL, INPUT_PULLUP);
  }


  memset(lastPlayedUrl, sizeof(lastPlayedUrl), 0);
  memset(lastPlayedStreamTitle, sizeof(lastPlayedStreamTitle), 0);

  // playUrl("http://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true");
  setAmpEnabled(true);

  // after restart resume playing where we left
  if (currentStationNum < 0)
  {
    changeStationIndex(0);
  }
  else
  {
    int realStationNum = currentStationNum;
    currentStationNum = currentStationNum - 1;
    changeStationIndex(realStationNum);
  }
}

//-----------------------------------------------------------------------

void loop()
{
#ifdef USE_ARDUINO_AUDIO_TOOLS
  // player.copy();
  copier.copy();
#else  // USE_ARDUINO_AUDIO_TOOLS
  audio.loop();
#endif // USE_ARDUINO_AUDIO_TOOLS
  // button3.loop();
  // button4.loop();
  // button5.loop();
  // button6.loop();
  ArduinoOTA.handle();
  ElegantOTA.loop();

  if (foundSSD1306Display)
  {
    getLocalTime(&timeinfo);
    static uint8_t secondsWhenShown = timeinfo.tm_sec;
    if (secondsWhenShown != timeinfo.tm_sec)
    {
      secondsWhenShown = timeinfo.tm_sec;
      drawDisplay();
    }
  }

  if (GPIO_PA_EN_EXTERNAL != GPIO_NUM_NC)
  {
    if (digitalRead(GPIO_PA_EN_EXTERNAL) == LOW)
    {
      setAmpEnabled(false);
    }
  }

  if (ROTARY_ENCODER_A_CLK_PIN != GPIO_NUM_NC && ROTARY_ENCODER_A_CLK_PIN != GPIO_NUM_NC)
  {
    int64_t val = rotaryEncoder.getCount();
    bool rotaryPressed = false;
    if (ROTARY_ENCODER_BUTTON_PIN != GPIO_NUM_NC)
    {
      // pullup enabled, so level LOW is pressed
      rotaryPressed = digitalRead(ROTARY_ENCODER_BUTTON_PIN) == LOW;
    }
    if (val)
    {
      // Serial.printf("encoder: %s %ld\r\n", rotaryPressed ? "T" : "F", val);
      rotaryEncoder.clearCount();
      if (rotaryPressed)
      {
        changeStationIndex(currentStationNum + val);
      }
      else
      {
        setVolume(volume + val);
      }
    }
  }
}

// optional
void audio_info(const char *info)
{
  Serial.print("info        ");
  Serial.println(info);

  String infoStr(info);
  infoStr.toLowerCase();
  if (infoStr.startsWith("streamurl="))
  {
    asyncWebSocket.textAll("AINF\t" + String(info));

    // experimental fetching of audio data like radio rock norge does not encapsulate the data into the stream
    // audio_info("StreamUrl='https://listenapi.planetradio.co.uk/api9.2/eventdata/292929585'");
    if (1 == 1)
    {
      String url = String(info + 11);
      url = url.substring(0, url.length() - 1);
      // Serial.print("Trying to get audio meta data from: ");
      // Serial.println(url.c_str());

      HTTPClient http;
      // WiFiClient newClient;
      if (http.begin(url))
      {
        int httpCode = http.GET();
        if (httpCode > 0)
        {
          // HTTP header has been send and Server response header has been handled
          // Serial.printf("[HTTP] GET... code: %d\n", httpCode);

          // file found at server
          if (httpCode == HTTP_CODE_OK)
          {
            String payload = http.getString();
            // Serial.println(payload);
            JsonDocument doc;
            DeserializationError derr = deserializeJson(doc, payload);
            if (derr == DeserializationError::Ok)
            {
              if (doc["eventType"].is<String>() && doc["eventSongArtist"].is<String>() && doc["eventSongTitle"].is<String>())
              {
                if (doc["eventType"].as<String>() == "Song")
                {
                  // Serial.println(doc["eventType"].as<String>());
                  // Serial.println(doc["eventSongArtist"].as<String>());
                  // Serial.println(doc["eventSongTitle"].as<String>());
                  sprintf(lastPlayedStreamTitle, "%s - %s", doc["eventSongArtist"].as<String>().c_str(), doc["eventSongTitle"].as<String>().c_str());
                  // Serial.println(lastPlayedStreamTitle);
                  audio_showstreamtitle(lastPlayedStreamTitle);
                }
                else
                {
                  Serial.printf("eventType is not Song...\r\n");
                }
              }
              else
              {
                Serial.printf("eventType || eventSongArtist || eventSongTitle not set\r\n");
              }
            }
            else
            {
              Serial.printf("DeserializationError: %d\r\n", derr);
            }
          }
        }
        else
        {
          Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        }
      }
      else
      {
        Serial.printf("[HTTP] begin... failed\r\n");
      }
    }
  }
}
void audio_id3data(const char *info)
{ // id3 metadata
  Serial.print("id3data     ");
  Serial.println(info);

  asyncWebSocket.textAll("AID3\t" + String(info));
}
void audio_eof_mp3(const char *info)
{ // end of file
  Serial.print("eof_mp3     ");
  Serial.println(info);

  asyncWebSocket.textAll("AEM3\t" + String(info));
}
void audio_showstation(const char *info)
{
  Serial.print("station     ");
  Serial.println(info);
  strncpy(lastPlayedStation, info, sizeof(lastPlayedStation));
  asyncWebSocket.textAll("ASTA\t" + String(lastPlayedStation));

  drawDisplay();
}
void audio_showstreamtitle(const char *info)
{
  Serial.print("streamtitle ");
  Serial.println(info);
  strncpy(lastPlayedStreamTitle, info, sizeof(lastPlayedStreamTitle));
  asyncWebSocket.textAll("ASTT\t" + String(lastPlayedStreamTitle));

  drawDisplay();
}
void audio_bitrate(const char *info)
{
  Serial.print("bitrate     ");
  Serial.println(info);
}
void audio_commercial(const char *info)
{ // duration in sec
  Serial.print("commercial  ");
  Serial.println(info);
}
void audio_icyurl(const char *info)
{ // homepage
  Serial.print("icyurl      ");
  Serial.println(info);
}
void audio_lasthost(const char *info)
{ // stream URL played
  Serial.print("lasthost    ");
  Serial.println(info);
}
void audio_eof_speech(const char *info)
{
  Serial.print("eof_speech  ");
  Serial.println(info);
}