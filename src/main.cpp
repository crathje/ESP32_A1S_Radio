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
#endif
#ifdef DAC2USE_ES8388
#include "ES8388.h" // https://github.com/maditnerd/es8388
#endif
#include <ArduinoOTA.h>
#include <ESPAsyncWebServer.h>   // https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ESP32Encoder.h>        // https://github.com/madhephaestus/ESP32Encoder
#include <ElegantOTA.h>

#include <Wire.h>
TwoWire I2C_2 = TwoWire(1);
#define SSD1306_ADDRESS 0x3C

#include <Adafruit_SSD1306.h>
// #include <U8g2lib.h>

#include "Audio.h" //https://github.com/schreibfaul1/ESP32-audioI2S
AsyncWebServer asyncWebServer(80);
AsyncWebSocket asyncWebSocket("/ws");
DNSServer dnsServer;
char hostname[32 + 1];
const char compile_date[] = __DATE__ " " __TIME__;

char lastPlayedUrl[2048];
char lastPlayedStreamTitle[2048];
char lastPlayedStation[2048];

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
    <style>
        a:link {
            text-decoration: none;
            font-size: 40px;
        }

        div {
            margin: 2px;
        }

        .lcd {
            font-family: 'LCD', sans-serif;
            border: 0px;
            padding: 5px;
            color: mediumaquamarine;
            background-color: black;
            font-size: xx-large;
        }

        .smalllcd {
            font-family: 'LCD', sans-serif;
            border: 0px;
            padding: 5px;
            color: greenyellow;
            background-color: black;
            font-size: large;
        }

        .buttonscontainer {
            text-align: center;
        }

        .button {
            border: none;
            background-color: #cccccc;
            padding: 15px 32px;
            margin: auto;
            text-align: center;
            text-decoration: none;
            display: inline-block;
            font-size: xx-large;
            border-radius: 10px;
            box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 0, 0.19);
        }

        .button:hover {
            box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 255, 0.80);
        }

        .button:active {
            box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(255, 0, 255, 0.80);
        }

        .singleStationContainer {
        }

        .station {
            background-color: #cccccc;
            text-decoration: none;
            margin: 5px;
            display: inline-block;
        }

        .station:hover, .playStationButton:hover {
            background-color: rgb(86, 84, 204);
        }

        .playStationButton {
            background-color: #FFcccc;
            display: inline-block;
        }

        .volume-bar {
            position: relative;
            width: 100%;
            background-color: #666666;
            color: white;
            font-weight: bolder;
        }

        .volume-bar-fill {
            background-color: green;
        }

        .centered {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
        }
    </style>
    <link href="https://fonts.cdnfonts.com/css/lcd" rel="stylesheet">
    <script>
        // develope is easier local
        var host = 'A1S-Radio-448026455F34.home'
        var websocket;


        switch (window.location.protocol) {
            case 'http:':
            case 'https:':
                if (window.location.host.split(':')[0] != '127.0.0.1') {
                    host = window.location.host
                }
                break;
            default:
        }

        ;
        websocket = new WebSocket('ws://' + host + '/ws');
        // websocket.onopen    = onOpen;
        // websocket.onclose   = onClose;
        // websocket.onmessage = onMessage; 

        function handleCommand(message) {
            console.debug("handleCommand:", message)
            if (message.includes("\t")) {
                var splitted = message.split("\t")
                var payload = message.substring(splitted[0].length + 1).trim()
                // console.debug(splitted, splitted[0].length)
                // console.debug("+" + payload + "+")
                switch (splitted[0]) {
                    case 'C':
                        document.getElementById('currentPlaying').innerHTML = payload
                        document.getElementById('station').innerHTML = '&#x2047;'
                        document.getElementById('streamInfo').innerHTML = '&#x2047;'
                        break
                    case 'V':
                        document.getElementById('volume-bar-label').textContent = payload
                        document.getElementById('volume-bar-fill').style.width = payload + '%'
                        break
                    case 'ASTA':
                        if (payload.length > 0) {
                            document.getElementById('station').textContent = payload
                        }
                        break
                    case 'ASTT':
                        if (payload.length > 0) {
                            document.getElementById('streamInfo').textContent = payload
                        }
                        break
                    default:
                        // console.debug("unhandled websocket:", event.data)
                        break
                }
            }
        }

        websocket.addEventListener("message", (event) => {
            // console.log(event.data)
            if (event.data.toString().includes("\t")) {
                handleCommand(event.data.toString())
            }
        });
        var xhr = new XMLHttpRequest();
        xhr.addEventListener("load", function () {
            var stations = this.responseText.trim().split("\n");
            stations.forEach((station) => {
                var stationsDiv = document.getElementById('stations');
                var singleStationContainer = document.createElement('div');
                singleStationContainer.className = 'singleStationContainer'
                var ldiv = document.createElement('div');
                ldiv.className = 'station'
                ldiv.innerHTML = station;
                var playdiv = document.createElement('div');
                playdiv.innerHTML = '&#x25B6;';
                playdiv.className = 'playStationButton';                
                playdiv.onclick = function () {
                    var xhr = new XMLHttpRequest();
                    xhr.timeout = 2000;
                    xhr.open("GET", "http://" + host + "/playurl?playurl=" + ldiv.innerHTML, true);
                    xhr.send();
                };
                
                singleStationContainer.appendChild(playdiv);
                singleStationContainer.appendChild(ldiv);

                
                stationsDiv.appendChild(singleStationContainer);
            });
        });

        xhr.open("GET", "http://" + host + "/stations");
        xhr.send();
    </script>
