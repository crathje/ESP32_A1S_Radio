#include <Arduino.h>

// based on https://github.com/schreibfaul1/ESP32-audioI2S/blob/master/examples/ESP32_A1S/ESP32_A1S.ino

// default some DAC
#ifndef DAC2USE_ES8388
#ifndef DAC2USE_AC101
#define DAC2USE_ES8388 1
// #define DAC2USE_AC101 1
#endif
#endif

#ifdef DAC2USE_AC101
#include "AC101.h" // https://github.com/schreibfaul1/AC101
#endif
#ifdef DAC2USE_ES8388
#include "ES8388.h" // https://github.com/maditnerd/es8388
#endif
#include "Audio.h" //https://github.com/schreibfaul1/ESP32-audioI2S

#include <ESPAsyncWebServer.h>   // https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoOTA.h>

AsyncWebServer asyncWebServer(80);
DNSServer dnsServer;

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
// #define BUTTON_2_PIN 13             // shared mit SPI_CS
#define BUTTON_3_PIN 19
#define BUTTON_4_PIN 23
#define BUTTON_5_PIN 18 // Stop
#define BUTTON_6_PIN 5  // Play

// amplifier enable
#define GPIO_PA_EN 21

// Switch S1: 1-OFF, 2-ON, 3-ON, 4-ON, 5-ON

#ifdef DAC2USE_AC101
static AC101 dac; // AC101
#endif

#ifdef DAC2USE_ES8388
static ES8388 dac; // ES8388 (new board)
#endif
int volume = 70; // 0...100

Audio audio;

// #####################################################################

void setup()
{
  Serial.begin(115200);
  Serial.println("\r\nReset");

  uint64_t chipid = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  Serial.println(F("===== CHIP INFO ====="));
  Serial.printf_P(PSTR("ChipID:            %04X%08X\r\n"), (uint16_t)(chipid >> 32), (uint32_t)chipid);
  Serial.printf_P(PSTR("ChipCores:         %d\r\n"), ESP.getChipCores());
  Serial.printf_P(PSTR("CpuFreqMHz:        %3d\r\n"), ESP.getCpuFreqMHz());
  Serial.printf_P(PSTR("ChipRevision:      %d\r\n"), ESP.getChipRevision());
  Serial.printf_P(PSTR("FreeHeap:          %3d\r\n"), ESP.getFreeHeap());
  Serial.printf_P(PSTR("CycleCount:        %3d\r\n"), ESP.getCycleCount());
  Serial.printf_P(PSTR("SdkVersion:        %s\r\n"), ESP.getSdkVersion());
  Serial.printf_P(PSTR("FlashChipSize:     %3d\r\n"), ESP.getFlashChipSize());
  // DebugHWSerial.printf("FlashChipRealSize: %3d\n"), ESP.getFlashChipRealSize());
  Serial.printf_P(PSTR("FlashChipSpeed:    %3d\r\n"), ESP.getFlashChipSpeed());
  Serial.printf_P(PSTR("FlashChipMode:     %d\r\n"), ESP.getFlashChipMode());
  Serial.printf_P(PSTR("SketchSize:        %d\r\n"), ESP.getSketchSize());
  Serial.printf_P(PSTR("FreeSketchSpace:   %d\r\n"), ESP.getFreeSketchSpace());
  Serial.printf_P(PSTR("ArduinoStack:      %d\r\n"), getArduinoLoopTaskStackSize());

  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI);
  SPI.setFrequency(1000000);

  SD.begin(SD_CS);

  AsyncWiFiManager wifiManager(&asyncWebServer, &dnsServer);
  wifiManager.autoConnect();

  // WiFi.mode(WIFI_STA);
  // WiFi.begin("SSID", "Password...");
  // while (WiFi.status() != WL_CONNECTED)
  // {
  //   Serial.print(".");
  //   delay(100);
  // }

  Serial.printf_P(PSTR("Connected\r\nRSSI: "));
  Serial.print(WiFi.RSSI());
  Serial.print(" IP: ");
  Serial.println(WiFi.localIP());

  ArduinoOTA.setHostname(wifiManager.getConfiguredSTASSID().c_str());
  //  ArduinoOTA.setPassword("secretPass");
  ArduinoOTA.begin();

  Serial.printf("Connect to DAC codec... ");
  while (not dac.begin(IIC_DATA, IIC_CLK))
  {
    Serial.printf("Failed!\n");
    delay(1000);
  }
  Serial.printf("OK\n");

#ifdef DAC2USE_ES8388
  dac.volume(ES8388::ES_MAIN, volume);
  dac.volume(ES8388::ES_OUT1, volume);
  dac.volume(ES8388::ES_OUT2, volume);
  dac.mute(ES8388::ES_OUT1, false);
  dac.mute(ES8388::ES_OUT2, false);
  dac.mute(ES8388::ES_MAIN, false);
#endif

#ifdef DAC2USE_AC101
  dac.SetVolumeSpeaker(volume);
  dac.SetVolumeHeadphone(volume);
#endif
  //  ac.DumpRegisters();

  // Enable amplifier
  pinMode(GPIO_PA_EN, OUTPUT);
  digitalWrite(GPIO_PA_EN, HIGH);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(10); // 0...21

  // audio.connecttoFS(SD, "/320k_test.mp3");
  //  audio.connecttoSD("/Banana Boat Song - Harry Belafonte.mp3");
  // audio.connecttohost("http://s1-webradio.antenne.de/oldie-antenne");
  audio.connecttohost("https://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true");
  //  audio.connecttohost("http://dg-rbb-http-dus-dtag-cdn.cast.addradio.de/rbb/antennebrandenburg/live/mp3/128/stream.mp3");
  //  audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");
}

//-----------------------------------------------------------------------

void loop()
{
  audio.loop();
  ArduinoOTA.handle();
}

// optional
void audio_info(const char *info)
{
  Serial.print("info        ");
  Serial.println(info);
}
void audio_id3data(const char *info)
{ // id3 metadata
  Serial.print("id3data     ");
  Serial.println(info);
}
void audio_eof_mp3(const char *info)
{ // end of file
  Serial.print("eof_mp3     ");
  Serial.println(info);
}
void audio_showstation(const char *info)
{
  Serial.print("station     ");
  Serial.println(info);
}
void audio_showstreamtitle(const char *info)
{
  Serial.print("streamtitle ");
  Serial.println(info);
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