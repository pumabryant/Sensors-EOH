#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "cactus_io_DHT22.h"

#define DHT22_PIN 7    // Pin the DHT22's data line is connected to
LiquidCrystal_I2C lcd(0x27,16,2);
DHT22 dht(DHT22_PIN);

/* DHT22 SET UP */
const int SAMPLE_TIME = 500; // The number of milliseconds we are sampling
const int NUM_SAMPLES = 256; // The number of samples we are taking within the 
unsigned long INDEX = 0;    // Used to indicate what index we are storing the current data  at 
float points[NUM_SAMPLES];  // Array of floats to store the recorded temperature data 
float PREV_AVERAGE = 0;     // The previous average of the last NUM_SAMPLE of temperature data
double PREV_DERIVATIVE = 0; // The previous derivative used to determine breathing pattern
int SIGN_CHANGE_COUNT = 0;  // Record the number of times the sign of the derivative changed
int LAST_BREATH_OCCURENCE = 0;

/* SENSOR' VALUE SET-UP*/
int AIR_QUALITY = 0;
int HUMIDITY = 0;
float THERMISTOR_TEMP = 0;
int TEMPERATURE = 0;
float VOLT = 0;

/* BUZZER_VOLT AND THERMISTOR SET UP */
const int BUZZER_VOLT = 9;         //active BUZZER_VOLT at pin 9
const int BUZZER_ERROR = 10;
int THERMISTOR_PIN = 0;       //analog pin "A0" in the voltage divider
int OUTPUT_VOLT;              //variable for output voltage
float R1 = 10000;             //Value of resistor in series with thermistor
float logR2, R2, T, T_C, T_F; //setting up variables
//constants for Steinhart-Hart equation to solve for temperature reading
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07; 

const int VOLTAGE_PIN = 2;
const int AIR_QUALITY_PIN = 1;


/**
 * Set up the LCD and DHT modules
 */
void setup() {
  Serial.begin(9600); 
  pinMode(BUZZER_VOLT, OUTPUT);              
  lcd.begin();
  dht.begin();
}

/**
 * Run the program
 */
void loop() {
  dht.readTemperature();
  dht.readHumidity();

  // Check if the DHT module is reading temperature data correctly
  if (isnan(dht.temperature_F)) {
    Serial.println("DHT sensor read failure!");
  }

  // Once we've collected enough data values, 
  if(INDEX == NUM_SAMPLES){
    INDEX = 0;
    float current_average = calculateAverage(points);
    if(PREV_AVERAGE != 0){
      double derivative = calculateDerivative(PREV_AVERAGE, current_average);
      if(get_sign(derivative) != get_sign(PREV_DERIVATIVE)){
        ++SIGN_CHANGE_COUNT;
        PREV_DERIVATIVE = derivative;
      }
    }
    checkBreath();
    PREV_AVERAGE = current_average;


    AIR_QUALITY =  analogRead(AIR_QUALITY_PIN);
    HUMIDITY = dht.humidity;
    TEMPERATURE = dht.temperature_F;
    get_thermistor_temp();
    print_sensor_values();
    check_voltage();
    displayTempVolt();
  }
  
  // Collect temperature data
  points[INDEX] = dht.temperature_F;
  INDEX++;
  delay(SAMPLE_TIME/NUM_SAMPLES);
}

/**
 * Check whether a breath has occured based on the number of times the derivative changed
 */
void checkBreath(){
  if(SIGN_CHANGE_COUNT % 3 == 0 && SIGN_CHANGE_COUNT != 0 && SIGN_CHANGE_COUNT != LAST_BREATH_OCCURENCE){
    LAST_BREATH_OCCURENCE = SIGN_CHANGE_COUNT;
  } 
}

/**
 * Display the temperature on the LCD screen
 */
void displayTempVolt(){
  lcd.setCursor(0,0);
  lcd.print("Temp: " + String(dht.temperature_F,2) + char(223) + "F");
  lcd.setCursor(0,1);
  lcd.print("Volt: " + String(VOLT,2) + " V");
}

