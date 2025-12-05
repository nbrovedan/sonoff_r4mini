#include "webpage.h"
#include <Preferences.h>

WebPage::WebPage(WebServer* server) {
    _server = server;
}

void WebPage::setNetworkInfo(IPAddress ip, String mac) {
    _ip = ip;
    _mac = mac;
}

void WebPage::setStatus(bool lampOn) {
    _lampOn = lampOn;

    time_t now = time(nullptr);
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M:%S", localtime(&now));

    String linha = "🕒 " + String(buffer) + " → " + (_lampOn ? "Ligada" : "Desligada") + "<br>";
    _historico = linha + _historico;
}

void WebPage::onToggle(std::function<void(void)> cb) {
    _callback = cb;
}

String WebPage::getWifiBars(int rssi) {
    if (rssi == 0) return "-----";

    int bars = 0;
    if (rssi >= -55) bars = 5;
    else if (rssi >= -60) bars = 4;
    else if (rssi >= -67) bars = 3;
    else if (rssi >= -75) bars = 2;
    else if (rssi >= -85) bars = 1;

    String out = "";
    for (int i = 0; i < bars; i++) out += "▮";
    for (int i = bars; i < 5; i++) out += "▯";

    return out;
}

void WebPage::setupRoutes() {

    // ======================================================
    // PÁGINA PRINCIPAL
    // ======================================================
    _server->on("/", [this]() {
        String html = R"rawliteral(
            <!DOCTYPE html>
            <html lang='pt-BR'>
            <head>
            <meta charset='UTF-8'>
            <meta name='viewport' content='width=device-width, initial-scale=1'>
            <title>Lâmpada Lavanderia</title>

            <style>
                body {
                    font-family: Arial, sans-serif;
                    background: #f2f2f2;
                    margin: 0;
                    padding: 20px;
                    display: flex;
                    justify-content: center;
                }

                .container {
                    max-width: 600px;
                    width: 100%;
                }

                .card {
                    background: #fff;
                    padding: 18px;
                    border-radius: 12px;
                    box-shadow: 0 0 10px rgba(0,0,0,0.15);
                    margin-bottom: 20px;
                }

                h1 {
                    text-align: center;
                    margin-bottom: 20px;
                    font-size: 26px;
                }

                .info-row {
                    display: flex;
                    justify-content: space-between;
                    align-items: center;
                    gap: 10px;
                    flex-wrap: wrap;
                }

                .info-list {
                    line-height: 1.6em;
                    font-size: 15px;
                }

                .btn {
                    padding: 10px 14px;
                    border: none;
                    border-radius: 8px;
                    font-size: 14px;
                    cursor: pointer;
                    transition: 0.2s;
                    white-space: nowrap;
                    color: white;
                }

                .btn-blue { background: #0077cc; }
                .btn-blue:hover { background: #3399ff; }

                .btn-red { background: #cc0000; }
                .btn-red:hover { background: #ff3333; }

                .btn-green { background: #00994d; }
                .btn-green:hover { background: #00cc66; }

                #lampButton {
                    width: 100%;
                    padding: 14px;
                    font-size: 18px;
                    border-radius: 10px;
                    font-weight: bold;
                }

                .lamp-on { background: #00994d; color: white; }
                .lamp-off { background: #cc0000; color: white; }

            </style>
            </head>

            <body>
            <div class='container'>

            <h1>💡 Lâmpada Lavanderia</h1>

            <!-- CARD SUPERIOR -->
            <div class='card'>
                <div class='info-row'>
                    <div class='info-list'>
                        <div>🛜 Wi-Fi: %WIFI%</div>
                        <div>🌐 SSID: %SSID%</div>
                        <div>🔢 IP: %IP%</div>
                        <div>🔠 MAC: %MAC%</div>
                    </div>

                    <div style="display:flex; flex-direction:column; gap:8px;">
                        <button class='btn btn-blue' onclick='openWifi()'>Trocar Wi-Fi</button>
                        <button class='btn btn-blue' onclick='openUpload()'>Atualizar</button>
                        <button class='btn btn-red' onclick='reboot()'>Reiniciar</button>
                    </div>
                </div>
            </div>

            <!-- CARD DA LÂMPADA -->
            <div class='card'>
                <h3 style="margin-top:0;">Controle da Lâmpada</h3>
                <button id='lampButton' class='lamp-off' onclick='toggleLamp()'>
                    Desligada
                </button>
            </div>

            </div> <!-- container -->

            <!-- MODAL OTA -->
            <div id='modal' style='display:none; position:fixed; top:0; left:0; 
                width:100%; height:100%; background:rgba(0,0,0,0.6);
                justify-content:center; align-items:center;'>
            <div style='background:white; padding:20px; border-radius:10px; width:80%; max-width:300px; text-align:center;'>
                <h3>Atualizar Firmware</h3>
                <input type='file' id='file'><br><br>
                <button class='btn btn-blue' onclick='upload()'>Enviar</button>
                <button class='btn btn-red' onclick='closeUpload()'>Cancelar</button>
                <p id='status' style='margin-top:10px;'></p>
            </div>
            </div>

            <!-- MODAL WI-FI -->
            <div id='wifiModal' style='display:none; position:fixed; top:0; left:0;
                width:100%; height:100%; background:rgba(0,0,0,0.6);
                justify-content:center; align-items:center; z-index:9999;'>

                <div style='background:white; padding:20px; border-radius:12px;
                    width:90%; max-width:350px; text-align:center;'>

                    <h3>Selecionar Rede Wi-Fi</h3>

                    <!-- Lista de redes -->
                    <div id="wifiList" 
                        style="max-height:250px; overflow-y:auto; margin-top:10px; border:1px solid #ddd; border-radius:8px; padding:10px; text-align:left;">
                        Buscando redes...
                    </div>

                    <!-- Caixa de senha -->
                    <div id="wifiPassArea" style="display:none; margin-top:20px;">
                        <h4 id="wifiChosen"></h4>
                        <input id="wifiPass" type="password" placeholder="Senha" 
                            style="width:100%; padding:10px; border-radius:6px; border:1px solid #ccc;">
                        <br><br>
                        <button class="btn btn-blue" onclick="saveWifi()">Salvar</button>
                    </div>

                    <p id="wifiMsg" style="margin-top:10px;"></p>

                    <button class='btn btn-red' style="margin-top:15px;" onclick='closeWifi()'>
                        Fechar
                    </button>
                </div>
            </div>



            <script>
            // ---------------- Atualiza status da lâmpada ----------------
            function refreshLamp() {
                fetch('/status')
                .then(r => r.json())
                .then(j => {
                    const b = document.getElementById('lampButton');
                    if (j.on) {
                        b.textContent = "Ligada";
                        b.className = "lamp-on";
                    } else {
                        b.textContent = "Desligada";
                        b.className = "lamp-off";
                    }
                });
            }
            setInterval(refreshLamp, 2000);
            refreshLamp();

            function toggleLamp() {
                fetch('/toggle', {method:'POST'});
                setTimeout(refreshLamp, 300);
            }

            function getWifiBars(rssi) {
                if (rssi === 0) return "-----";

                let bars = 0;

                if (rssi >= -55) bars = 5;
                else if (rssi >= -60) bars = 4;
                else if (rssi >= -67) bars = 3;
                else if (rssi >= -75) bars = 2;
                else if (rssi >= -85) bars = 1;

                let out = "";
                for (let i = 0; i < bars; i++) out += "▮";
                for (let i = bars; i < 5; i++) out += "▯";

                return out;
            }

            // ----------------- OTA -----------------
            function openUpload(){ document.getElementById('modal').style.display='flex'; }
            function closeUpload(){ document.getElementById('modal').style.display='none'; }

            async function upload() {
                const file = document.getElementById('file').files[0];
                const status = document.getElementById('status');

                if (!file) {
                    status.innerText = "Selecione um arquivo!";
                    return;
                }

                status.innerHTML = "📤 Enviando firmware...<br><b>0%</b>";
                
                const form = new FormData();
                form.append('update', file);

                // XHR permite acompanhar o progresso — fetch NÃO permite.
                const xhr = new XMLHttpRequest();
                xhr.open("POST", "/update", true);

                xhr.upload.onprogress = function(e) {
                    if (e.lengthComputable) {
                        let pct = Math.round((e.loaded / e.total) * 100);
                        status.innerHTML = "📤 Enviando firmware...<br><b>" + pct + "%</b>";
                    }
                };

                xhr.onload = function() {
                    if (xhr.status === 200) {
                        status.innerHTML = "✅ Atualização concluída! Reiniciando...";
                        setTimeout(() => location.reload(), 5000);
                    } else {
                        status.innerHTML = "❌ Erro no upload!";
                    }
                };

                xhr.onerror = function() {
                    status.innerHTML = "❌ Erro de comunicação!";
                };

                xhr.send(form);
            }

            // ------------------ MODAL WI-FI ------------------
            let selectedSSID = null;

            // abrir modal
            function openWifi() {
                document.getElementById('wifiModal').style.display = 'flex';
                loadWiFiList();   // primeira busca imediata
            }

            // fechar modal
            function closeWifi() {
                document.getElementById('wifiModal').style.display = 'none';
                selectedSSID = null;
                document.getElementById('wifiPassArea').style.display = "none";
                document.getElementById('wifiMsg').innerText = "";
            }

            // carregar lista de redes
            function loadWiFiList() {
                fetch('/scan')
                    .then(r => r.json())
                    .then(list => {
                        let out = "";

                        if (list.length === 0) {
                            out = "<i>Buscando redes...</i>";
                        } else {
                            list.forEach(w => {
                                const bars = getWifiBars(w.rssi);

                                out += `
                                    <div style="padding:8px; border-bottom:1px solid #eee; cursor:pointer;"
                                        onclick='selectSSID("${w.ssid}")'>
                                        <b>${w.ssid}</b> — ${bars}
                                    </div>`;
                            });
                        }

                        document.getElementById("wifiList").innerHTML = out;
                    });
            }

            // selecionar rede
            function selectSSID(ssid) {
                selectedSSID = ssid;
                document.getElementById("wifiChosen").innerText = "Rede selecionada: " + ssid;
                document.getElementById("wifiPassArea").style.display = "block";
            }

            // salvar nova rede
            function saveWifi() {
                if (!selectedSSID) return;

                const pass = document.getElementById("wifiPass").value;
                const msg = document.getElementById("wifiMsg");

                msg.innerText = "Salvando...";

                fetch(`/setwifi?ssid=${selectedSSID}&pass=${pass}`)
                    .then(r => r.text())
                    .then(text => {
                        msg.innerText = text + " Reiniciando...";

                        setTimeout(() => {
                            location.reload();
                        }, 3000);
                    });
            }

            // atualizar lista a cada 3s enquanto o modal estiver aberto
            setInterval(() => {
                const modal = document.getElementById("wifiModal");
                if (modal.style.display === "flex") loadWiFiList();
            }, 3000);


            // ----------------- REBOOT -----------------
            async function reboot() {
                if (!confirm("Tem certeza que deseja reiniciar?")) return;
                await fetch('/reboot');
                alert("Reiniciando...");
                setTimeout(()=>location.reload(), 4000);
            }
            </script>

            </body>
            </html>
            )rawliteral";

        html.replace("%SSID%", WiFi.SSID());
        html.replace("%WIFI%", getWifiBars(WiFi.RSSI()));
        html.replace("%IP%", _ip.toString());
        html.replace("%MAC%", _mac);

        _server->send(200, "text/html", html);
    });

    // ======================================================
    // STATUS JSON
    // ======================================================
    _server->on("/status", [this]() {
        String json = "{\"on\":" + String(_lampOn ? 1 : 0) +
                      ",\"historico\":\"" + _historico + "\"}";
        _server->send(200, "application/json", json);
    });

    // ======================================================
    // ALTERAR ESTADO
    // ======================================================
    _server->on("/toggle", HTTP_POST, [this]() {
        if (_callback) _callback();
        _server->send(200, "text/plain", "ok");
    });

    // ======================================================
    // LISTAR REDES Wi-Fi
    // ======================================================
    _server->on("/scan", [this]() {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) {
            _server->send(200, "application/json", "[]");
            return;
        }

        if (n < 0) {
            WiFi.scanNetworks(true);
            _server->send(200, "application/json", "[]");
            return;
        }

        String json = "[";
        for (int i = 0; i < n; i++) {
            if (i) json += ",";
            json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
        }
        json += "]";

        WiFi.scanDelete();
        WiFi.scanNetworks(true); // iniciar próximo scan assíncrono

        _server->send(200, "application/json", json);
    });

    // ======================================================
    // Salvar Wi-Fi
    // ======================================================
    _server->on("/setwifi", [this]() {
        String ssid = _server->arg("ssid");
        String pass = _server->arg("pass");

        if (ssid.length() == 0) {
            _server->send(400, "text/plain", "SSID inválido!");
            return;
        }

        Preferences prefs;
        prefs.begin("wifi", false);
        prefs.putString("ssid", ssid);
        prefs.putString("pass", pass);
        prefs.end();

        _server->send(200, "text/plain", "Wi-Fi salvo. Reiniciando...");
        delay(800);
        ESP.restart();
    });

    // ======================================================
    // OTA
    // ======================================================
    _server->on("/update", HTTP_POST,
        [this]() {
            if (Update.hasError()) {
                _server->send(500, "text/plain", "❌ Erro na atualização!");
            } else {
                _server->send(200, "text/plain", "✅ Atualizado! Reiniciando...");
            }
            delay(800);
            ESP.restart();
        },
        [this]() {
            HTTPUpload& up = _server->upload();

            if (up.status == UPLOAD_FILE_START) {
                Serial.printf("OTA inicio: %s\n", up.filename.c_str());
                Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH);
            }

            else if (up.status == UPLOAD_FILE_WRITE) {
                size_t written = Update.write(up.buf, up.currentSize);
                if (written != up.currentSize) {
                    Update.printError(Serial);
                }
            }

            else if (up.status == UPLOAD_FILE_END) {
                if (!Update.end(true)) {
                    Update.printError(Serial);
                }
            }
        }
        );

    _server->on("/config", [this]() {
    String html = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset='UTF-8'>
            <meta name='viewport' content='width=device-width, initial-scale=1'>
            <title>Configurar Wi-Fi</title>
            <style>
                body { font-family: Arial; padding: 20px; }
                .wifi { padding: 14px; border: 1px solid #ccc;
                        border-radius: 8px; margin-bottom: 10px;
                        cursor: pointer; }
                .wifi:hover { background:#f0f0f0; }
                #list { margin-top:20px; }
                #passBox { display:none; margin-top:20px; }
                button { padding:10px 20px; border-radius:8px; border:none;
                        color:white; background:#0077cc; cursor:pointer; }
            </style>
        </head>
        <body>

        <h2>Selecione a Rede Wi-Fi</h2>
        <div id="list">Buscando redes...</div>

        <div id="passBox">
            <h3 id="chosen"></h3>
            <input id="pass" placeholder="Senha" style="width:100%; padding:10px;">
            <br><br>
            <button onclick="save()">Salvar</button>
        </div>

        <script>
        let selectedSSID = "";

        function load() {
            fetch('/scan')
            .then(r => r.json())
            .then(list => {
                let out = "";
                list.forEach(w => {
                    out += `<div class='wifi' onclick='pick("${w.ssid}")'>
                                ${w.ssid} (${w.rssi} dBm)
                            </div>`;
                });
                document.getElementById("list").innerHTML = out;
            });
        }

        function pick(ssid) {
            selectedSSID = ssid;
            document.getElementById("chosen").innerText = "Rede selecionada: " + ssid;
            document.getElementById("passBox").style.display = "block";
        }

        function save() {
            let pass = document.getElementById("pass").value;
            fetch(`/setwifi?ssid=${selectedSSID}&pass=${pass}`)
                .then(r => r.text())
                .then(t => alert(t));
        }

        setInterval(load, 3000);
        load();
        </script>

        </body></html>
        )rawliteral";

        _server->send(200, "text/html", html);
    });


}
