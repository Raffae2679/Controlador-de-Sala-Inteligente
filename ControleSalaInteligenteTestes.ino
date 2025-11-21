/*
 * PROJETO A3 - CONTROLE DE SALA INTELIGENTE 
*/

#include <WiFi.h> // Biblioteca para conexão WI-FI
#include <WebServer.h> // Biblioteca para utilizar Webserver
#include <LiquidCrystal_I2C.h> // Biblioteca do LCD
#include <OneWire.h> // Biblioteca para uso do sensor de temperatura
#include <DallasTemperature.h> // Biblioteca do sensor de temperatura

// --- Configurações de Rede ---
const char* ssid = "apt502"; // Nome da rede wi-fi
const char* password = "72lc06r12"; // Senha da rede wi-fi

// --- Configurações do PWM ---
const int PWM_FREQ = 5000; // Frequencia para motores DC
const int PWM_RESOLUTION = 8; // 8 Bits de resolução (0 a 255)

WebServer server(80); // Instancia Webserver

// --- Pinos do Projeto --
#define PINO_BTN_MODO 19 // Botão que troca de modo automatico para modo manual
#define PINO_BTN_SWITCH 18 // Botão para acender led no modo manual
#define PINO_POT 34 // Pino do potenciometro que controla velocidade da ventoinha
#define PINO_VENTOINHA 15 // Pino do terminal de transistor
#define PINO_SENSOR_LUZ 16 // Pino do modulo ldr
#define PINO_SENSOR_TEMP 17 // Pino do sensor de temperatura (DS18b20)
#define PINO_RELE 5 // Pino para o modulo rele

// --- Variáveis Globais de Estado ---
float temperatura = 0.0;
int posPotenciometro = 0;
int velocidadePWM = 0;
int velocidadePWMDisplay = 0; // Para exibir velocidade da ventoinha na escala 0 a 100%

volatile bool modoAutomatico = 0; // 1 modo Automatico, 0 modo Manual
volatile bool estadoRele = false;  // Iniciando como false para consistência

// --- Inicialização dos Objetos ---
LiquidCrystal_I2C lcd(0x27, 16, 2);
OneWire oneWire(PINO_SENSOR_TEMP);
DallasTemperature sensors(&oneWire);

