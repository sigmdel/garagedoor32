#include <Arduino.h>


/*  // [ ] TODO:  Remove this long comment when this is documented

https://github.com/me-no-dev/ESPAsyncWebServer/issues/1249
Poor selection of % as placeholder delimiter because is commonly used in CSS and JavaScript code. #1249
by aiotech_pub (2022-12-04)

In a reply/update
https://github.com/me-no-dev/ESPAsyncWebServer/issues/1249#issuecomment-1342781407
Sveninndh

"
I ran into the same problem, it took me a few hours. When studying the source code, I noticed two things that are unfortunately not documented.
1.) The maximum length of a placeholder is limited to 32 bytes.

2.) If you use % within a string or HTML file,
you must escape all occurrences of % - which are not enclose a placeholder - to %%.
Example: 'Width: 50%;' needs to be changed to 'width: 50%%;'
"

This has been criticized by DeeEMM for not being

"
Escaping the % characters is little more than a hack as it generates non-compliant css...
any solution that creates another problem is not a solution at all ;)
"

Sveninndh suggested a different solution in a fork of the ESPAsyncWebServer

https://github.com/Sveninndh/ESPAsyncWebServer/commit/e7c9e3f0801bad234b25a6f04b76d9ceaa2a381a

which does not require escaping %% in the HTML code fed to the server. However

.../ESPAsyncWebServer/src/WebResponses.cpp

381:  size_t AsyncAbstractResponse::_fillBufferAndProcessTemplates(uint8_t* data, size_t len)
...
404:    } else { // double percent sign encountered, this is single percent sign escaped.
405:      // remove the 2nd percent sign
406:      memmove(pTemplateEnd, pTemplateEnd + 1, &data[len] - pTemplateEnd - 1);
407:        len += _readDataFromCacheOrContent(&data[len - 1], 1) - 1;
408:      ++pTemplateStart;
409:    }

So 'width: 50%%' will be changed to 'width: 50%' so compliant CSS code will be sent to any client.

Of course if '%%' (as a substitute for  U+2030 PER MILLE SIGN ‰) is used in the HTML file (instead )

Note that in .../ESPAsyncWebServer/src/WebResponseImpl.h

62:  #ifndef TEMPLATE_PLACEHOLDER
63:  #define TEMPLATE_PLACEHOLDER '%'
64:  #endif
65:
66:  #define TEMPLATE_PARAM_NAME_LENGTH 32

So, placeholder names must be 32 characters in length or less and it is possible to
use a different TEMPLATE_PLACEHOLDER in the platform build_flags such as

build_flags =
  "-D TEMPLATE_PLACEHOLDER='#'"

https://github.com/me-no-dev/ESPAsyncWebServer/issues/1249#issuecomment-1372618455
by radozebra

That is not the best solution because # or any other ASCII character could easily appear in an
HTML file also.

https://github.com/me-no-dev/ESPAsyncWebServer/issues/1249#issuecomment-1342781407

*/

// [ ] NOTE: https://www.w3schools.com/js/js_timing.asp, using window.setTimeout(function, milliseconds) to try to reload page after restart?
// [ ] NOTE: https://www.w3schools.com/js/js_window_location.asp, to set the page to reload

// [ ] Note: https://stackoverflow.com/questions/60076905/how-to-share-a-websocket-connection-between-different-html-pages


/*
 *  /index.html and / when AP is not up
 */
