#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

// =============================================
// CONFIGURAÇÕES DE HARDWARE
// =============================================
#define RXD2 16                 // Serial2 RX (GPS)
#define TXD2 17                 // Serial2 TX (GPS)
#define SD_CS 5                 // Pino ChipSelect do SD Card
#define CHAVE_PIN 4             // Pino da chave ON/OFF (LOW=ON)
#define LED_MQTT_PIN 2          // Pino do LED indicador MQTT

// =============================================
// CONFIGURAÇÕES DE REDE
// =============================================

const char* ssid = "SUA_REDE_WIFI";
const char* password = "SENHA_WIFI";

// =============================================
// CONFIGURAÇÕES MQTT (HIVEMQ CLOUD)
// =============================================
const char* mqtt_server = "SEU_SERVIDOR_MQTT";
const int mqtt_port = 8883;
const char* mqtt_user = "mmqtt_user";
const char* mqtt_password = "mqtt_senha";
const char* topic = "frota/veiculo1";

// Certificado CA HiveMQ (obrigatório para TLS)
const char* root_ca = \
"-----BEGIN CERTIFICATE-----\n" \
"MIIDdzCCAl+gAwIBAgIEAgAAuTANBgkqhkiG9w0BAQUFADBaMQswCQYDVQQGEwJJ\n" \
"RTESMBAGA1UEChMJQmFsdGltb3JlMRMwEQYDVQQLEwpDeWJlclRydXN0MSIwIAYD\n" \
"VQQDExlCYWx0aW1vcmUgQ3liZXJUcnVzdCBSb290MB4XDTAwMDUxMjE4NDYwMFoX\n" \
"DTI1MDUxMjIzNTkwMFowWjELMAkGA1UEBhMCSUUxEjAQBgNVBAoTCUJhbHRpbW9y\n" \
"ZTETMBEGA1UECxMKQ3liZXJUcnVzdDEiMCAGA1UEAxMZQmFsdGltb3JlIEN5YmVy\n" \
"VHJ1c3QgUm9vdDCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAKMEuyKr\n" \
"mD1X6CZymrV51Cni4eiVgLGw41uOKymaZN+hXe2wCQVt2yguzmKiYv60iNoS6zjr\n" \
"IZ3AQSsBUnuId9Mcj8e6uYi1agnnc+gRQKfRzMpijS3ljwumUNKoUMMo6vWrJYeK\n" \
"mpYcqWe4PwzV9/lSEy/CG9VwcPCPwBLKBsua4dnKM3p31vjsufFoREJIE9LAwqSu\n" \
"XmD+tqYF/LTdB1kC1FkYmGP1pWPgkAx9XbIGevOF6uvUA65ehD5f/xXtabz5OTZy\n" \
"dc93Uk3zyZAsuT3lySNTPx8kmCFcB5kpvcY67Oduhjprl3RjM71oGDHweI12v/ye\n" \
"jl0qhqdNkNwnGjkCAwEAAaNFMEMwHQYDVR0OBBYEFOWdWTCCR1jMrPoIVDaGezq1\n" \
"BE3wMBIGA1UdEwEB/wQIMAYBAf8CAQMwDgYDVR0PAQH/BAQDAgEGMA0GCSqGSIb3\n" \
"DQEBBQUAA4IBAQCFDF2O5G9RaEIFoN27TyclhAO992T9Ldcw46QQF+vaKSm2eT92\n" \
"9hkTI7gQCvlYpNRhcL0EYWoSihfVCr3FvDB81ukMJY2GQE/szKN+OMY3EU/t3Wgx\n" \
"jkzSswF07r51XgdIGn9w/xZchMB5hbgF/X++ZRGjD8ACtPhSNzkE1akxehi/oCr0\n" \
"Epn3o0WC4zxe9Z2etciefC7IpJ5OCBRLbf1wbWsaY71k5h+3zvDyny67G7fyUIhz\n" \
"ksLi4xaNmjICq44Y3ekQEe5+NauQrz4wlHrQMz2nZQ/1/I6eYs9HRCwBXbsdtTLS\n" \
"R9I4LtD+gdwyah617jzV/OeBHRnDJELqYzmp\n" \
"-----END CERTIFICATE-----\n";

// =============================================
// OBJETOS GLOBAIS
// =============================================
HardwareSerial neogps(1);                  // UART2 para GPS
LiquidCrystal_I2C lcd(0x27, 20, 4);       // LCD I2C 20x4
TinyGPSPlus gps;                           // Objeto GPS
File dataFile;                             // Arquivo no SD
WiFiClientSecure espClient;                // Cliente WiFi seguro
PubSubClient client(espClient);            // Cliente MQTT

// =============================================
// VARIÁVEIS DE CONTROLE
// =============================================
unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 1000;  // Intervalo de atualização do LCD (ms)
int displayPage = 0;                         // Página atual do display
bool mqttEnabled = false;                    // Status do MQTT (controlado pela chave)
unsigned long lastMqttReconnectAttempt = 0;
const unsigned long mqttReconnectInterval = 5000;  // Intervalo de tentativa de reconexão MQTT