</head>

<body>
    <div id="streamInfo" class="lcd"></div>
    <div id="station" class="smalllcd"></div>
    <div id="currentPlaying" class="smalllcd"></div>
    <div id="volume-bar" class="volume-bar">
        <div id="volume-bar-label" class="centered">&nbsp;</div>
        <div id="volume-bar-fill" class="volume-bar-fill">&nbsp;</div>
    </div>
    <br />
    <div id="buttons" class="buttonscontainer">
        <a class="button" href="playpause" target="dummy">&#x23ef;</a>
        <a class="button" href="voldown" target="dummy">&#x1f509;</a>
        <a class="button" href="volup" target="dummy">&#x1F50A;</a>
    </div>
    <br />
    <div id="stations"></div>
    <br />
    <form action="playurl" target="dummy">
        <input type="text" name="playurl"></input>
        <input type="submit" value="play url">
    </form>
    <br />

    <iframe src="" name="dummy" style="visibility:hidden;"></iframe>
</body>

</html>
)rawliteral";

const char stations[] PROGMEM = R"rawliteral(
http://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true
http://streams.radiobob.de/bob-shlive/mp3-128/streams.radiobob.de/play.m3u
http://streams.deltaradio.de/delta-foehnfrisur/mp3-128/streams.deltaradio.de/play.m3u
http://streams.radiobob.de/bob-metal/mp3-128/streams.radiobob.de/play.m3u
http://www.ndr.de/resources/metadaten/audio/m3u/ndr2_sh.m3u
)rawliteral";

#include "Button2.h"
Button2 button3, button4;

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

#define IIC_EXTERNAL_CLK 5
#define IIC_EXTERNAL_DATA 18
#define IIC_EXTERNAL_DATA_FREQ 400000

// amplifier enable
#define GPIO_PA_EN 21

#define ROTARY_ENCODER_A_CLK_PIN 22
#define ROTARY_ENCODER_B_DT_PIN 19
#define ROTARY_ENCODER_BUTTON_PIN -1
#define ROTARY_ENCODER_STEPS 2
ESP32Encoder rotaryEncoder;

// Switch S1: 1-OFF, 2-ON, 3-ON, 4-ON, 5-ON

#ifdef DAC2USE_AC101
static AC101 dac; // AC101
#endif

#ifdef DAC2USE_ES8388
static ES8388 dac; // ES8388 (new board)
#endif
int volume = 70; // 0...100

Audio audio;

