// Function: Detection of heart rate and blood oxygen concentration
// Units used: M5StickCPlus, Heart(MAX30102)
// please install MAX3010x lib for MAX30102 by library manager first
// addr: https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library
#include <Arduino.h>
#include <M5StickC.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

TFT_eSprite Disbuff = TFT_eSprite(&M5.Lcd);
MAX30105 Sensor;

#define SDA_PIN   0
#define SCL_PIN   26
#define MAX_BRIGHTNESS 255
#define bufferLength 100

const byte Button_A = 37;
const byte pulseLED = SCL_PIN;

uint32_t irBuffer[bufferLength] = {0};
uint32_t redBuffer[bufferLength] = {0};

int8_t V_Button, flag_Reset;
int32_t spo2, heartRate, old_spo2;
int8_t validSPO2, validHeartRate;
const byte RATE_SIZE = 5;
uint16_t rate_begin = 0;
uint16_t rates[RATE_SIZE] = {0};
byte rateSpot = 0;
float beatsPerMinute = 0;
int beatAvg = 0;
byte num_fail = 0;

uint32_t red_pos = 0, ir_pos = 0;
uint32_t t1, t2, last_beat, Program_freq;

void PressACallBack();

void PressACallBack()
{
  V_Button = digitalRead(Button_A);
  if (V_Button == 0)
  {
    flag_Reset = 1;
  }
  delay(10);
}

void setup()
{
  // init
  M5.begin();                // initialize M5StickCPlus
  Serial.begin(115200);      // initialize serial communication
  pinMode(25, INPUT_PULLUP); // set pin mode
  pinMode(pulseLED, OUTPUT);
  pinMode(Button_A, INPUT);
  Wire.begin(SDA_PIN, SCL_PIN); // initialize I2C

  // set Lcd
  M5.Lcd.setRotation(3);
  M5.Lcd.setSwapBytes(false);
  Disbuff.createSprite(240, 135);
  Disbuff.setSwapBytes(true);
  Disbuff.createSprite(240, 135);

  // initialize Sensor
  if (!Sensor.begin(Wire, I2C_SPEED_FAST))
  {
    // init fail
    M5.Lcd.print("Init Failed");
    Serial.println(F("MAX30102 was not found. Please check wiring/power."));
    while(1)
    {
      ;
    }
  }
  Serial.println(F("Place your index finger on the Sensor with steady pressure"));

  attachInterrupt(Button_A, PressACallBack, FALLING);
  // set Max30102
  Sensor.setup();
  // Sensor.clearFIFO();
}

void loop()
{
  // put your main code here, to run repeatedly:
  uint16_t ir, red;

  if (flag_Reset)
  {
    Sensor.clearFIFO();
    delay(5);
    flag_Reset = 0;
  }

  while (flag_Reset == 0)
  {
    while (Sensor.available() == false)
    {
      delay(10);
      Sensor.check();
    }

    while (1)
    {
      red = Sensor.getRed();
      ir = Sensor.getIR();

      if ((ir > 1000) && (red > 1000))
      {
        num_fail = 0;
        t1 = millis();
        redBuffer[(red_pos + 100) % 100] = red;
        irBuffer[(ir_pos + 100) % 100] = ir;

        // calculate the heart rate
        t2 = millis();
        Program_freq++;
        if (checkForBeat(ir) == true)
        {
          long delta = millis() - last_beat - (t2 - t1) * (Program_freq - 1);
          last_beat = millis();

          Program_freq = 0;
          beatsPerMinute = 60 / (delta / 1000.0);
          if ((beatsPerMinute > 30) && (beatsPerMinute < 120))
          {
            rate_begin++;
            if ((abs(beatsPerMinute - beatAvg) > 15) && ((beatsPerMinute < 55) || (beatsPerMinute > 95)))
            {
              beatsPerMinute = beatAvg * 0.9 + beatsPerMinute * 0.1;
            }

            if ((abs(beatsPerMinute - beatAvg) > 10) && (beatAvg > 60) && ((beatsPerMinute < 65) || (beatsPerMinute > 90)))
            {
              beatsPerMinute = beatsPerMinute * 0.4 + beatAvg * 0.6;
            }

            rates[rateSpot++] = (byte)beatsPerMinute; // Store this reading in the array
            rateSpot %= RATE_SIZE; // Wrap variable

            // Take average of readings
            beatAvg = 0;
            if ((beatsPerMinute == 0) && (heartRate > 60) && (heartRate < 90))
            {
              beatsPerMinute = heartRate;
            }

            if (rate_begin > RATE_SIZE)
            {
              for (byte x = 0; x < RATE_SIZE; x++)
              {
                beatAvg += rates[x];
              }
              beatAvg /= RATE_SIZE;
            }
            else
            {
              for (byte x = 0; x < rate_begin; x++)
              {
                beatAvg += rates[x];
              }
              beatAvg /= rate_begin;
            }
          }
        }
      }
      else
      {
        num_fail++;
      }

      red_pos++;
      ir_pos++;

      if ((Sensor.check() == false) || flag_Reset)
      {
        break;
      }
    }

    Sensor.clearFIFO();

    delay(5);

    Disbuff.fillRect(0, 0, 240, 135, BLACK);

    old_spo2 = spo2;

    if (red_pos > 100)
    {
      maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
    }


    if (!validSPO2)
    {
      spo2 = old_spo2;
    }

    Disbuff.setCursor(5, 10);

    if (num_fail < 10)
    {
      Disbuff.setTextSize(2);
      Disbuff.setTextColor(GREEN);
      Disbuff.printf("SPO2:%d", spo2);
      Disbuff.setCursor(5, 40);
      Disbuff.printf("BPM:%d", beatAvg);
    }
    else
    {
      Disbuff.setTextSize(2);
      Disbuff.setTextColor(RED);
      Disbuff.printf("No Finger!!");
    }
    Disbuff.pushSprite(0, 0);
  }
}