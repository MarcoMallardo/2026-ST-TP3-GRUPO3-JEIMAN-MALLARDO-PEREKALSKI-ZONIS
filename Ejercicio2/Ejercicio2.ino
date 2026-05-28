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
#define BOTtoken "8657893650:AAFE4LEFG1kVS3-XOd7zaRadOI3auWASIjM" // your Bot Token (Get from Botfather)
#define CHAT_ID "1487922548"

TaskHandle_t Task1;
TaskHandle_t Task2;

DHT dht(DHTPIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

typedef enum
{
  RST,
  P1,
  P1AP2,
  P2,
  P2AP1
} estados_t;
estados_t maquinaPantalla;

const char *ssid = "MECA-IoT-V2";
const char *password = "IoT$2026";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

float temperatura = 0;
float valorUmbral = 26;

void setup()
{
  Serial.begin(115200);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  dht.begin();
  u8g2.begin();

  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  bot.sendMessage(CHAT_ID, "Bot started up", "");

  xTaskCreatePinnedToCore(
      Task1code, /* Task function. */
      "Task1",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task1,    /* Task handle to keep track of created task */
      0);        /* pin task to core 0 */

  xTaskCreatePinnedToCore(
      Task2code, /* Task function. */
      "Task2",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task2,    /* Task handle to keep track of created task */
      1);        /* pin task to core 1 */
}

void Task1code(void *pvParameters)
{
  // Bandera para evitar que el ESP haga "spam" de mensajes de alerta cada segundo
  bool alertaEnviada = false;
  long int millisUltimoCheck = millis();

  for (;;)
  {
    // 1. Leer el sensor de temperatura
    if (millis() - millisUltimoCheck >= 5000)
    { // Leer el sensor cada 5 segundos
      float temperaturaTest = dht.readTemperature();
      if (isnan(temperaturaTest))
      {
        Serial.println("Failed to read from DHT sensor!");
      }
      else
      {
        temperatura = temperaturaTest;
      }

      if (temperatura > valorUmbral)
      {
        if (!alertaEnviada)
        { // Si es la primera vez que lo supera, enviamos
          bot.sendMessage(CHAT_ID, "¡Alerta! La temperatura ha superado el umbral: " + String(temperatura) + " °C", "");
          alertaEnviada = true; // Marcamos que ya enviamos la alerta
        }
      }
      else
      {
        // Si la temperatura bajó por debajo de 26, reseteamos la bandera
        alertaEnviada = false;
      }
      millisUltimoCheck = millis();
    }

    // 3. Revisar si hay mensajes nuevos en Telegram
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      for (int i = 0; i < numNewMessages; i++)
      {
        String chat_id = String(bot.messages[i].chat_id);
        String text = bot.messages[i].text;

        // Solo responder si el mensaje viene del CHAT_ID autorizado
        if (chat_id == CHAT_ID)
        {

          // Si nos envían el comando /temperatura
          if (text == "/temperatura")
          {
            bot.sendMessage(chat_id, "La temperatura actual es: " + String(temperatura) + " °C", "");
          }
          // Si nos envían /start (muy común al iniciar el bot)
          else if (text == "/start")
          {
            bot.sendMessage(chat_id, "¡Hola! Envíame /temperatura para consultar el sensor.", "");
          }
        }
      }
      // Volver a revisar por si llegaron más mensajes mientras procesábamos
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    
    // Delay de FreeRTOS para la tarea 1
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void Task2code(void *pvParameters)
{
  // Configuramos el estado inicial de la pantalla
  maquinaPantalla = P1;
  // Estado "suelto" ahora es lógicamente false (0)
  bool sw1_flag = false;
  bool sw2_flag = false;

  // Variables para la secuencia del código de seguridad
  int etapa_secuencia = 0;
  unsigned long tiempo_inicio_codigo = 0;

  for (;;)
  {
    Serial.println(etapa_secuencia);

    // 3. Máquina de estados para el control de pantallas
    switch (maquinaPantalla)
    {

    case P1:
      // Solo actualizamos la flag de SW1, no necesitamos SW2 en este momento
      sw1_flag = !digitalRead(SW1);

      // Dibujar Pantalla 1: Temperatura y Umbral
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Temp: ");
      u8g2.print(temperatura);
      u8g2.setCursor(0, 35);
      u8g2.print("VU: ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Si se PRESIONA SW1, iniciamos la secuencia para intentar ir a P2
      if (sw1_flag)
      {
        sw1_flag = false;                // Reseteamos la bandera para evitar múltiples detecciones
        etapa_secuencia = 1;             // Primer paso completado (Presionar SW1)
        tiempo_inicio_codigo = millis(); // Guardamos el tiempo de inicio
        maquinaPantalla = P1AP2;
      }

      break;

    case P1AP2:
      // Aquí SÍ necesitamos actualizar ambas porque la secuencia requiere los dos botones
      sw1_flag = !digitalRead(SW1);
      sw2_flag = !digitalRead(SW2);

      // Mientras intentamos poner el código, la pantalla sigue mostrando lo mismo que en P1
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Temp: ");
      u8g2.print(temperatura);
      u8g2.setCursor(0, 35);
      u8g2.print("VU: ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Evaluar el Timeout de 5 segundos
      if (millis() - tiempo_inicio_codigo > 5000)
      {
        maquinaPantalla = P1; // Tiempo agotado, volvemos al inicio de P1
        etapa_secuencia = 0;
      }
      else
      {
        // Secuencia del código evaluando SOLO los flags
        if (etapa_secuencia == 1 && !sw1_flag)
        {
          etapa_secuencia = 2;
        } // Esperando soltar SW1
        else if (etapa_secuencia == 2 && sw2_flag)
        {
          etapa_secuencia = 3;
        } // Esperando presionar SW2
        else if (etapa_secuencia == 3 && !sw2_flag)
        {
          etapa_secuencia = 4;
        } // Esperando soltar SW2
        else if (etapa_secuencia == 4 && sw1_flag)
        {
          etapa_secuencia = 5;
        } // Esperando presionar SW1
        else if (etapa_secuencia == 5 && !sw1_flag)
        {
          // ¡Secuencia correcta finalizada a tiempo!
          maquinaPantalla = P2;
          etapa_secuencia = 0;
        }
        // Si el usuario presiona el botón equivocado en la secuencia, se resetea inmediatamente
        else if ((etapa_secuencia == 1 && sw2_flag) ||
                 (etapa_secuencia == 2 && sw1_flag) ||
                 (etapa_secuencia == 3 && sw1_flag) ||
                 (etapa_secuencia == 4 && sw2_flag))
        {
          maquinaPantalla = P1;
          etapa_secuencia = 0;
        }
      }
      break;

    case P2:
      // Necesitamos leer ambos para aumentar/disminuir y para la condición de salida
      sw1_flag = !digitalRead(SW1);
      sw2_flag = !digitalRead(SW2);

      // Dibujar Pantalla 2: Menú para configurar el Valor Umbral
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Seteando VU:");
      u8g2.setCursor(0, 35);
      u8g2.print("VU = ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Aumentar o disminuir verificando que el otro flag no esté activado
      if (sw1_flag && digitalRead(SW1) && !sw2_flag)
      {
        sw1_flag = false; // Reseteamos la bandera para evitar múltiples incrementos
        valorUmbral++;
      }
      if (sw2_flag && digitalRead(SW2) && !sw1_flag)
      {
        sw2_flag = false; // Reseteamos la bandera para evitar múltiples decrementos
        valorUmbral--;
      }

      // Condición de salida: SW1=1 && SW2=1 (ambas banderas verdaderas a la vez)
      if (sw1_flag && sw2_flag)
      {
        sw1_flag = false; // Reseteamos ambas banderas
        sw2_flag = false;
        maquinaPantalla = P1;
      }
      break;
    }
    
    // Delay de FreeRTOS para la tarea 2
    
  }
}

void loop()
{
}#include <Arduino.h>
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
#define BOTtoken "8657893650:AAFE4LEFG1kVS3-XOd7zaRadOI3auWASIjM" // your Bot Token (Get from Botfather)
#define CHAT_ID "1487922548"

TaskHandle_t Task1;
TaskHandle_t Task2;

DHT dht(DHTPIN, DHTTYPE);
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

typedef enum
{
  RST,
  P1,
  P1AP2,
  P2,
  P2AP1
} estados_t;
estados_t maquinaPantalla;

const char *ssid = "MECA-IoT-V2";
const char *password = "IoT$2026";

WiFiClientSecure client;
UniversalTelegramBot bot(BOTtoken, client);

float temperatura = 0;
float valorUmbral = 26;

void setup()
{
  Serial.begin(115200);
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);
  dht.begin();
  u8g2.begin();

  WiFi.begin(ssid, password);
  client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  bot.sendMessage(CHAT_ID, "Bot started up", "");

  xTaskCreatePinnedToCore(
      Task1code, /* Task function. */
      "Task1",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task1,    /* Task handle to keep track of created task */
      0);        /* pin task to core 0 */

  xTaskCreatePinnedToCore(
      Task2code, /* Task function. */
      "Task2",   /* name of task. */
      10000,     /* Stack size of task */
      NULL,      /* parameter of the task */
      1,         /* priority of the task */
      &Task2,    /* Task handle to keep track of created task */
      1);        /* pin task to core 1 */
}

void Task1code(void *pvParameters)
{
  // Bandera para evitar que el ESP haga "spam" de mensajes de alerta cada segundo
  bool alertaEnviada = false;
  long int millisUltimoCheck = millis();

  for (;;)
  {
    // 1. Leer el sensor de temperatura
    if (millis() - millisUltimoCheck >= 5000)
    { // Leer el sensor cada 5 segundos
      float temperaturaTest = dht.readTemperature();
      if (isnan(temperaturaTest))
      {
        Serial.println("Failed to read from DHT sensor!");
      }
      else
      {
        temperatura = temperaturaTest;
      }

      if (temperatura > valorUmbral)
      {
        if (!alertaEnviada)
        { // Si es la primera vez que lo supera, enviamos
          bot.sendMessage(CHAT_ID, "¡Alerta! La temperatura ha superado el umbral: " + String(temperatura) + " °C", "");
          alertaEnviada = true; // Marcamos que ya enviamos la alerta
        }
      }
      else
      {
        // Si la temperatura bajó por debajo de 26, reseteamos la bandera
        alertaEnviada = false;
      }
      millisUltimoCheck = millis();
    }

    // 3. Revisar si hay mensajes nuevos en Telegram
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages)
    {
      for (int i = 0; i < numNewMessages; i++)
      {
        String chat_id = String(bot.messages[i].chat_id);
        String text = bot.messages[i].text;

        // Solo responder si el mensaje viene del CHAT_ID autorizado
        if (chat_id == CHAT_ID)
        {

          // Si nos envían el comando /temperatura
          if (text == "/temperatura")
          {
            bot.sendMessage(chat_id, "La temperatura actual es: " + String(temperatura) + " °C", "");
          }
          // Si nos envían /start (muy común al iniciar el bot)
          else if (text == "/start")
          {
            bot.sendMessage(chat_id, "¡Hola! Envíame /temperatura para consultar el sensor.", "");
          }
        }
      }
      // Volver a revisar por si llegaron más mensajes mientras procesábamos
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    
    // Delay de FreeRTOS para la tarea 1
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void Task2code(void *pvParameters)
{
  // Configuramos el estado inicial de la pantalla
  maquinaPantalla = P1;
  // Estado "suelto" ahora es lógicamente false (0)
  bool sw1_flag = false;
  bool sw2_flag = false;

  // Variables para la secuencia del código de seguridad
  int etapa_secuencia = 0;
  unsigned long tiempo_inicio_codigo = 0;

  for (;;)
  {
    Serial.println(etapa_secuencia);

    // 3. Máquina de estados para el control de pantallas
    switch (maquinaPantalla)
    {

    case P1:
      // Solo actualizamos la flag de SW1, no necesitamos SW2 en este momento
      sw1_flag = !digitalRead(SW1);

      // Dibujar Pantalla 1: Temperatura y Umbral
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Temp: ");
      u8g2.print(temperatura);
      u8g2.setCursor(0, 35);
      u8g2.print("VU: ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Si se PRESIONA SW1, iniciamos la secuencia para intentar ir a P2
      if (sw1_flag)
      {
        sw1_flag = false;                // Reseteamos la bandera para evitar múltiples detecciones
        etapa_secuencia = 1;             // Primer paso completado (Presionar SW1)
        tiempo_inicio_codigo = millis(); // Guardamos el tiempo de inicio
        maquinaPantalla = P1AP2;
      }

      break;

    case P1AP2:
      // Aquí SÍ necesitamos actualizar ambas porque la secuencia requiere los dos botones
      sw1_flag = !digitalRead(SW1);
      sw2_flag = !digitalRead(SW2);

      // Mientras intentamos poner el código, la pantalla sigue mostrando lo mismo que en P1
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Temp: ");
      u8g2.print(temperatura);
      u8g2.setCursor(0, 35);
      u8g2.print("VU: ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Evaluar el Timeout de 5 segundos
      if (millis() - tiempo_inicio_codigo > 5000)
      {
        maquinaPantalla = P1; // Tiempo agotado, volvemos al inicio de P1
        etapa_secuencia = 0;
      }
      else
      {
        // Secuencia del código evaluando SOLO los flags
        if (etapa_secuencia == 1 && !sw1_flag)
        {
          etapa_secuencia = 2;
        } // Esperando soltar SW1
        else if (etapa_secuencia == 2 && sw2_flag)
        {
          etapa_secuencia = 3;
        } // Esperando presionar SW2
        else if (etapa_secuencia == 3 && !sw2_flag)
        {
          etapa_secuencia = 4;
        } // Esperando soltar SW2
        else if (etapa_secuencia == 4 && sw1_flag)
        {
          etapa_secuencia = 5;
        } // Esperando presionar SW1
        else if (etapa_secuencia == 5 && !sw1_flag)
        {
          // ¡Secuencia correcta finalizada a tiempo!
          maquinaPantalla = P2;
          etapa_secuencia = 0;
        }
        // Si el usuario presiona el botón equivocado en la secuencia, se resetea inmediatamente
        else if ((etapa_secuencia == 1 && sw2_flag) ||
                 (etapa_secuencia == 2 && sw1_flag) ||
                 (etapa_secuencia == 3 && sw1_flag) ||
                 (etapa_secuencia == 4 && sw2_flag))
        {
          maquinaPantalla = P1;
          etapa_secuencia = 0;
        }
      }
      break;

    case P2:
      // Necesitamos leer ambos para aumentar/disminuir y para la condición de salida
      sw1_flag = !digitalRead(SW1);
      sw2_flag = !digitalRead(SW2);

      // Dibujar Pantalla 2: Menú para configurar el Valor Umbral
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB08_tr);
      u8g2.setCursor(0, 15);
      u8g2.print("Seteando VU:");
      u8g2.setCursor(0, 35);
      u8g2.print("VU = ");
      u8g2.print(valorUmbral);
      u8g2.sendBuffer();

      // Aumentar o disminuir verificando que el otro flag no esté activado
      if (sw1_flag && digitalRead(SW1) && !sw2_flag)
      {
        sw1_flag = false; // Reseteamos la bandera para evitar múltiples incrementos
        valorUmbral++;
      }
      if (sw2_flag && digitalRead(SW2) && !sw1_flag)
      {
        sw2_flag = false; // Reseteamos la bandera para evitar múltiples decrementos
        valorUmbral--;
      }

      // Condición de salida: SW1=1 && SW2=1 (ambas banderas verdaderas a la vez)
      if (sw1_flag && sw2_flag)
      {
        sw1_flag = false; // Reseteamos ambas banderas
        sw2_flag = false;
        maquinaPantalla = P1;
      }
      break;
    }
    
    // Delay de FreeRTOS para la tarea 2
    
  }
}

void loop()
{
}