// #ifdef U8X8_HAVE_HW_SPI
// #include <SPI.h>
// #endif
// #ifdef U8X8_HAVE_HW_I2C
// #include <Wire.h>
// #endif/
// U8G2_SSD1306_128X32_UNIVISION_1_2ND_HW_I2C u8g2(U8G2_R0 /* rotation */, /* reset=*/U8X8_PIN_NONE, IIC_EXTERNAL_DATA, IIC_EXTERNAL_CLK);
// U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 4, 5);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_2, -1);
volatile boolean foundDisplay = false;

// #####################################################################

void drawDisplay()
{
  if (foundDisplay)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    String lastPlayedStationS = String(lastPlayedStation);
    display.println(lastPlayedStationS.substring(0, 20).c_str());
    display.println(lastPlayedStreamTitle);

    int mapped = map(volume, 0, 100, 0, SCREEN_WIDTH);
    for (int y = SCREEN_HEIGHT - 3; y < SCREEN_HEIGHT; y++)
    {
      display.drawLine(0, y, mapped, y, WHITE);
    }

    display.display();
  }
}

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
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
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void setVolume(int value)
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

  volume = value;
#ifdef DAC2USE_ES8388
  dac.volume(ES8388::ES_MAIN, value);
  dac.volume(ES8388::ES_OUT1, value);
  dac.volume(ES8388::ES_OUT2, value);
  dac.mute(ES8388::ES_OUT1, false);
  dac.mute(ES8388::ES_OUT2, false);
  dac.mute(ES8388::ES_MAIN, false);
#endif

#ifdef DAC2USE_AC101
  dac.setVolumeSpeaker(value);
  dac.setVolumeHeadphone(value);
#endif

  asyncWebSocket.textAll("V\t" + String(volume));
  drawDisplay();
}

void actionPlayPause() { audio.pauseResume(); }
void actionVolumeUp() { setVolume(volume + 1); }
void actionVolumeDown() { setVolume(volume - 1); }

void actionUpdateStarting()
{
  digitalWrite(GPIO_PA_EN, LOW);
// mute!
#ifdef DAC2USE_ES8388
  dac.mute(ES8388::ES_OUT1, true);
  dac.mute(ES8388::ES_OUT2, true);
  dac.mute(ES8388::ES_MAIN, true);
#endif
  audio.stopSong();

  if (foundDisplay)
  {
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Updating...");
    display.display();
  }
}

void buttonClick(Button2 &btn)
{
  if (btn == button4)
  {
  }
  // else if (btn == button5)
  // {
  //   actionPlayPause();
  // }
  // else if (btn == button6)
  // {
  //   actionVolumeUp();
  // }
}

void playUrl(const char *url)
{
  audio_showstreamtitle("");

  String surl = String(url);
  sprintf(lastPlayedStation, "%s", surl.substring(surl.lastIndexOf("/") + 1).c_str());
  audio_showstation(lastPlayedStation);
  strcpy(lastPlayedUrl, url);
  asyncWebSocket.textAll("C\t" + String(url));
  audio.connecttohost(url);
}

