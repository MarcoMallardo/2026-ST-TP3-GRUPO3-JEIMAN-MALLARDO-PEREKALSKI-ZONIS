//Grupo 3: Kenai Jeiman, Marco Mallardo, Ramiro Perekalski, Martín Zonis
#include <Arduino.h>
#define pin_LED_1 25
#define pin_LED_2 26
TaskHandle_t Task1;
TaskHandle_t Task2;

void setup() {
  Serial.begin(115200); 
  pinMode(pin_LED_1, OUTPUT);
  pinMode(pin_LED_2, OUTPUT);

  //create a task that will be executed in the Task1code() function, with priority 1 and executed on core 0
  xTaskCreatePinnedToCore(
                    Task1code,   /* Task function. */
                    "Task1",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task1,      /* Task handle to keep track of created task */
                    0);          /* pin task to core 0 */                  

  //create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
  xTaskCreatePinnedToCore(
                    Task2code,   /* Task function. */
                    "Task2",     /* name of task. */
                    10000,       /* Stack size of task */
                    NULL,        /* parameter of the task */
                    1,           /* priority of the task */
                    &Task2,      /* Task handle to keep track of created task */
                    1);          /* pin task to core 1 */
}

void Task1code( void * pvParameters ){
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    digitalWrite(pin_LED_1, HIGH); Serial.println("1ON");
    delay (10000);
    digitalWrite(pin_LED_1, LOW); Serial.println("1OFF");
    delay (10000);
  } 
}

void Task2code( void * pvParameters ){
  Serial.print("Task2 running on core ");
  Serial.println(xPortGetCoreID());

  for(;;){
    digitalWrite(pin_LED_2, HIGH); Serial.println("2ON");
    delay (500);
    digitalWrite(pin_LED_2, LOW); Serial.println("2OFF");
    delay (500);
  }
}

//La razón por la que el delay de una tarea no bloquea a la otra es porque cada tarea se ejecuta en un núcleo diferente del ESP32. El ESP32 tiene dos núcleos. Cuando una tarea está en espera (por ejemplo, durante un delay), el otro núcleo puede seguir ejecutando la otra tarea sin interrupciones.

void loop() {
  
}