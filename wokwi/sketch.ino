// ===== Dispensador Inteligente de Arroz · SIMULADOR WOKWI + Blynk =====
// Cambia SOLO las 2 lineas marcadas con  <-- CAMBIA ESTO
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxxx"     // <-- CAMBIA ESTO
#define BLYNK_TEMPLATE_NAME "Dispensador de Arroz"
#define BLYNK_AUTH_TOKEN    "TU_AUTH_TOKEN"   // <-- CAMBIA ESTO
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <ESP32Servo.h>

// WiFi virtual de Wokwi (no cambiar)
char ssid[] = "Wokwi-GUEST";
char pass[] = "";

// ---- Pines ----
#define TRIG_PIN  5
#define ECHO_PIN  18
#define DHT_PIN   4
#define DHT_TYPE  DHT22
#define SERVO_PIN 13

// ---- Calibración (ver sección 6) ----
const float DIST_VACIO_CM  = 25.0;  // sensor -> fondo del depósito vacío
const float DIST_LLENO_CM  = 3.0;   // sensor -> arroz con depósito lleno
const float GRAMOS_POR_SEG = 20.0;  // gramos que caen por segundo con la compuerta abierta
const int   SERVO_CERRADO  = 0;
const int   SERVO_ABIERTO  = 90;
const float NIVEL_BAJO     = 15.0;  // % para alerta
const float HUMEDAD_ALTA   = 70.0;  // % para alerta

DHT dht(DHT_PIN, DHT_TYPE);
Servo compuerta;
BlynkTimer timer;

int   gramosPorcion = 70;
bool  dispensando   = false;
float servidoHoy    = 0;
bool  avisoNivel = false, avisoHumedad = false;

float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long dur = pulseIn(ECHO_PIN, HIGH, 30000);
  if (dur == 0) return -1;
  return dur * 0.0343 / 2.0;
}

void enviarSensores() {
  // Promedio de 5 lecturas del ultrasónico
  float suma = 0; int n = 0;
  for (int i = 0; i < 5; i++) {
    float d = medirDistancia();
    if (d > 0) { suma += d; n++; }
    delay(30);
  }
  if (n > 0) {
    float d = suma / n;
    float nivel = (DIST_VACIO_CM - d) / (DIST_VACIO_CM - DIST_LLENO_CM) * 100.0;
    nivel = constrain(nivel, 0, 100);
    Blynk.virtualWrite(V2, nivel);
    Blynk.virtualWrite(V6, d);
    if (nivel < NIVEL_BAJO && !avisoNivel) {
      Blynk.logEvent("arroz_bajo", "Queda poco arroz en el dispensador");
      avisoNivel = true;
    }
    if (nivel > NIVEL_BAJO + 10) avisoNivel = false;
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (!isnan(h)) {
    Blynk.virtualWrite(V3, h);
    if (h > HUMEDAD_ALTA && !avisoHumedad) {
      Blynk.logEvent("humedad_alta", "Humedad alta en el dispensador: revisa el arroz");
      avisoHumedad = true;
    }
    if (h < HUMEDAD_ALTA - 5) avisoHumedad = false;
  }
  if (!isnan(t)) Blynk.virtualWrite(V4, t);
}

void cerrarCompuerta() {
  compuerta.write(SERVO_CERRADO);
  dispensando = false;
  servidoHoy += gramosPorcion;
  Blynk.virtualWrite(V5, servidoHoy);
  Blynk.virtualWrite(V1, 0);
}

BLYNK_CONNECTED() { Blynk.syncVirtual(V0, V5); }

BLYNK_WRITE(V0) { gramosPorcion = param.asInt(); }
BLYNK_WRITE(V5) { servidoHoy = param.asFloat(); }

BLYNK_WRITE(V1) {
  if (param.asInt() == 1 && !dispensando && gramosPorcion > 0) {
    dispensando = true;
    unsigned long ms = (unsigned long)(gramosPorcion / GRAMOS_POR_SEG * 1000.0);
    compuerta.write(SERVO_ABIERTO);
    timer.setTimeout(ms, cerrarCompuerta);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  dht.begin();
  compuerta.attach(SERVO_PIN);
  compuerta.write(SERVO_CERRADO);
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(5000L, enviarSensores);
}

void loop() {
  Blynk.run();
  timer.run();
}