void setup()
{
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

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  asyncWebServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                    { request->send_P(200, "text/html", index_html); });
  asyncWebServer.on("/stations", HTTP_GET, [](AsyncWebServerRequest *request)
                    { request->send_P(200, "text/plain", stations); });
  asyncWebServer.on("/playpause", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
    request->send(200, "text/plain", "OK");
    actionPlayPause(); });
  asyncWebServer.on("/playurl", HTTP_GET, [](AsyncWebServerRequest *request)
                    {
    if (request->hasParam("playurl")) {
      String url = request->getParam("playurl")->value();
      playUrl(url.c_str());
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

  asyncWebServer.on("/data.json", HTTP_GET, [](AsyncWebServerRequest *request)
                    { request->send(200, "application/json", String()                                                                   //
                                                                 + F("{")                                                               //
                                                                 + F("\n  \"lastPlayedUrl\": \"") + String(lastPlayedUrl) + F("\"")     //
                                                                 + F("\n, \"rssi\": \"") + String(WiFi.RSSI()) + F("\"")                //
                                                                 + F("\n, \"hostname\": \"") + String(WiFi.getHostname()) + F("\"")     //
                                                                 + F("\n, \"SSID\": \"") + String(WiFi.SSID()) + F("\"")                //
                                                                 + F("\n, \"compile_date\": \"") + String(compile_date) + F("\"")       //
                                                                 + F("\n, \"sdk_version\": \"") + String(ESP.getSdkVersion()) + F("\"") //
                                                                 + F("\n, \"PIO_ENV\": \"") + String(PIO_ENV) + F("\"") + F("\n}")); });

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

  ArduinoOTA.setHostname(wifiManager.getConfiguredSTASSID().c_str());
  //  ArduinoOTA.setPassword("secretPass");
  ArduinoOTA.begin();

  Serial.printf("Connect to DAC codec... ");
  while (not dac.begin(IIC_DATA, IIC_CLK))
  {
    Serial.printf("Failed!\n");
    delay(1000);
  }
  Serial.printf("OK\r\n");

  Serial.printf("Starting 2nd I2C on data: %d clk: %d...", IIC_EXTERNAL_DATA, IIC_EXTERNAL_CLK);
  I2C_2.begin(IIC_EXTERNAL_DATA, IIC_EXTERNAL_CLK, IIC_EXTERNAL_DATA_FREQ);
  Serial.printf("OK\r\n");

  Serial.println();
  Serial.println("I2C scanner. Scanning ...");
  byte count = 0;

#define MIN_I2C_DEVICE_ADDR 8
#define MAX_I2C_DEVICE_ADDR 120
  for (byte i = MIN_I2C_DEVICE_ADDR; i < MAX_I2C_DEVICE_ADDR; i++)
  {
    I2C_2.beginTransmission(i);
    if (I2C_2.endTransmission() == 0)
    {
      Serial.printf("Found 0x%02x\r\n", i);
      if (i == SSD1306_ADDRESS)
      {
        foundDisplay = true;
      }
    }
    else
    {
      // Serial.printf("Absent 0x%02x", i);
    }
  }
  Serial.printf("DONE\r\n");

  if (foundDisplay)
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
    display.begin(SSD1306_SWITCHCAPVCC, SSD1306_ADDRESS);
    display.clearDisplay();
    display.display();

    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(10, 0);
    display.println("Booting...");
    display.display();
  }
  setVolume(volume);

  //  ac.DumpRegisters();

  // Enable amplifier
  pinMode(GPIO_PA_EN, OUTPUT);
  digitalWrite(GPIO_PA_EN, HIGH);

  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_MCLK);
  audio.setVolume(10); // 0...21

  // audio.connecttoFS(SD, "/320k_test.mp3");
  // audio.connecttoSD("/Banana Boat Song - Harry Belafonte.mp3");
  // audio.connecttohost("http://s1-webradio.antenne.de/oldie-antenne");
  // audio.connecttohost("https://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true");
  // audio.connecttohost("http://dg-rbb-http-dus-dtag-cdn.cast.addradio.de/rbb/antennebrandenburg/live/mp3/128/stream.mp3");
  // audio.connecttospeech("Wenn die Hunde schlafen, kann der Wolf gut Schafe stehlen.", "de");

  if (ROTARY_ENCODER_A_CLK_PIN != -1 && ROTARY_ENCODER_A_CLK_PIN != -1)
  {
    rotaryEncoder.attachHalfQuad(ROTARY_ENCODER_B_DT_PIN, ROTARY_ENCODER_A_CLK_PIN);
    rotaryEncoder.setCount(0);
  }

  memset(lastPlayedUrl, sizeof(lastPlayedUrl), 0);
  memset(lastPlayedStreamTitle, sizeof(lastPlayedStreamTitle), 0);

  playUrl("http://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true");
}

//-----------------------------------------------------------------------

void loop()
{
  audio.loop();
  // button3.loop();
  // button4.loop();
  // button5.loop();
  // button6.loop();
  ArduinoOTA.handle();
  ElegantOTA.loop();

  if (ROTARY_ENCODER_A_CLK_PIN != -1 && ROTARY_ENCODER_A_CLK_PIN != -1)
  {
    int64_t val = rotaryEncoder.getCount();
    if (val)
    {
      // Serial.printf("encoder: %ld\r\n", val);
      rotaryEncoder.clearCount();
      setVolume(volume + val);
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