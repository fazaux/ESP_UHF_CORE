// HTMLPage.cpp
#include "HTMLPage.h"
#include <WiFi.h>

String HTMLPage() {
    String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>WebSocket Client</title>
    <style>
        body {
            font-family: Arial, sans-serif;
        }
        .container {
            width: 50%;
            margin: 0 auto;
        }
        .status {
            margin-bottom: 10px;
            color: red;
        }
        .message-log {
            width: 100%;
            height: 300px;
            border: 1px solid #ccc;
            margin-top: 10px;
            overflow-y: scroll;
        }
        .input-group {
            display: flex;
            margin-top: 10px;
        }
        .input-group input, .input-group button {
            margin-right: 10px;
        }
        .input-group button {
            padding: 5px 10px;
        }
    </style>
</head>
<body>
    <div> class="container">
      <div>
      )rawliteral";

    if (WiFi.status() == WL_CONNECTED) {
        page += "<p>Connected to SSID: " + WiFi.SSID() + "</p>";
        page += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
    } else {
        page += "<p>Not connected to any WiFi network.</p>";
    }

    page += R"rawliteral(
        </div>
    </div>
    <div class="container">
        <h2>UHF Network Configuration</h2>
        <div>
            <form action="/save">
                <label for="ssid">SSID</label>
                <input type="text" name="ssid" id="ssid">
                <label for="ssid">Password</label>
                <input type="text" name="password" id="password">
                <button type="submit">Save</button>
            </form>
        </div>
    </div>
    <div class="container">
        <h2>WebSocket Client</h2>
        <div>
            <label for="url">URL:</label>
            <input type="text" id="url" value="ws://deras-rfid-config.local/ws">
            <button onclick="connectWebSocket()">Open</button>
        </div>
        <div class="status" id="status">Status: CLOSED</div>
        <div>
            <label for="request">Request:</label>
            <textarea id="request" rows="3"></textarea>
            <button onclick="sendMessage()">Send</button>
        </div>
        <div>
            <h3>Message Log</h3>
            <button onclick="clearLog()">Clear</button>
            <div class="message-log" id="message-log"></div>
        </div>
    </div>

    <script>
        let ws;

        function connectWebSocket() {
            const url = document.getElementById('url').value;
            ws = new WebSocket(url);

            ws.onopen = function() {
                document.getElementById('status').textContent = 'Status: OPEN';
                document.getElementById('status').style.color = 'green';
            };

            ws.onmessage = function(event) {
                const messageLog = document.getElementById('message-log');
                const message = document.createElement('div');
                message.textContent = 'Received: ' + event.data;
                messageLog.appendChild(message);
                messageLog.scrollTop = messageLog.scrollHeight;
            };

            ws.onclose = function() {
                document.getElementById('status').textContent = 'Status: CLOSED';
                document.getElementById('status').style.color = 'red';
            };

            ws.onerror = function(error) {
                console.error('WebSocket Error: ', error);
            };
        }

        function sendMessage() {
            const message = document.getElementById('request').value;
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(message);
                const messageLog = document.getElementById('message-log');
                const sentMessage = document.createElement('div');
                sentMessage.textContent = 'Sent: ' + message;
                messageLog.appendChild(sentMessage);
                messageLog.scrollTop = messageLog.scrollHeight;
            } else {
                alert('WebSocket is not open.');
            }
        }

        function clearLog() {
            document.getElementById('message-log').innerHTML = '';
        }
    </script>
</body>
</html>
)rawliteral";

    return page;
}
