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

// Crear objeto de la pantalla OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Configuración de red WiFi
const char* ssid = "Nexxt";
const char* password = "vacaloca";

// Configuración de MQTT
const char* mqtt_server = "3.233.176.250";
const char* mqtt_user = "esp32";
const char* mqtt_password = "";

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
    } else if (messageTemp == "OFF") {
      digitalWrite(relay1Pin, HIGH); // HIGH apaga el relé en algunos módulos
      client.publish(relay1_status_topic, "OFF");
    }
  }

  // Controlar el Relé 2
  if (String(topic) == relay2_control_topic) {
    if (messageTemp == "ON") {
      digitalWrite(relay2Pin, LOW); // LOW enciende el relé en algunos módulos
      client.publish(relay2_status_topic, "ON");
      delay (1000);
      digitalWrite(relay2Pin, HIGH); // HIGH apaga el relé en algunos módulos
      client.publish(relay2_status_topic, "OFF");
    
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
  if (now - lastMsg > 2000) {
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

    display.display();
  }
}
