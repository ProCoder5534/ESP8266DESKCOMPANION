#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include "config.h"

String getWiFiFormHTML() {
  return R"rawliteral(
  <!DOCTYPE html><html><head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESPClock</title>
  <style>
    body {
      background: black;
      color: #33ff33;
      font-family: monospace;
      display: flex;
      align-items: center;
      justify-content: center;
      height: 100vh;
    }
    .card {
      background: #111;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px #33ff33;
      text-align: center;
    }
    input {
      background: black;
      color: #33ff33;
      border: 1px solid #33ff33;
      padding: 10px;
      width: 100%;
      margin: 10px 0;
    }
    input[type=submit] {
      cursor: pointer;
    }
  </style>
  </head><body>
    <div class="card">
      <h2>Enter WiFi</h2>
      <form action="/connect" method="POST">
        <input name="ssid" placeholder="WiFi SSID" required><br>
        <input name="password" type="password" placeholder="WiFi Password" required><br>
        <input type="submit" value="Connect">
      </form>
    </div>
  </body></html>
  )rawliteral";
}

#endif