// ==========================================
// CÓDIGO HTML/CSS/JS
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="pt-BR">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Painel ESP32</title>
  <style>
    body { font-family: Arial, sans-serif; background-color: #f4f4f9; text-align: center; margin: 0; padding: 20px; }
    h2 { color: #333; }
    .container { max-width: 600px; margin: auto; display: grid; gap: 20px; }
    .card { background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); transition: 0.3s; }
    .value { font-size: 2rem; font-weight: bold; color: #007bff; }
    .label { font-size: 1rem; color: #666; margin-bottom: 10px; }
    .disabled-card { opacity: 0.5; pointer-events: none; filter: grayscale(100%); }
    .switch { position: relative; display: inline-block; width: 60px; height: 34px; }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }
    .slider:before { position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #2196F3; }
    input:checked + .slider:before { transform: translateX(26px); }
  </style>
</head>
<body>
  <h2>Painel de Controle ESP32</h2>
  
  <div class="container">
    
    <div class="card" style="border-left: 5px solid #ff9800;">
      <div class="label">Modo de Operação</div>
      <label class="switch">
        <input type="checkbox" id="modeSwitch" onchange="toggleMode(this)">
        <span class="slider"></span>
      </label>
      <div id="modeStatusText" style="font-weight:bold; margin-top:5px;">Manual</div>
    </div>

    <div class="card">
      <div class="label">Temperatura</div>
      <div class="value"><span id="temp">--</span> &deg;C</div>
    </div>
    
    <div class="card">
      <div class="label">Luminosidade</div>
      <div class="value"><span id="light">--</span></div>
    </div>

    <div class="card" id="cardLight">
      <div class="label">Controle da Luz</div>
      <label class="switch">
        <input type="checkbox" id="lightSwitch" onchange="toggleLight(this)">
        <span class="slider"></span>
      </label>
      <div id="lightStatusText">Desligada</div>
    </div>

    <div class="card" id="cardFan">
      <div class="label">Ventoinha</div>
      <label class="switch">
        <input type="checkbox" id="fanSwitch" onchange="toggleFan(this)">
        <span class="slider"></span>
      </label>
      <div id="fanStatusText">Desligada</div>
    </div>
  </div>

<script>
  window.onload = function() {
    getReadings(); 
  };

  setInterval(getReadings, 2000);

  function getReadings() {
    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
        var obj = JSON.parse(this.responseText);
        
        // Atualiza Sensores
        document.getElementById("temp").innerHTML = obj.temperature;
        document.getElementById("light").innerHTML = obj.light;
        
        // Atualiza Modo
        var isAuto = (obj.mode === "auto");
        var modeSwitch = document.getElementById("modeSwitch");
        if (modeSwitch.checked !== isAuto) {
            modeSwitch.checked = isAuto;
            document.getElementById("modeStatusText").innerText = isAuto ? "Automático" : "Manual";
            setControlsState(isAuto);
        }

        // Atualiza Interface APENAS se não estiver em modo automático
        // Luz
        var lightSw = document.getElementById("lightSwitch");
        var lightStateBool = (obj.lightState == 1);
        if (lightSw.checked !== lightStateBool) {
            lightSw.checked = lightStateBool;
            document.getElementById("lightStatusText").innerText = lightStateBool ? "Ligada" : "Desligada";
        }

        // Ventoinha (Agora é Toggle)
        var fanSw = document.getElementById("fanSwitch");
        // Consideramos ligada se a velocidade for > 0
        var fanStateBool = (parseInt(obj.fanSpeed) > 0);
        if (fanSw.checked !== fanStateBool) {
            fanSw.checked = fanStateBool;
            document.getElementById("fanStatusText").innerText = fanStateBool ? "Ligada" : "Desligada";
        }
      }
    };
    xhttp.open("GET", "/readings", true);
    xhttp.send();
  }

  function toggleMode(element) {
    var isAuto = element.checked;
    var modeStr = isAuto ? "auto" : "manual";
    
    document.getElementById("modeStatusText").innerText = isAuto ? "Automático" : "Manual";
    setControlsState(isAuto);

    if (isAuto) {
        document.getElementById("lightSwitch").checked = false;
        document.getElementById("lightStatusText").innerText = "Desligada";
        document.getElementById("fanSwitch").checked = false;
        document.getElementById("fanStatusText").innerText = "Desligada";
    }

    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", "/mode?value=" + modeStr, true);
    xhttp.send();
  }

  function setControlsState(isAuto) {
    var cardLight = document.getElementById("cardLight");
    var cardFan = document.getElementById("cardFan");
    var inputLight = document.getElementById("lightSwitch");
    var inputFan = document.getElementById("fanSwitch");

    if (isAuto) {
      cardLight.classList.add("disabled-card");
      cardFan.classList.add("disabled-card");
      inputLight.disabled = true;
      inputFan.disabled = true;
    } else {
      cardLight.classList.remove("disabled-card");
      cardFan.classList.remove("disabled-card");
      inputLight.disabled = false;
      inputFan.disabled = false;
    }
  }

  function toggleLight(element) {
    var state = element.checked ? "1" : "0";
    document.getElementById("lightStatusText").innerText = element.checked ? "Ligada" : "Desligada";
    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", "/light?state=" + state, true);
    xhttp.send();
  }

  function toggleFan(element) {
    // Se ligar, manda 200. Se desligar, manda 0.
    var val = element.checked ? "200" : "0";
    document.getElementById("fanStatusText").innerText = element.checked ? "Ligada" : "Desligada";
    var xhttp = new XMLHttpRequest();
    xhttp.open("GET", "/fan?value=" + val, true);
    xhttp.send();
  }
</script>
</body>
</html>
)rawliteral";

