#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <U8g2lib.h>

#define DHTPIN 23
#define DHTTYPE DHT11
#define SW1 35
#define SW2 34
#define BOTtoken "8657893650:AAFE4LEFG1kVS3-XOd7zaRadOI3auWASIjM"  // your Bot Token (Get from Botfather)
#define CHAT_ID "1487922548"

TaskHandle_t Task1;
TaskHandle_t Task2;

DHT dht(DHTPIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

typedef enum { RST, P1, P1AP2, P2, P2AP1 } estados_t;
estados_t maquinaPantalla;

const char* ssid = "iPhone de Marco";
const char* password = "abcdefghi";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

float temperatura = 0;
float valorUmbral = 26;

void setup() {
  Serial.begin(115200);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  dht.begin();
  u8g2.begin();

  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");  

  bot.sendMessage(CHAT_ID, "Bot started up", "");

  xTaskCreatePinnedToCore(
    Task1code,   /* Task function. */
    "Task1",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task1,      /* Task handle to keep track of created task */
    0);          /* pin task to core 0 */                  

  xTaskCreatePinnedToCore(
    Task2code,   /* Task function. */
    "Task2",     /* name of task. */
    10000,       /* Stack size of task */
    NULL,        /* parameter of the task */
    1,           /* priority of the task */
    &Task2,      /* Task handle to keep track of created task */
    1);          /* pin task to core 1 */
}

void Task1code( void * pvParameters ) {
  // Bandera para evitar que el ESP haga "spam" de mensajes de alerta cada segundo
  bool alertaEnviada = false; 

  for(;;){
    // 1. Leer el sensor de temperatura
    float temperaturaTest = dht.readTemperature();
    if (isnan(temperaturaTest)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      temperatura = temperaturaTest;
    }

    // 2. Control del Umbral (con prevención de Spam)
    if (temperatura > valorUmbral) {
      if (!alertaEnviada) { // Si es la primera vez que lo supera, enviamos
        bot.sendMessage(CHAT_ID, "¡Alerta! La temperatura ha superado el umbral: " + String(temperatura) + " °C", "");
        alertaEnviada = true; // Marcamos que ya enviamos la alerta
      }
    } else {
      // Si la temperatura bajó por debajo de 26, reseteamos la bandera
      alertaEnviada = false; 
    }

    // 3. Revisar si hay mensajes nuevos en Telegram
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    
    while (numNewMessages) {
      for (int i = 0; i < numNewMessages; i++) {
        String chat_id = String(bot.messages[i].chat_id);
        String text = bot.messages[i].text;

        // Solo responder si el mensaje viene del CHAT_ID autorizado
        if (chat_id == CHAT_ID) {
          
          // Si nos envían el comando /temperatura
          if (text == "/temperatura") {
            bot.sendMessage(chat_id, "La temperatura actual es: " + String(temperatura) + " °C", "");
          } 
          // Si nos envían /start (muy común al iniciar el bot)
          else if (text == "/start") {
            bot.sendMessage(chat_id, "¡Hola! Envíame /temperatura para consultar el sensor.", "");
          }
        }
      }
      // Volver a revisar por si llegaron más mensajes mientras procesábamos
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }

    Serial.println(temperatura);
    delay(1000); 
  }
}

void Task2code( void * pvParameters ) {
  for(;;){
    delay(5000);
    Serial.println("task2");
    delay(5000);
  }
}

void loop() {

}