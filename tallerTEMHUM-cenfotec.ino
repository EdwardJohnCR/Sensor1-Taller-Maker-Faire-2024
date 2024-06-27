#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Definir pines y tipo de sensor
#define DHTPIN 14       // Pin donde está conectado el sensor DHT11
#define DHTTYPE DHT11   // Definir el tipo de sensor DHT11

DHT dht(DHTPIN, DHTTYPE);

// Definir dimensiones de la pantalla OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Pines I2C para ESP32
#define OLED_SDA 21  // Pin SDA
#define OLED_SCL 22  // Pin SCL

// Crear objeto de la pantalla OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Configuración de red WiFi
const char* ssid = "RED WIFI";
const char* password = "PASSWORD";

// Configuración de MQTT
const char* mqtt_server = "IP-SERVER";
const char* mqtt_user = "USER";
const char* mqtt_password = "PASSWORD";

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

// Tópicos MQTT
const char* temp_topic = "sensor1/temp";
const char* hum_topic = "sensor1/hum";
const char* statusLed_topic = "sensor1/statusLed";
const char* ledControl_topic = "sensor1/ledControl";
const char* relay1_status_topic = "sensor1/relay1/status";
const char* relay2_status_topic = "sensor1/relay2/status";
const char* relay1_control_topic = "sensor1/relay1/control";
const char* relay2_control_topic = "sensor1/relay2/control";

// LED y Relés de control
const int ledPin = 2;
const int relay1Pin = 25;
const int relay2Pin = 26;

bool relay1State = false; // Estado del relé 1
bool relay2State = false; // Estado del relé 2

void setup_wifi() {
  delay(10);
  // Conexión al WiFi
  Serial.println();
  Serial.print("Conectando a ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi conectado");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)payload[i];
  }

  Serial.println(messageTemp);

  // Controlar el LED
  if (String(topic) == ledControl_topic) {
    if (messageTemp == "ON") {
      digitalWrite(ledPin, HIGH);
      client.publish(statusLed_topic, "ON");
    } else if (messageTemp == "OFF") {
      digitalWrite(ledPin, LOW);
      client.publish(statusLed_topic, "OFF");
    }
  }

  // Controlar el Relé 1
  if (String(topic) == relay1_control_topic) {
    if (messageTemp == "ON") {
      digitalWrite(relay1Pin, LOW); // LOW enciende el relé en algunos módulos
      client.publish(relay1_status_topic, "ON");
      relay1State = true;
    } else if (messageTemp == "OFF") {
      digitalWrite(relay1Pin, HIGH); // HIGH apaga el relé en algunos módulos
      client.publish(relay1_status_topic, "OFF");
      relay1State = false;
    }
  }

  // Controlar el Relé 2
  if (String(topic) == relay2_control_topic) {
    if (messageTemp == "ON") {
      digitalWrite(relay2Pin, LOW); // LOW enciende el relé en algunos módulos
      client.publish(relay2_status_topic, "ON");
      relay2State = true;
      delay (3000);
      digitalWrite(relay2Pin, HIGH); // HIGH apaga el relé en algunos módulos
      client.publish(relay2_status_topic, "OFF");
      relay2State = false;
    }
  }
}

void reconnect() {
  // Bucle hasta que se reconecte
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    // Intentar conectar
    if (client.connect("ESP32Client", mqtt_user, mqtt_password)) {
      Serial.println("conectado");
      // Suscribirse a los tópicos
      client.subscribe(ledControl_topic);
      client.subscribe(relay1_control_topic);
      client.subscribe(relay2_control_topic);
    } else {
      Serial.print("falló, rc=");
      Serial.print(client.state());
      Serial.println(" intentar de nuevo en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  // Iniciar comunicación serial
  Serial.begin(115200);
  
  // Configuración de pines
  pinMode(ledPin, OUTPUT);
  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(relay1Pin, HIGH); // Relés normalmente apagados
  digitalWrite(relay2Pin, HIGH); // Relés normalmente apagados

  // Iniciar el sensor DHT
  dht.begin();
  
  // Configurar los pines I2C
  Wire.begin(OLED_SDA, OLED_SCL);

  // Iniciar la pantalla OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Fallo en la asignación de SSD1306"));
    for(;;);
  }
  
  // Limpiar el buffer de la pantalla
  display.clearDisplay();
  
  // Mostrar mensaje de inicio en la pantalla
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("DHT11 Sensor"));
  display.display();
  delay(2000);  // Pausa de 2 segundos

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    // Leer la temperatura y humedad del sensor
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    // Verificar si alguna lectura falló e imprimir error
    if (isnan(h) || isnan(t)) {
      Serial.println(F("Error en la lectura del sensor DHT11!"));
      display.setCursor(0, 0);
      display.println(F("Error en sensor!"));
      display.display();
      return;
    }

    // Publicar los valores en los tópicos MQTT
    snprintf (msg, 50, "%f", t);
    client.publish(temp_topic, msg, false);
    snprintf (msg, 50, "%f", h);
    client.publish(hum_topic, msg, false);

    // Imprimir los valores en la consola serial
    Serial.print(F("Humedad: "));
    Serial.print(h);
    Serial.print(F("%  Temperatura: "));
    Serial.print(t);
    Serial.println(F("°C"));

    // Mostrar los valores en la pantalla OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("Humedad: "));
    display.print(h);
    display.println(F("%"));

    display.setCursor(0, 20);
    display.print(F("Temp: "));
    display.print(t);
    display.println(F(" C"));

    // Mostrar estado de relé 1 (Bombillo encendido/apagado)
    display.setCursor(0, 40);
    if (relay1State) {
      display.println(F("Relay 1: ON"));
      display.fillCircle(100, 50, 5, SSD1306_WHITE); // Bombillo encendido
    } else {
      display.println(F("Relay 1: OFF"));
      display.drawCircle(100, 50, 5, SSD1306_WHITE); // Bombillo apagado
    }

    // Mostrar estado de relé 2 (Figura de alerta)
    display.setCursor(0, 50);
    if (relay2State) {
      display.println(F("Relay 2: ON"));
      display.fillTriangle(110, 50, 115, 40, 120, 50, SSD1306_WHITE); // Triángulo de alerta
    } else {
      display.println(F("Relay 2: OFF"));
      display.drawTriangle(110, 50, 115, 40, 120, 50, SSD1306_WHITE); // Triángulo de alerta vacío
    }

    display.display();
  }
}
