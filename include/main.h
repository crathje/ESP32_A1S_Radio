#include <Arduino.h>

#ifndef __MAIN_H
#define __MAIN_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">

<head>
    <title>ESP32_A1S_Ratio - %HOSTNAME%</title>
    <style>
        a:link {
            text-decoration: none;
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

        button {
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

        button:hover {
            box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(0, 0, 255, 0.80);
        }

        button:active {
            box-shadow: 0 8px 16px 0 rgba(0, 0, 0, 0.2), 0 6px 20px 0 rgba(255, 0, 255, 0.80);
        }

        .singleStationContainer {}

        .station {
            background-color: #cccccc;
            text-decoration: none;
            margin: 5px;
            display: inline-block;
        }

        .station:hover,
        playStationButton:hover {
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

        #footer {
            position: fixed;
            bottom: 0px;
            right: 0px;
        }
    </style>
    <link href="https://fonts.cdnfonts.com/css/lcd" rel="stylesheet">
    <script>
        // develope is easier local
        var host = 'A1S-Radio-448026455F34.home'
        // host = 'a1s-radio-9cfab1f7a608.home'
        // host = 'a1s-radio-d0f7b1f7a608.home'
        var websocket;

        // borrowed from https://gist.github.com/g1eb/62d9a48164fe7336fdf4845e22ae3d2c
        function convertTime(seconds) {
            var seconds = parseInt(seconds, 10)
            var hours = Math.floor(seconds / 3600)
            var minutes = Math.floor((seconds - (hours * 3600)) / 60)
            var seconds = seconds - (hours * 3600) - (minutes * 60)
            if (!!hours) {
                if (!!minutes) {
                    return `${hours}h ${minutes}m ${seconds}s`
                } else {
                    return `${hours}h ${seconds}s`
                }
            }
            if (!!minutes) {
                return `${minutes}m ${seconds}s`
            }
            return `${seconds}s`
        }
        switch (window.location.protocol) {
            case 'http:':
            case 'https:':
                if (window.location.host.split(':')[0] != '127.0.0.1') {
                    host = window.location.host
                }
                break;
            default:
        }

        document.title = document.title.replace("%HOSTNAME%", host);

        function highlightStation() {
            const curr = document.getElementById('currentPlaying').innerHTML;
            Array.prototype.forEach.call(document.getElementsByClassName('station'), function (element) {
                if (element.innerHTML.includes(curr)) {
                    element.style.backgroundColor = '#FFcccc';
                } else {
                    element.style.backgroundColor = '#cccccc';
                }
            }
            );
        }

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
                        document.title = '???' + document.title.substring(document.title.lastIndexOf(' - '))
                        highlightStation() 
                        break
                    case 'V':
                        document.getElementById('volume-bar-label').textContent = payload
                        document.getElementById('volume-bar-fill').style.width = payload + '%'
                        var volSlider = document.getElementById('volume')
                        if (volSlider.value != parseInt(payload.toString())) {
                            volSlider.value = parseInt(payload.toString())
                        }
                        break
                    case 'ASTA':
                        if (payload.length > 0) {
                            document.getElementById('station').textContent = payload
                            document.title = payload + document.title.substring(document.title.lastIndexOf(' - '))
                        }
                        break
                    case 'ASTT':
                        if (payload.length > 0) {
                            document.getElementById('streamInfo').textContent = payload
                            document.title = payload + document.title.substring(document.title.lastIndexOf(' - '))
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
        var xhrConfig = new XMLHttpRequest();
        xhrConfig.addEventListener("load", function () {
            var configJSon = JSON.parse(this.responseText)
            //var stations = this.responseText.trim().split("\n");
            // console.log(configJSon)
            configJSon.stations.forEach((station) => {
                var stationsDiv = document.getElementById('stations');
                var singleStationContainer = document.createElement('div');
                singleStationContainer.className = 'singleStationContainer'
                var ldiv = document.createElement('div');
                ldiv.className = 'station'
                ldiv.innerHTML = station.name + ' (' + station.url + ')';
                var playdiv = document.createElement('div');
                playdiv.innerHTML = '&#x25B6;';
                playdiv.className = 'playStationButton';
                playdiv.onclick = function () {
                    var xhr = new XMLHttpRequest();
                    xhr.timeout = 2000;
                    xhr.open("GET", "http://" + host + "/playurl?playurl=" + station.url, true);
                    xhr.send();
                };
                ldiv.onclick = playdiv.onclick;
                singleStationContainer.appendChild(playdiv);
                singleStationContainer.appendChild(ldiv);
                stationsDiv.appendChild(singleStationContainer);
            });
            highlightStation() 
        });
        xhrConfig.open("GET", "http://" + host + "/config.json");
        xhrConfig.send();

        function volumeSliderOnChange(val) {
            //document.getElementById('textInput').value=val; 
            var xhrData = new XMLHttpRequest();
            xhrData.open("GET", "http://" + host + "/volume?v=" + val);
            xhrData.send();
        }

        const loadFooter = function () {
            var xhrData = new XMLHttpRequest();
            xhrData.addEventListener("load", function () {
                var dataJson = JSON.parse(this.responseText)
                var footer = document.getElementById('footer')
                footer.innerHTML = '<small>' + convertTime(dataJson.uptime / 1000) + ' &centerdot; '
                    + (dataJson.FreeHeap / 1024).toFixed(3) + 'kb &centerdot; (min: '
                    + (dataJson.MinFreeHeap / 1024).toFixed(3) + 'kb) &centerdot; '
                    + dataJson.compile_date + '</small></i>'
            });
            xhrData.open("GET", "http://" + host + "/data.json");
            xhrData.send();
        }
        loadFooter()
        const interval = setInterval(loadFooter, 10 * 1000);

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
    <div>
        <input type="range" id="volume" class="volume-bar" name="volume" min="0" max="100"
            onchange="volumeSliderOnChange(this.value);" />
    </div>
    <br />
    <div id="buttons" class="buttonscontainer">
        <button onclick="websocket.send('stationdown')">&#x23EE;</button>
        <button onclick="websocket.send('voldown')">&#x1f509;</button>
        <button onclick="websocket.send('playpause')">&#x23ef;</button>
        <button onclick="websocket.send('volup')">&#x1F50A;</button>
        <button onclick="websocket.send('stationup')">&#x23ED;</button>
    </div>
    <br />
    <div id="stations"></div>
    <br />
    <form action="playurl" target="dummy">
        <input type="text" name="playurl"></input>
        <input type="submit" value="play url">
    </form>
    <br />
    <br />

    <a href="/config">config</a><br />
    <a href="/update">update</a><br />
    <a href="/data.json">data</a><br />
    amp <a href="/amp?enable=1">on</a>/<a href="/amp?enable=0">off</a><br />

    <iframe src="" name="dummy" style="visibility:hidden;"></iframe>
    <div id="footer"></div>
</body>

</html>
    )rawliteral";
    
#define _DEFAULT_GPIO_SPDIFF_OUTPUT 12
    const char _DEFAULT_CONFIG[] PROGMEM = R"rawliteral(
{
  "volume": 60,
  "hostname": "",
  "GPIO_SPDIFF_OUTPUT": -1,
  "GPIO_PA_EN_EXTERNAL": -1,
  "SCREEN_WIDTH": 128,
  "SCREEN_HEIGHT": 32,
  "IIC_EXTERNAL_CLK": 5,
  "IIC_EXTERNAL_DATA": 18,
  "ROTARY_ENCODER_A_CLK_PIN": 22,
  "ROTARY_ENCODER_B_DT_PIN": 19,
  "ROTARY_ENCODER_BUTTON_PIN": 23,
  "ROTARY_ENCODER_STEPS": 2,
  "stations": [
    {
      "name": "Radio Rock Norge",
      "url": "http://live-bauerno.sharp-stream.com/radiorock_no_mp3?direct=true"
    },
    {
      "name": "Bob SH",
      "url": "http://streams.radiobob.de/bob-shlive/mp3-128/streams.radiobob.de/play.m3u"
    },
    {
      "name": "Delta Foehnfrisur",
      "url": "http://streams.deltaradio.de/delta-foehnfrisur/mp3-128/streams.deltaradio.de/play.m3u"
    },
    {
      "name": "Bob Metal",
      "url": "http://streams.radiobob.de/bob-metal/mp3-128/streams.radiobob.de/play.m3u"
    },
    {
      "name": "NDR2",
      "url": "http://www.ndr.de/resources/metadaten/audio/m3u/ndr2_sh.m3u"
    },
    {
      "name": "Bob Alternative",
      "url": "http://streams.radiobob.de/bob-alternative/mp3-128/streams.radiobob.de/play.m3u"
    },
    {
      "name": "Bob Symphmetal",
      "url": "http://streams.radiobob.de/symphmetal/mp3-192/streams.radiobob.de/play.m3u"
    },
    {
      "name": "laut.fm/metal",
      "url": "http://stream.laut.fm/metal"
    },
    {
      "name": "antenne/hair",
      "url": "https://stream.rockantenne.de/hair-metal/stream/mp3"
    }
  ]
}
    )rawliteral";
    
#endif