// =============================================
// SETUP INICIAL
// =============================================
void setup() {
  Serial.begin(115200);
  
  // Configura GPIOs
  pinMode(CHAVE_PIN, INPUT_PULLUP);
  pinMode(LED_MQTT_PIN, OUTPUT);
  digitalWrite(LED_MQTT_PIN, LOW);

  // Inicializa GPS
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);

  // Inicializa LCD
// Inicializa LCD
lcd.init();
lcd.backlight();
lcd.clear();
 
lcd.setCursor(0, 0);
lcd.print("BusCar v1.0     2025"); 
lcd.setCursor(0, 1);
lcd.print("--------------------");
lcd.setCursor(0, 3);
lcd.print("PI UNIVESP  Polo CPV");  
delay(2000);
lcd.clear();
lcd.print("BusCar v1.0     2025"); 
lcd.setCursor(0, 1);
lcd.print("--------------------");

  // Inicializa SD Card
  if (!SD.begin(SD_CS)) {
    mostrarMensagemLCD("Erro SD Card", "Verifique o cartao", 0);
    while(1); // Trava o sistema
  }

  // Verifica estado inicial da chave
  mqttEnabled = digitalRead(CHAVE_PIN) == LOW;
  atualizarStatusMQTT();

  // Conecta WiFi se MQTT ativado
  if (mqttEnabled) {
    conectarWiFi();
    conectarMQTT();
  }
}

// =============================================
// LOOP PRINCIPAL
// =============================================
void loop() {
  // 1. Verifica estado da chave
  verificarChave();

  // 2. Processa dados do GPS
  bool newData = processarGPS();

  // 3. Atualiza display LCD
  atualizarDisplay();

  // 4. Processa dados válidos
  if (newData && gps.location.isValid()) {
    gravarNoSD();
    visualizarSerial();
    
    if (mqttEnabled) {
      enviarParaMQTT();
    }
  }

  // 5. Gerencia conexão MQTT
  gerenciarConexaoMQTT();
}

// =============================================
// FUNÇÕES PRINCIPAIS
// =============================================

void verificarChave() {
  bool estadoChave = digitalRead(CHAVE_PIN) == LOW;
  if (estadoChave != mqttEnabled) {
    mqttEnabled = estadoChave;
    atualizarStatusMQTT();
    
    if (mqttEnabled && WiFi.status() != WL_CONNECTED) {
      conectarWiFi();
    }
  }
}

bool processarGPS() {
  bool newData = false;
  while (neogps.available()) {
    if (gps.encode(neogps.read())) {
      newData = true;
    }
  }
  return newData;
}

void atualizarDisplay() {
  if (millis() - lastDisplayUpdate >= displayInterval) {
    lastDisplayUpdate = millis();
    
    switch(displayPage) {
      case 0: // Página 1: Coordenadas
        mostrarDadosLCD("LAT: " + formatarFloat(gps.location.lat(), 6), 
                       "LON: " + formatarFloat(gps.location.lng(), 6));
        break;
        
      case 1: // Página 2: Velocidade e Satélites
        mostrarDadosLCD("VEL: " + formatarFloat(gps.speed.kmph(), 1) + "Km/h", 
                       "SAT: " + String(gps.satellites.value()));
        break;
        
      case 2: // Página 3: Altitude e Precisão
        mostrarDadosLCD("ALT: " + formatarFloat(gps.altitude.meters(), 0) + "m", 
                       "PREC: " + formatarFloat(gps.hdop.value()/100.0, 1) + "m");
        break;
        
      case 3: // Página 4: Data e Hora
        mostrarDadosLCD("HORA: " + formatarHora(), 
                       "DATA: " + formatarData());
        break;
    }
    
    displayPage = (displayPage + 1) % 4;
  }
}

void gerenciarConexaoMQTT() {
  if (mqttEnabled) {
    if (!client.connected()) {
      tentarReconexaoMQTT();
    }
    client.loop();
    
    // Piscar LED quando conectado
    static unsigned long lastBlink = 0;
    if (millis() - lastBlink > 1000 && client.connected()) {
      digitalWrite(LED_MQTT_PIN, !digitalRead(LED_MQTT_PIN));
      lastBlink = millis();
    }
  }
}

// =============================================
// FUNÇÕES DE COMUNICAÇÃO
// =============================================

void conectarWiFi() {
  mostrarMensagemLCD("WiFi", "Conectando...", 0);
  
  WiFi.begin(ssid, password);
  unsigned long startTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    mostrarMensagemLCD("WiFi Conectado", WiFi.localIP().toString(), 2000);
  } else {
    mostrarMensagemLCD("Erro WiFi", "Falha na conexao", 2000);
  }
}