// ==========================================
// FUNÇÕES DO SERVIDOR
// ==========================================

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleReadings() {
  float t = temperatura;
  if (isnan(t)) t = 0.0;

  String lightStatus;
  if (digitalRead(PINO_SENSOR_LUZ)) {
    lightStatus = "Escuro";
  } else {
    lightStatus = "Claro";
  }

  String json = "{";
  json += "\"temperature\":\"" + String(t, 1) + "\",";
  json += "\"light\":\"" + lightStatus + "\",";
  json += "\"mode\":\"" + String(modoAutomatico ? "auto" : "manual") + "\",";
  json += "\"fanSpeed\":\"" + String(velocidadePWM) + "\",";  // Enviando a variavel real PWM
  json += "\"lightState\":\"" + String(estadoRele ? 1 : 0) + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleMode() {
  if (server.hasArg("value")) {
    String val = server.arg("value");
    if (val == "auto") {
      modoAutomatico = true;
      Serial.println("Modo: Auto. Resetando...");

      estadoRele = false;
      velocidadePWM = 0;
      // A atualização física ocorre no loop

    } else {
      modoAutomatico = false;
      Serial.println("Modo: Manual");
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleLight() {
  if (!modoAutomatico && server.hasArg("state")) {
    String state = server.arg("state");
    if (state == "1") {
      estadoRele = true;
    } else {
      estadoRele = false;
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleFan() {
  if (!modoAutomatico && server.hasArg("value")) {
    velocidadePWM = server.arg("value").toInt();
    // Proteção simples
    if (velocidadePWM < 0) velocidadePWM = 0;
    if (velocidadePWM > 255) velocidadePWM = 255;
  }
  server.send(200, "text/plain", "OK");
}


void setup() {
  Serial.begin(115200);
  pinMode(PINO_SENSOR_LUZ, INPUT);
  pinMode(PINO_POT, INPUT);
  pinMode(PINO_RELE, OUTPUT);
  pinMode(PINO_BTN_MODO, INPUT_PULLUP);
  pinMode(PINO_BTN_SWITCH, INPUT_PULLUP);

  // Configura o pino da ventoinha para PWM
  ledcAttach(PINO_VENTOINHA, PWM_FREQ, PWM_RESOLUTION);

  // Interrupção
  attachInterrupt(digitalPinToInterrupt(PINO_BTN_MODO), mudarModo, FALLING);
  attachInterrupt(digitalPinToInterrupt(PINO_BTN_SWITCH), ligarLuz, FALLING);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Iniciando...");

  sensors.begin();

  Serial.print("Conectando ao ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(0, 1);
    lcd.print("Conectando WiFi");
  }

  Serial.println("\nWiFi Conectado!");
  Serial.print("Endereco IP: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectado!");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // Rotas 
  server.on("/", handleRoot);
  server.on("/readings", handleReadings);
  server.on("/mode", handleMode);
  server.on("/light", handleLight);
  server.on("/fan", handleFan);  // Adicionado de volta

  server.begin();
  delay(1000);
}

void loop() {
  server.handleClient(); // Escuta o servidor web
  lerSensores(); // Função que le os dados dos sensores
  aplicarLogicaControle(); // Função que aplica a lógica (Ex: Ligar ventoinha quando estiver 30°C)
  atualizarAtuadores(); // Função responsavel por ativar o rele
  atualizarLCD(); // Função que atualiza e exibe informações no LCD
}

void lerSensores() {
  sensors.requestTemperatures();
  temperatura = sensors.getTempCByIndex(0);
  posPotenciometro = analogRead(PINO_POT);

  // --- Informações para Debug no Terminal ---
  Serial.print(" | Temp: ");
  Serial.print(temperatura);
  Serial.print(" | Luz: ");
  Serial.print(digitalRead(PINO_SENSOR_LUZ) ? "Escuro" : "Claro");
  Serial.print(" | Pot: ");
  Serial.println(posPotenciometro);
}

void atualizarLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(modoAutomatico ? "Modo: AUTO" : "Modo: MANUAL");

  // --- Se estiver no modo automatico ele exibe a temperatura, caso não, mostra a velocidade da ventoinha ---
  if (modoAutomatico) {
    lcd.setCursor(10, 1);
    lcd.print(temperatura, 1);  // Temperatura com 1 decimal
    lcd.print("C");
  } else {
    velocidadePWMDisplay = map(velocidadePWM, 0, 255, 0, 100); // Converte a escala do pwm para 0 a 100%
    lcd.setCursor(9, 1);
    lcd.print("V:" + String(velocidadePWMDisplay) + "%");
  }

  lcd.setCursor(0, 1);
  lcd.print("Luz: ");

  // --- Se no modo automatico, pega a informação do sensor de luz, caso não, pega do estado do rele ---
  if (modoAutomatico) {
    lcd.print(digitalRead(PINO_SENSOR_LUZ) ? "ON" : "OFF");
  } else {
    lcd.print(estadoRele ? "ON" : "OFF");
  }
}

void aplicarLogicaControle() {
  if (modoAutomatico) {
    // --- LÓGICA MODO AUTOMÁTICO ---
    if (temperatura >= 30.0) {
      // Exemplo: Ligar ventoinha no auto se quente
      ledcWrite(PINO_VENTOINHA, 255);
    } else if (temperatura > 25.0) {
      ledcWrite(PINO_VENTOINHA, 125);
    } else {
      ledcWrite(PINO_VENTOINHA, 0);
    }

    if (digitalRead(PINO_SENSOR_LUZ)) {
      estadoRele = true;
    } else {
      estadoRele = false;
    }

  } else {
    // --- LÓGICA MODO MANUAL ---
    velocidadePWM = map(posPotenciometro, 0, 4095, 0, 255);

    ledcWrite(PINO_VENTOINHA, velocidadePWM); // Passa a velocidade da ventoinha a partir do valor obtido do potenciometro.
  }
}

void atualizarAtuadores() {
  digitalWrite(PINO_RELE, estadoRele ? LOW : HIGH);
}

void mudarModo() {
  // Alterna modo
  modoAutomatico = !modoAutomatico;
  // Reseta estados para segurança ao trocar
  estadoRele = false;
}

void ligarLuz() {
  if (!modoAutomatico) {
    estadoRele = !estadoRele;
  }
}