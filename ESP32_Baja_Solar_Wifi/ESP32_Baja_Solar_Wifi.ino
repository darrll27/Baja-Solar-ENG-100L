#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>

const char *ssid = "Baja_Solar_PW_00000000";
const char *password = "00000000";

WebServer server(80);

TwoWire I2C_one = TwoWire(0);

// Define the input pin for the flow sensor
const int flowSensorPin = 18;
const int pumpPin = 16;

volatile int pumpStatus = 0;
volatile bool prevButton = false;

// Data wire is plugged into pin 17 and 18 on ESP32
#define ONE_WIRE_BUS_1 15
#define ONE_WIRE_BUS_2 17

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire_1(ONE_WIRE_BUS_1);
OneWire oneWire_2(ONE_WIRE_BUS_2);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors_1(&oneWire_1);
DallasTemperature sensors_2(&oneWire_2);

volatile int flowPulses = 0;   // Keeps track of the number of pulses from the flow sensor
volatile float flowRate;       // Stores the calculated flow rate

QueueHandle_t temperatureQueue; // Queue handler for passing temperature readings between tasks
QueueHandle_t flowRateQueue;   // Queue handler to communicate flow rate values between tasks

// Initialize the OLED display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_one, OLED_RESET);

const int JoyY = 4;
const int JoyBtn = 5;
const int JoyPower = 12;

volatile float settemp = 50;
volatile float targettemp = 50; //30 degrees celsius
volatile float lastTemperature_1 = 0; // variable to store the last known temperature reading from sensor 1
volatile float lastTemperature_2 = 0; // variable to store the last known temperature reading from sensor 2
volatile float lastFlowRate = 0;


void flowRateTask(void *pvParameters) {
  (void) pvParameters;

  pinMode(flowSensorPin, INPUT_PULLUP);  // Set the flow sensor pin as an input and enable the internal pull-up resistor
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowSensorISR, FALLING);  // Attach an interrupt to the flow sensor pin to count pulses

  for (;;) {
    flowPulses = 0;             // Reset the pulse count
    int delay_num = 1000; //wait for 1 second
    vTaskDelay(pdMS_TO_TICKS(delay_num));  // Wait for one second to allow pulses to accumulate
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));  // Disable the interrupt while calculating the flow rate
    float flowRate = (1000/(float)delay_num) * (float)flowPulses / 7.5;  // Calculate the flow rate in liters per minute, assuming a flow sensor with a pulse rate of 7.5 pulses per liter
    xQueueSend(flowRateQueue, &flowRate, portMAX_DELAY);  // Send the flow rate value to the queue
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), flowSensorISR, FALLING);  // Re-enable the interrupt to count pulses
  }
}

void temperatureTask(void *pvParameters) {
  (void) pvParameters;

  for (;;) {
    // Read temperature sensor and send readings to the queue
    sensors_1.requestTemperatures();
    sensors_2.requestTemperatures();
    float temperature_1 = sensors_1.getTempCByIndex(0);
    float temperature_2 = sensors_2.getTempCByIndex(0);
    float temperatureArray[2] = { temperature_1, temperature_2 };
    xQueueSend(temperatureQueue, &temperatureArray, portMAX_DELAY);

    vTaskDelay(pdMS_TO_TICKS(100)); // Wait for 0.1 second
  }
}

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi as an Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  // Get the IP address of the AP
  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apIP);

  // Route for root / page
  server.on("/", HTTP_GET, handleRoot);

  // Start the server
  server.begin();

  pinMode(pumpPin,OUTPUT);
  pinMode(ONE_WIRE_BUS_1, INPUT_PULLUP);
  pinMode(ONE_WIRE_BUS_2, INPUT_PULLUP);
  delay(100);
  sensors_1.begin();
  sensors_2.begin();
  uint8_t address_1[8];
  uint8_t address_2[8];
  sensors_1.getAddress(address_1, 0);
  sensors_2.getAddress(address_2, 0);

  Serial.print("Sensor 1 address: ");
  for (int i = 0; i < 8; i++) {
    Serial.print(address_1[i], HEX);
  }
  Serial.println();

  Serial.print("Sensor 2 address: ");
  for (int i = 0; i < 8; i++) {
    Serial.print(address_2[i], HEX);
  }
  Serial.println();
  delay(100);

  //I2C_one.begin(8,9);
  I2C_one.begin(13,14);
  I2C_one.setClock(400000);
  // OLED display initialization
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  // display.clearDisplay();
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("UC San Diego");
  display.setCursor(0, 10);
  display.print("ENG 100D");

  display.setCursor(0, 30);
  display.print("Baja Solar Project");
  display.setCursor(0, 50);
  display.print("For Neustra Familia");

  display.display();
  delay(1500);


  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("ENG 100D SP 2023");
  display.setCursor(0, 10);
  display.print("Prof: Ahn-Thu Ngo");
  display.setCursor(0, 20);
  display.print("TA: Albert Chang");
  display.setCursor(0, 30);
  display.print("William Harris");
  display.setCursor(0, 40);
  display.print("Madison Ragone");
  display.setCursor(0, 50);
  display.print("Darell Chua");
  display.display();
  delay(1500);

  temperatureQueue = xQueueCreate(1, sizeof(float[2]));
  xTaskCreate(temperatureTask, "TemperatureTask", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);

  flowRateQueue = xQueueCreate(1, sizeof(float));  // Create a queue to hold one float value
  xTaskCreate(flowRateTask, "FlowRateTask", configMINIMAL_STACK_SIZE + 1024, NULL, 1, NULL);

  pinMode(JoyPower,OUTPUT);
  pinMode(JoyY,INPUT_PULLDOWN);
  pinMode(JoyBtn,INPUT_PULLUP);

}

