#include <Arduino.h>
#include "main.h"
#include "web.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

const char *ssid = "TankReader";
const char *password = "12345678";

WebServer server(80);

void handleRoot()
{

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset='utf-8'>
  <title>Tank pressure history</title>
  <style>
    body {
      margin: 10px;
      font-family: Arial, sans-serif;
    }

    .main-container {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      gap: 20px;
    }

    .table-container {
      flex: 1 1 600px;
      max-width: 100%;
    }

    .form-container {
      flex: 0 0 300px;
      border: 1px solid #ccc;
      padding: 10px;
      border-radius: 10px;
    }

    table {
      width: 100%;
      max-width: 700px;
      margin-left: auto;
      margin-right: auto;
      border-collapse: collapse;
    }

    th, td {
      border: 1px solid black;
      padding: 8px;
    }

    th {
      background-color: #f2f2f2;
    }

    th:first-child, td:first-child {
      text-align: center;
    }

    input {
      width: 60px;
      margin: 4px;
    }

    button {
      margin: 6px;
      padding: 5px 10px;
    }

    @media screen and (max-width: 768px) {
      .main-container {
        flex-direction: column;
        align-items: center;
      }

      .form-container {
        width: 100%;
      }
    }
  </style>
</head>
<body>


  <div class="main-container">
    <!-- Tableau -->
    <div class="table-container">
      <h2 style="text-align:center;">History</h2>
      <table id="dataTable">
        <thead>
          <tr><th>Num</th><th>Time</th><th>Id</th><th>Pressure</th><th>Battery</th></tr>
        </thead>
        <tbody></tbody>
      </table>
    </div>

    <!-- Formulaire -->
    <div class="form-container">
      <h3>Time update</h3>
      <div>
        <label>Day:</label><input type="number" id="day" min="1" max="31"><br>
        <label>Month:</label><input type="number" id="month" min="1" max="12"><br>
        <label>Year:</label><input type="number" id="year" min="2024" max="2099"><br>
        <label>Hours:</label><input type="number" id="hour" min="0" max="23"><br>
        <label>Minutes:</label><input type="number" id="minute" min="0" max="59"><br>
        <label>Secondes:</label><input type="number" id="second" min="0" max="59"><br>
        <button onclick="fillWithBrowserTime()">Browser time</button>
        <button onclick="sendDateTime()">Update</button>
      </div>
    </div>
  </div>

  <script>
    function refreshTable() {
      fetch("/data").then(r => r.json()).then(data => {
        let tbody = document.querySelector("#dataTable tbody");
        tbody.innerHTML = "";
        data.forEach(row => {
          let tr = document.createElement("tr");
          tr.innerHTML =
            `<td data-label="Num">${row.Num}</td>` +
            `<td data-label="Time">${row.Time}</td>` +
            `<td data-label="Id">${row.ID}</td>` +
            `<td data-label="Pressure">${row.Pressure}</td>` +
            `<td data-label="Battery">${row.Battery}</td>`;
          tbody.appendChild(tr);
        });
      });
    }

    function fillWithBrowserTime() {
      const now = new Date();
      document.getElementById("day").value = now.getDate();
      document.getElementById("month").value = now.getMonth() + 1;
      document.getElementById("year").value = now.getFullYear();
      document.getElementById("hour").value = now.getHours();
      document.getElementById("minute").value = now.getMinutes();
      document.getElementById("second").value = now.getSeconds();
    }

    function sendDateTime() {
      const payload = {
        day: parseInt(document.getElementById("day").value),
        month: parseInt(document.getElementById("month").value),
        year: parseInt(document.getElementById("year").value),
        hour: parseInt(document.getElementById("hour").value),
        minute: parseInt(document.getElementById("minute").value),
        second: parseInt(document.getElementById("second").value),
      };

      if (Object.values(payload).some(v => isNaN(v))) {
        alert("Merci de remplir tous les champs correctement.");
        return;
      }

      fetch("/set-time", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload)
      })
      .then(r => r.text())
      .then(msg => alert("Réponse ESP32 : " + msg))
      .catch(err => alert("Erreur : " + err));
    }

    setInterval(refreshTable, 2000);
    refreshTable();
  </script>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleData()
{
  char tbuf[32];

  int index = 0xff;
  unsigned int maxNum = 0;
  bool first = true;

  for (int i = 0; i < HISTORY_LENGTH; i++)
  {
    if (history[i].num > maxNum)
    {
      maxNum = history[i].num;
      index = i;
    }
  }

  String json = "[";
  for (int i = 0; i < HISTORY_LENGTH; ++i)
  {
    if (index < 0)
      index = HISTORY_LENGTH - 1;

    if (String(history[index].Battery) != "")
    {
      if (first)
        first = false;
      else
        json += ",";

      json += "{";
      json += "\"Num\":\"" + String(history[index].num) + "\",";

      snprintf(tbuf, sizeof(tbuf), "%02d/%02d/%02d - %02d:%02d:%02d",
               history[index].time.tm_mday, history[index].time.tm_mon + 1, history[index].time.tm_year % 100,
               history[index].time.tm_hour, history[index].time.tm_min, history[index].time.tm_sec);
      String timeStr = String(tbuf);

      json += "\"Time\":\"" + timeStr + "\",";

      json += "\"ID\":\"" + String(history[index].ID) + "\",";

      snprintf(tbuf, sizeof(tbuf), "%d PSI - %.2f bars", history[index].Pressure * 2, history[index].Pressure * 2 / 14.504);
      String pressureStr = String(tbuf);

      json += "\"Pressure\":\"" + pressureStr + "\",";

      json += "\"Battery\":\"" + String(history[index].Battery) + "\"";
      json += "}";
    }

    index--;
  }
  json += "]";

  server.send(200, "application/json", json);
}

void handleSetTime()
{
  if (!server.hasArg("plain"))
  {
    server.send(400, "text/plain", "Aucune donnée reçue");
    return;
  }

  String body = server.arg("plain");
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, body);

  if (error)
  {
    server.send(400, "text/plain", "JSON invalide");
    return;
  }

  int day = doc["day"];
  int month = doc["month"];
  int year = doc["year"];
  int hour = doc["hour"];
  int minute = doc["minute"];
  int second = doc["second"];

  if (day < 1 || day > 31 || month < 1 || month > 12 ||
      year < 2024 || hour > 23 || minute > 59 || second > 59)
  {
    server.send(400, "text/plain", "Valeurs invalides");
    return;
  }

  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;

  time_t epoch = mktime(&t);
  if (epoch == -1)
  {
    server.send(500, "text/plain", "Erreur conversion date/heure");
    return;
  }

  struct timeval now = {.tv_sec = epoch, .tv_usec = 0};
  settimeofday(&now, nullptr);
  updateTime(epoch);

  server.send(200, "text/plain", "Heure mise à jour à " + String(asctime(&t)));
}

void initWeb(void)
{
  WiFi.softAP(ssid, password);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/set-time", HTTP_POST, handleSetTime);
  server.begin();

  MDNS.begin("tankreader");
}

void loopWeb(void)
{
  server.handleClient();
}