// =============================================
// FUNÇÕES DE COMUNICAÇÃO (PARTE CORRIGIDA)
// =============================================

bool conectarMQTT() {  // Alterado de void para bool
  espClient.setInsecure();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  if (client.connect("ESP32_GPS", mqtt_user, mqtt_password)) {
    mostrarMensagemLCD("MQTT", "Conectado!", 1000);
    return true;
  } else {
    mostrarMensagemLCD("Erro MQTT", String(client.state()), 1000);
    return false;
  }
}

void tentarReconexaoMQTT() {
  if (millis() - lastMqttReconnectAttempt >= mqttReconnectInterval) {
    lastMqttReconnectAttempt = millis();
    
    if (conectarMQTT()) {  // Agora funciona pois a função retorna bool
      lastMqttReconnectAttempt = 0;
    }
  }
}

void enviarParaMQTT() {
  if (!client.connected()) return;

  StaticJsonDocument<256> doc;
  doc["lat"] = gps.location.lat();
  doc["lng"] = gps.location.lng();
  doc["vel"] = gps.speed.kmph();
  doc["alt"] = gps.altitude.meters();
  doc["sat"] = gps.satellites.value();
  doc["time"] = formatarTimestamp();

  char payload[256];
  serializeJson(doc, payload);

  if (client.publish(topic, payload)) {
    Serial.println("[MQTT] Dados enviados");
  } else {
    Serial.println("[MQTT] Falha no envio");
  }
}

// =============================================
// FUNÇÕES DE DADOS
// =============================================

void gravarNoSD() {
  StaticJsonDocument<256> doc;
  doc["latitude"] = gps.location.lat();
  doc["longitude"] = gps.location.lng();
  doc["velocidade"] = gps.speed.kmph();
  doc["altitude"] = gps.altitude.meters();
  doc["satelites"] = gps.satellites.value();
  doc["timestamp"] = formatarTimestamp();

  dataFile = SD.open("/dados_gps.json", FILE_APPEND);
  if (dataFile) {
    serializeJson(doc, dataFile);
    dataFile.println();
    dataFile.close();
    Serial.println("[SD] Dados gravados");
  } else {
    Serial.println("[SD] Erro ao gravar");
  }
}

// =============================================
// FUNÇÕES AUXILIARES
// =============================================

void atualizarStatusMQTT() {
  digitalWrite(LED_MQTT_PIN, mqttEnabled ? HIGH : LOW);
  mostrarMensagemLCD("Modo MQTT", mqttEnabled ? "ATIVADO" : "DESATIVADO", 1000);
}

String formatarFloat(float value, int decimals) {
  char buffer[20];
  dtostrf(value, 0, decimals, buffer);
  return String(buffer);
}

String formatarHora() {
  if (!gps.time.isValid()) return "--:--:--";
  
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", 
           gps.time.hour(), gps.time.minute(), gps.time.second());
  return String(buffer);
}

String formatarData() {
  if (!gps.date.isValid()) return "--/--/----";
  
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%02d/%02d/%04d", 
           gps.date.day(), gps.date.month(), gps.date.year());
  return String(buffer);
}

String formatarTimestamp() {
  if (!gps.time.isValid() || !gps.date.isValid()) return "";
  
  char buffer[25];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
           gps.date.year(), gps.date.month(), gps.date.day(),
           gps.time.hour(), gps.time.minute(), gps.time.second());
  return String(buffer);
}

void mostrarMensagemLCD(String linha1, String linha2, unsigned int delayTime) {
lcd.clear();
 
lcd.setCursor(0, 0);
lcd.print("BusCar v1.0     2025"); 
lcd.setCursor(0, 1);
lcd.print("--------------------");
  lcd.setCursor(2, 2);
  lcd.print(linha1);
  lcd.setCursor(2, 3);
  lcd.print(linha2);
  if (delayTime > 0) delay(delayTime);
}

void mostrarDadosLCD(String linha1, String linha2) {
lcd.clear();
 
lcd.setCursor(0, 0);
lcd.print("BusCar v1.0     2025"); 
lcd.setCursor(0, 1);
lcd.print("--------------------");
  lcd.setCursor(2, 2);
  lcd.print(linha1);
  lcd.setCursor(2, 3);
  lcd.print(linha2);
}

void visualizarSerial() {
  Serial.println("=================================");
  Serial.print("Latitude: "); Serial.println(gps.location.lat(), 6);
  Serial.print("Longitude: "); Serial.println(gps.location.lng(), 6);
  Serial.print("Velocidade: "); Serial.print(gps.speed.kmph()); Serial.println(" km/h");
  Serial.print("Altitude: "); Serial.print(gps.altitude.meters()); Serial.println(" m");
  Serial.print("Satelites: "); Serial.println(gps.satellites.value());
  Serial.print("Data/Hora: "); Serial.println(formatarTimestamp());
  Serial.println("=================================");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida [");
  Serial.print(topic);
  Serial.print("]: ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}