void loop() {

  // Handle client requests
  server.handleClient();

  // Check if new temperature readings are available
  float temperatureArray[2];
  if (xQueueReceive(temperatureQueue, &temperatureArray, 0) == pdTRUE) {
    lastTemperature_1 = temperatureArray[0]; // Update the last known temperature reading from sensor 1
    lastTemperature_2 = temperatureArray[1]; // Update the last known temperature reading from sensor 2
  }

  // Print the last known temperature readings
  Serial.print("Temperature 1: ");
  Serial.print(lastTemperature_1);
  Serial.print(" °C  Temperature 2: ");
  Serial.print(lastTemperature_2);
  Serial.println(" °C");
  float latestFlowRate;
  if (xQueueReceive(flowRateQueue, &latestFlowRate, 0) == pdPASS) {  // Try to read the latest flow rate value from the queue
    lastFlowRate = latestFlowRate;
  }

  Serial.print("Flow rate: ");
  Serial.print(lastFlowRate);
  Serial.println(" L/min");

  // Display the readings on the OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print("Temp 1:");
  display.setCursor(60, 0);
  display.print(lastTemperature_1, 1);
  display.print("C");

  display.setCursor(0, 10);
  display.print("Temp 2:");
  display.setCursor(60, 10);
  display.print(lastTemperature_2, 1);
  display.print("C");


  analogWrite(JoyPower,1023); //turn on power for joystick
  //Serial.println(analogRead(JoyY));
  //Serial.println(digitalRead(JoyBtn));

  if (targettemp > 100){
    targettemp = 100;
  }else if (targettemp < 0){
    targettemp = 0;
  }

  int normJoyY = analogRead(JoyY)-2048;
  //Serial.println(normJoyY);

  if (normJoyY < -1500){
      settemp += 0.1;
    if (normJoyY < -1750){
      settemp += 0.4;
    }
    if (normJoyY < -2000){
      settemp += 1;
    }
  }else if (normJoyY > 1500){
    settemp -= 0.1;
    if (normJoyY > 1750){
      settemp -= 0.4;
    }
    if (normJoyY > 2000){
      settemp -= 1;
    }
  }

  if (!digitalRead(JoyBtn)){
    targettemp = settemp;
  }

  display.setCursor(0, 20);
  display.print("Target:");
  display.setCursor(60, 20);
  display.print(targettemp, 1);
  display.print(" C");

  display.setCursor(0, 30);
  display.print("New temp:");
  display.setCursor(60, 30);
  display.print(settemp, 1);
  display.print(" C");

  Serial.print("Target Temp: ");
  Serial.print(targettemp);
  Serial.println(" C");

  if (lastTemperature_1 < targettemp){
    pumpStatus = 1;
    delay(100);
  }else{
    pumpStatus = 0;
  }

  display.setCursor(0, 40);
  display.print("Pump:");
  display.setCursor(60, 40);

  bool PumpSwitching = false;
  int ButtonRead = !digitalRead(0);
  if ((ButtonRead - prevButton)>0){
    pumpStatus = !pumpStatus;
  }

  if (pumpStatus){
    display.print("on");
    digitalWrite(pumpPin,HIGH);
  }else{
    digitalWrite(pumpPin,LOW);
    display.print("off");
  }

  Serial.print("Pump Status: ");
  Serial.println(pumpStatus);

  display.setCursor(0, 50);
  display.print("Flow:");
  display.setCursor(60, 50);
  display.print(lastFlowRate, 1);
  display.print(" L/min");

  display.display();
  delay(10);
}

void flowSensorISR() {
  flowPulses++;  // Increment the pulse count
}

void handleRoot() {
  // Create an HTML page to display your variables
  String html = "<html><body>";
  html += "<h1>Temperature and Flow Rate</h1>";
  html += "<p>Set Temperature: " + String(settemp) + "</p>";
  html += "<p>Target Temperature: " + String(targettemp) + "</p>";
  html += "<p>Pump Status: " + String(pumpStatus) + "</p>";
  html += "<p>Last Temperature (Sensor 1): " + String(lastTemperature_1) + "</p>";
  html += "<p>Last Temperature (Sensor 2): " + String(lastTemperature_2) + "</p>";
  html += "<p>Last Flow Rate: " + String(lastFlowRate) + "</p>";
  html += "</body></html>";

  // Send the HTML response to the client
  server.send(200, "text/html", html);
}