const char html_index[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>%TITLE%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <link rel="icon" href="data:,">
  <style>
   html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white; font-size: small;}
   h1{color: #0F3376;}
   .button{display: inline-block; background-color: blue; border: none; border-radius: 6px; color: white; font-size: 1.5rem; width: 12em; height: 3rem; text-decoration: none; margin: 0px 0px 8px 0px; cursor: pointer;}
   .bred{background:#d43535;}
   .bgrey{background:#D0D0D0;}
   .frame{border: 1px solid blue; width: 24em; margin: 12px auto 12px auto;}
  </style>
</head>
<body>
  <div class="frame">
  <h1>%DEVICENAME%</h1>
  <h2><span id="doorID">%DOORSTATE%</span></h2>
  <span id="doorAction">%DOORACTION%</span>
  </div>

  <div class="frame">
  <h1>Fermeture automatique</h1>
  <h2><span id="autoID">%AUTOSTATE%</h2>
  <button class="button" onclick="toggleAuto()">Basculer</button>
  </div>
  <br/>
  <form action='sensors' method='get'><button class="button">Capteurs...</button></form>
  <form action='log' method='get'><button class="button">Console...</button></form>
  <form action='update' method='get'><button class="button">Mise à jour...</button></form>
  <p><form action='rst' method='get' onsubmit='return confirm("Confirmer le redémarrage");'><button class='button bred'>Redémarrer</button></form></p>

  <script>
  function closeDoor() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/close", true);
    xhr.send();
  }
  function toggleAuto() {
    var xhr = new XMLHttpRequest();
    xhr.open("GET", "/toggle", true);
    xhr.send();
  }
  if (!Boolean(EventSource)) {
    alert("Error: Server-Sent Events are not supported in your browser");
  } else {
    var source = new EventSource('/events');

    source.addEventListener('open', function(e) {
      console.log("Events Connected in /"); });

    source.addEventListener('error', function(e) {
      if (e.target.readyState != EventSource.OPEN) {
        console.log("Events Disconnected in /");} });

    source.addEventListener('doorstate', function(e) {
      console.log("doorstate", e.data);
      document.getElementById("doorID").innerHTML = e.data; });

    source.addEventListener('dooraction', function(e) {
      console.log("dooraction", e.data);
      document.getElementById("doorAction").innerHTML = e.data; });

    source.addEventListener('autostate', function(e) {
      console.log("autostate", e.data);
      document.getElementById("autoID").innerHTML = e.data; });
  }
  </script>
</body>
</html>)rawliteral";


/*
 *  onNotFound (i.e. 404 error)
 */
const char html_404[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>%DEVICENAME%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <link rel="icon" href="data:,">
  <style>
    html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white;}
    h1{color: #0F3376; padding: 2vh;}
    p{font-size: 1.5rem;}
    .button{display: inline-block; background-color: blue; border: none; border-radius: 6px; color: white; font-size: 1.5rem; width: 12em; height: 3rem; text-decoration: none; margin: 2px; cursor: pointer;}
  </style>
</head>
<body>
  <h1>%DEVICENAME%</h1>
  <p><b>Page introuvable</b></p>
  <p><form action='/index.html' method='get'><button class="button">Menu principal</button></form></p>
</body>
</html>)rawliteral";


/*
 *  onNotFound (i.e. 404 error)
 */
const char html_sensors[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>%TITLE%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <link rel="icon" href="data:,">
  <style>
    html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white;}
    h1{color: #0F3376; padding: 2vh;}
    table{margin-left: auto; margin-right: auto; margin-top:20px;}
    td{font-size: 1.5rem; text-align: left; padding: 8px;}
    .button{display: inline-block; background-color: blue; border: none; border-radius: 6px; color: white; font-size: 1.5rem; width: 12em; height: 3rem; text-decoration: none; margin: 2px; cursor: pointer;}
  </style>
</head>
<body>
  <h1>%DEVICENAME%</h1>
  <table>
  <tr><td>Température:</td><td><b><span id="temperatureID">%TEMPERATURE%</span></b> °C</td></tr>
  <tr><td>Humidité:</td><td><b><span id="humidityID">%HUMIDITY%</span></b> &percnt;</td></tr>
  <tr><td>Luminosité:</td><td><b><span id="brightnessID">%BRIGHTNESS%</span></b></td></tr>
  </table>
  <p><form action='.' method='get'><button class="button">Menu principal</button></form></p>
  <script>
    if (!Boolean(EventSource)) {
      alert("Error: Server-Sent Events are not supported in your browser");
    } else {

      var source = new EventSource('/events');

      source.addEventListener('open', function(e) {
        console.log("Events Connected in /sensors"); });

      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected in /sensors");} });

      source.addEventListener('tempvalue', function(e) {
        console.log("tempvalue", e.data);
        var parts = e.data.split(' ');
        document.getElementById("temperatureID").innerHTML = parts[0];
        document.getElementById("humidityID").innerHTML = parts[1];  });

      source.addEventListener('brightvalue', function(e) {
        console.log("ligthvalue", e.data);
        document.getElementById("brightnessID").innerHTML = e.data; });
    }
  </script>
</body>
</html>)rawliteral";


/*
 *  /log (i.e. the web console)
 */
const char html_console[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>%TITLE%</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <link rel="icon" href="data:,">
  <style>
    html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white;}
    h1{color: #0F3376; padding: 2vh;}
    input{padding:5px; font-size:1em; width: 98%%}
    textarea{resize:vertical;width:98%%;height:318px;padding:5px;overflow:auto;background:#1f1f1f;color:#65c115;}
    .button{display: inline-block; background-color: blue; border: none; border-radius: 6px; color: white; font-size: 1.5rem; width: 12em; height: 3rem; text-decoration: none; margin: 2px; cursor: pointer;}
    .bred{background:#d43535;}
  </style>
</head>
<body>
  <h1>%DEVICENAME%</h1>
  <textarea readonly id='log' cols='340' wrap='off'>%LOG%</textarea>
  <p><input id="cmd" type="text" value="" placeholder='Enter commands separated with ;' autofocus/></p>
  <p><form action='.' method='get'><button class="button">Menu principal</button></form></p>
  <p><form action='/rst' method='get' onsubmit='return confirm("Confirmer");'><button class='button bred'>Redémarrer</button></form></p>
  <div class="info">%INFO%</div>
  <script>
    cmdinput = document.getElementById("cmd")
    cmdinput.addEventListener("change", sendCmd);
	  function sendCmd() {
      const xhr = new XMLHttpRequest();
      const url = "/cmd?cmd=" + cmdinput.value;
      xhr.open("GET", url);
      xhr.send();
      cmdinput.value = "";
	  }
    if (!Boolean(EventSource)) {
      alert("Error: Server-Sent Events are not supported in your browser");
    } else {

      var oldlog = '';
      var source = new EventSource('/events');

      source.addEventListener('open', function(e) {
        console.log("Events Connected in /log"); });

      source.addEventListener('error', function(e) {
        if (e.target.readyState != EventSource.OPEN) {
          console.log("Events Disconnected in /log");} });

      source.addEventListener('logvalue', function(e) {
        console.log("logvalue", e.data);
        if (e.data === oldlog) {
          console.log("logvalue", "repeat");
        } else {
          ta = document.getElementById("log")
          ta.innerHTML += e.data + "\n";
          ta.scrollTop = ta.scrollHeight;
          oldlog = e.data;
        }  });
    }
  </script>
</body>
</html>)rawliteral";


/*
 *  /  when the AP is up (i.e. wifi manager)
 */
const char html_wm[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Wi-Fi Credentials</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <link rel="icon" href="data:,">
  <style>
    html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white;}
    h1{color: #0F3376; padding: 2vh;}
    table{margin-left: auto; margin-right: auto; margin-top:20px;}
    input, td{font-size: 1.1rem; text-align: left; padding: 8px;}
    .small{font-size: 0.9rem; text-align: center; padding: 6px;}
    .blue{background-color: blue; border: none; border-radius: 4px; color: white; cursor: pointer; width: 18em; text-align: center; font-weight: bold;}
  </style>
</head>
<body>
  <h1>%DEVICENAME%<br/>Access Point</h1>
  <p>Not connected to a Wi-Fi network.</p>
  <h2>Enter Wi-Fi Credentials</h2>
  <form action="/creds" method="POST">
    <table>
    <tr><td>Network</td><td><input type="text" id ="ssid" name="ssid" placeholder='%SSID%'></td></tr>
    <tr><td>Password</td><td><input type="text" id ="pass" name="pass" placeholder='%PASS%'></td></tr>
    <tr><td colspan="2"><hr></td></tr>
    <tr><td class="small" colspan="2">Leave blank to obtain addresses from the network</td></tr>
    <tr><td>Static IP</td><td><input type="text" id ="staip" name="staip" placeholder='%STAIP%'></td></tr>
    <tr><td>Gateway IP</td><td><input type="text" id ="gateway" name="gateway" placeholder='%GATE%'></td></tr>
    <tr><td>Sub net mask</td><td><input type="text" id ="mask" name="mask" placeholder='%MASK%'></td></tr>
    <table>
    <p><input type ="submit" value ="Connect" class="blue"></p>
  </form>
</body>
</html>)rawliteral";


/*
 *  Error page when bad Wifi credentials given
 */
const char html_wm_bad_creds[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Wi-Fi Credentials Error</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <meta http-equiv="refresh" content="3;url=/">
  <link rel="icon" href="data:,">
  <style>
    html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white;}
    h1{color: #0F3376; padding: 2vh;}
    p {font-size: 1.5rem}
  </style>
</head>
<body>
  <h1>%DEVICENAME%<h1>
  <h2>Wi-Fi Credentials Error</h2>
  <p>Missing network name</p>
  <p>or</p>
  <p>the password is too short</p>
</body>
</html>)rawliteral";


/*
 *  Confirmation page when Wifi credentials changed
 */
const char html_wm_connect[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Wi-Fi Credentials</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
  <link rel="icon" href="data:,">
  <style>
    html{font-family: Helvetica; display:inline-block; margin: 0px auto; text-align: center; background-color: white;}
    h1{color: #0F3376; padding: 2vh;}
    p {font-size: 1.5rem}
  </style>
</head>
<body>
  <h1>%DEVICENAME%</h1>
  <h2>Trying to connect to the network</h2>
  <h2>Please wait</h2>
</body>
</html>)rawliteral";