// Display not connected error
void connectionError(){
    lcd.setCursor(0,0); //display on first row of LCD screen
    lcd.print("404 Not Found"); //message for not connected error
    lcd.setCursor(0,1); //display on second row of LCD screen
    lcd.print("");      //nothing to display on second row of LCD screen
}

/**
 * Display a short error on the LCD screen
 */
void shortError(){
    lcd.setCursor(0,0); //display on first row of LCD screen
    lcd.print("Error Short"); //error message for when there is a short
    lcd.setCursor(0,1); //display on second row of LCD screen
    lcd.print("");      //nothing to display on second row of screen
}

/**
 * Calculate the derivative from the breathing pattern 
 * param PREV_AVERAGE
 *: The previous average collected from data points
 * param current_average:  The current average collected from data points
 * return: The derivative of the values 
 */
double calculateDerivative(float previous_avg, float current_avg){
  return (current_avg - previous_avg) / (double(SAMPLE_TIME)/1000);
}

/**
 * Calculate the average value of the temperature values
 * param points: Array of data points
 * return: The average value of the data points
 */
float calculateAverage(float points[]){
  float sum = 0.0;
  for(unsigned int index = 0; index < NUM_SAMPLES; ++index){
    sum += points[index];
  }
  
  return sum / NUM_SAMPLES;
}

/*
 * Retrieve the sign of a number
 * param number: The number to be evaluated
 * return: The sign of the number, positive or negative
 */
int get_sign(double number){
  if(number < 0){
    return -1;
  }
  return 1;
}

/**
 * Get temperature data from thermistor
 **/
void get_thermistor_temp(){
  OUTPUT_VOLT = analogRead(THERMISTOR_PIN);         
  R2 = R1 * (1023.0 / (float)OUTPUT_VOLT - 1.0);      // solving for resistance of thermistor using voltage divider
  logR2 = log(R2);                                    // taking the log of R2 and putting it into a variable to use in Steinhart-Hart equation
  T = (1.0 / (c1 + c2*logR2 + c3*logR2*logR2*logR2)); // application of the Steinhart-Hart equation
  T_C = T - 273.15;                                   // convert from Kelvin to Celsuis
  T_F = (T_C * 9.0)/ 5.0 + 32.0;                      // convert from Celsuis to Farenheit
  check_errors(T_C);                                  // check for any errors
  THERMISTOR_TEMP = T_F;
}

/**
 * Check if any errors have occured from reading data from the thermistor
 * param: Temperature value
 **/
void check_errors(float temperature){
  if (temperature < -40) {                 
    connectionError();
    set_buzz(BUZZER_ERROR);
  } 
  else if(temperature > 40) {
    shortError();
    set_buzz(BUZZER_ERROR);
  }
}

/**
 * Set a buzzing sound to alert user
 **/
void set_buzz(int buzzer){
    tone(buzzer, 1000); // 1KHz sound signal
    delay(1000);        // high for 1000 ms
    noTone(buzzer);     // No sound signal
    delay(1000);        // low for 1000 ms
}

/**
 * Plot and record the sensor values
 **/
void print_sensor_values(){
    Serial.print("Humidity: ");
    Serial.print(HUMIDITY);
    Serial.print(" \t");
    Serial.print("Air Quality: ");
    Serial.print(AIR_QUALITY);
    Serial.print(" *PPM");
    Serial.print(" \t");
    Serial.print("Thermistor: ");
    Serial.print(THERMISTOR_TEMP);
    Serial.print(" \t");
    Serial.print("Temperature: ");
    Serial.println(TEMPERATURE);
}

/**
 * 
 **/
void check_voltage(){
  int i = analogRead(VOLTAGE_PIN);
  VOLT = i * 0.0048828 * 2.9871; //correction factors
  if (VOLT <= 7.5){
    set_buzz(BUZZER_VOLT);
  }
}
