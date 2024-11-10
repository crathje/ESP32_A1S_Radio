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

#include "Audio.h" //https://github.com/schreibfaul1/ESP32-audioI2S
AsyncWebServer asyncWebServer(80);
AsyncWebSocket asyncWebSocket("/ws");
DNSServer dnsServer;
char hostname[32 + 1];

char lastPlayedUrl[2048];

const char index_html[] PROGMEM = R"rawliteral(
<html>

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
            color: greenyellow;
            background-color: black;
            font-size: xx-large;
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

        .station {
            background-color: #cccccc;
            text-decoration: none;
            margin: 5px;
        }

        .station:hover {
            background-color: rgb(86, 84, 204);
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
                host = window.location.host
                break;
            default:
        }

        ;
        websocket = new WebSocket('ws://' + host + '/ws');
        // websocket.onopen    = onOpen;
        // websocket.onclose   = onClose;
        // websocket.onmessage = onMessage; 

        websocket.addEventListener("message", (event) => {
            // console.log(event.data)
            if (event.data.toString().includes("\t")) {
                var splitted = event.data.toString().split("\t");
                // console.debug(splitted[0])
                switch (splitted[0]) {
                    case 'C':
                        document.getElementById('currentPlaying').innerHTML = splitted[1]
                        break;
                    case 'V':
                        // document.getElementById('volume-bar-label').textContent = splitted[1]
                        // console.log(document.getElementById('volume-bar-label'))
                        document.getElementById('volume-bar-label').textContent = splitted[1]
                        document.getElementById('volume-bar-fill').style.width = splitted[1] + '%'
                        break;
                }
            }
        });
        var xhr = new XMLHttpRequest();
        xhr.addEventListener("load", function () {
            var stations = this.responseText.trim().split("\n");
            stations.forEach((station) => {
                var stationsDiv = document.getElementById('stations');
                var ldiv = document.createElement('div');
                // ldiv.style.border = '1px dashed grey';
                ldiv.className = 'station';
                ldiv.innerHTML = station;
                ldiv.ondblclick = function () {
                    var xhr = new XMLHttpRequest();
                    xhr.timeout = 2000;
                    xhr.open("GET", "http://" + host + "/playurl?playurl=" + this.innerHTML, true);
                    xhr.send();

                };
                stationsDiv.appendChild(ldiv);
            });
        });

        xhr.open("GET", "http://" + host + "/stations");
        xhr.send();
    </script>
</head>

<body>
    <div id="currentPlaying" class="lcd"></div>
    <div id="volume-bar" class="volume-bar">
        <div id="volume-bar-label" class="centered">&nbsp;</div>
        <div id="volume-bar-fill" class="volume-bar-fill">&nbsp;</div>
    </div>
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
https://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true
https://streams.radiobob.de/bob-shlive/mp3-128/streams.radiobob.de/play.m3u
https://streams.deltaradio.de/delta-foehnfrisur/mp3-128/streams.deltaradio.de/play.m3u
https://streams.radiobob.de/bob-metal/mp3-128/streams.radiobob.de/play.m3u
https://www.ndr.de/resources/metadaten/audio/m3u/ndr2_sh.m3u
)rawliteral";

#include "Button2.h"
Button2 button3, button4, button5, button6;

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
void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    client->text("C\t" + String(lastPlayedUrl));
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
}

void actionPlayPause() { audio.pauseResume(); }
void actionVolumeUp() { setVolume(volume + 1); }
void actionVolumeDown() { setVolume(volume - 1); }

void buttonClick(Button2 &btn)
{
  if (btn == button3)
  {
    actionVolumeDown();
  }
  else if (btn == button4)
  {
    actionVolumeUp();
  }
  else if (btn == button5)
  {
    actionPlayPause();
  }
  else if (btn == button6)
  {
  }
}

void playUrl(const char *url)
{
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

  pinMode(BUTTON_3_PIN, INPUT_PULLUP);
  pinMode(BUTTON_4_PIN, INPUT_PULLUP);
  pinMode(BUTTON_5_PIN, INPUT_PULLUP);
  pinMode(BUTTON_6_PIN, INPUT_PULLUP);

  button3.begin(BUTTON_3_PIN);
  button3.setClickHandler(buttonClick);
  button4.begin(BUTTON_4_PIN);
  button4.setClickHandler(buttonClick);
  button5.begin(BUTTON_5_PIN);
  button5.setClickHandler(buttonClick);
  button6.begin(BUTTON_6_PIN);
  button6.setClickHandler(buttonClick);

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
  asyncWebSocket.onEvent(onWebSocketEvent);
  asyncWebServer.addHandler(&asyncWebSocket);

  asyncWebServer.begin();

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
  Serial.print(WiFi.localIP());
  Serial.print(" Hostname: ");
  Serial.println(WiFi.getHostname());

  ArduinoOTA
      .onStart([]()
               {
        digitalWrite(GPIO_PA_EN, LOW);
// mute!
#ifdef DAC2USE_ES8388
        dac.mute(ES8388::ES_OUT1, true);
        dac.mute(ES8388::ES_OUT2, true);
        dac.mute(ES8388::ES_MAIN, true);
#endif
        audio.stopSong();

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
  Serial.printf("OK\n");

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

  playUrl("https://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true");
}

//-----------------------------------------------------------------------

void loop()
{
  audio.loop();
  button3.loop();
  button4.loop();
  button5.loop();
  button6.loop();
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