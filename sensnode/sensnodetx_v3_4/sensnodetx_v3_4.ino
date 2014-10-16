/*
SensnodeTX v3.4-dev
Written by Artur Wronowski <artur.wronowski@digi-led.pl>
Works with optiboot too.
Need Arduino 1.0 do compile

TODO:
- pomiar napiecia bateri, skalowanie czasu pomiedzy pomiarami w zaleznosci od panujacego napiecia
- srednia z pomiarow
- wiartomierz //#define ObwAnem 0.25434 // meters
- http://hacking.majenko.co.uk/node/57
*/

// libs for I2C and DS18B20
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
// lisb for SHT21 and BMP085
#include <SHT21.h>
#include <BMP085.h>
// lib for RFM12B from https://github.com/jcw/jeelib
#include <JeeLib.h>
// avr sleep instructions
#include <avr/sleep.h>

/*************************************************************/

static byte NODEID    = 6;
static byte NODEGROUP = 212;

// Input/Output definition
// Analog
#define LDR               2
#define BAT_VOL           3

// Digital
#define MOSFET            7
#define ONEWIRE_DATA      8
#define ACT_LED           9

// Settings
#define MEASURE_PERIOD    300 // how often to measure, in tenths of seconds
#define RETRY_PERIOD      5  // how soon to retry if ACK didn't come in
#define RETRY_ACK         5 // maximum number of times to retry
#define ACK_TIME          10 // number of milliseconds to wait for an ack
#define REPORT_EVERY      6 // report every N measurement cycles
#define SMOOTH            3 // smoothing factor used for running averages
#define RADIO_SYNC_MODE   2

// Used devices or buses
#define LDR               0 // use LDR sensor
#define DS18B20           0 // use 1WIE DS18B20
#define I2C               1 // use i2c bus for BMP085 and SHT21
#define DEBUG             1 // debug mode - serial output
#define LED_ON            1 // use act led for transmission
#define SOLAR             0 // use solar to charge batteries and power sensnode
#define LM35              0 // use analog temperature sensor LM35 / not implementad

  #define rf12_sleep(x)

/************************************************************/

// structure of data
typedef struct {
  byte nodeid;
  int light;
  float humi;
  float temp;
  float pressure;
  byte lobat  :1;
  int battvol;
} Payload;
Payload measure;

ISR(WDT_vect) { Sleepy::watchdogEvent(); }

enum { MEASURE, REPORT, TASKS };

static word schedbuf[TASKS];
Scheduler scheduler (schedbuf, TASKS);

Port p1 (1); // JeeLabs Port P1
Port p2 (2); // JeeLabs Port P2
//Port ldr (3);  // Analog pin 2
//Port batvol (4); // Analog pin 3

byte count = 0;
static byte reportCount;

#if DS18B20
    OneWire oneWire(ONEWIRE_DATA);
    DallasTemperature sensors(&oneWire);
#endif

void setup()
{
    rf12_initialize(NODEID, RF12_433MHZ, NODEGROUP);
    rf12_control(0xC040); // 2.2v low

#if I2C
    Wire.begin();
    BMP085.getCalData();
#endif

#if DEBUG
    Serial.begin(115200);
#endif

#if ONEWIRE
    sensors.begin();
#endif

  rf12_sleep(RF12_SLEEP);

  reportCount = REPORT_EVERY;
  scheduler.timer(MEASURE, 0);
  
//  batvol.digiWrite2(0);
//  ldr.digiWrite2(0);
  
}

// Usage: smoothedAverage(payload.humi, humi, firstTime);

/*static int smoothedAverage(int prev, int next, byte firstTime =0) {
    if (firstTime)
        return next;
    return ((SMOOTH - 1) * prev + next + SMOOTH / 2) / SMOOTH;
}
*/
void loop()
{
  #if DEBUG
     Serial.print('.');
     delay(2);	
  #endif
  
  switch (scheduler.pollWaiting()) {
    
    case MEASURE:
        scheduler.timer(MEASURE, MEASURE_PERIOD);
        doMeasure();

        if (++reportCount >= REPORT_EVERY) {
          reportCount = 0;
          scheduler.timer(REPORT, 0);
        }
        break;
    case REPORT:
        if (DEBUG) {
          transmissionRS();
          doReport();
        }
        else
        {
		if (LED_ON) {
			activityLed(1);
			}
          doReport();
		if (LED_ON) {
			activityLed(0);
			}
        }
        break;
  }
}


static void doReportACK()
{
  for (byte i = 0; i < RETRY_ACK; ++i) {
    rf12_sleep(RF12_WAKEUP);
    while (!rf12_canSend())
      rf12_recvDone();
    rf12_sendStart(RF12_HDR_ACK, &measure, sizeof measure);
    rf12_sendWait(RADIO_SYNC_MODE);
    byte acked = waitForACK();
    rf12_sleep(RF12_SLEEP);
    
    if (acked) {
      #if DEBUG
        Serial.print("ACK! ");
        Serial.println((int) i);
        Serial.println("-------");
        delay(2);
      #endif
      scheduler.timer(MEASURE, MEASURE_PERIOD);
      return;
    }
    Sleepy::loseSomeTime(RETRY_PERIOD * 100);
  }
  scheduler.timer(MEASURE, MEASURE_PERIOD);
  #if DEBUG
    Serial.print("no ACK! ");
    Serial.println("-------");
    delay(2);
  #endif
}

static void doReport()
{
  rf12_sleep(RF12_WAKEUP);
  while (!rf12_canSend())
    rf12_recvDone();
  rf12_sendStart(0, &measure, sizeof measure, RADIO_SYNC_MODE);
  rf12_sleep(RF12_SLEEP);
}

static byte waitForACK() {
  MilliTimer t;
  while (!t.poll(ACK_TIME)) {
    if (rf12_recvDone() && rf12_crc == 0 && rf12_hdr == (RF12_HDR_DST | RF12_HDR_CTL | NODEID))
      return 1;
     set_sleep_mode(SLEEP_MODE_IDLE);
     sleep_mode();
  }
  return 0;
}

static void transmissionRS()
{
  activityLed(1);
  Serial.println(' ');
  Serial.print("LIGHT ");
  Serial.println(measure.light);
  Serial.print("HUMI ");
  Serial.println(measure.humi);
  Serial.print("TEMP ");
  Serial.println(measure.temp);
  Serial.print("PRES ");
  Serial.println(measure.pressure);
  Serial.print("LOBAT " );
  Serial.println(measure.lobat, DEC);
  Serial.print("BATVOL ");
  Serial.println(measure.battvol);
  activityLed(0);
}

static void activityLed (byte on) {
  pinMode(ACT_LED, OUTPUT);
  digitalWrite(ACT_LED, on);
  delay(150);
}


long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
 
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

long readVcc2() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

static void doMeasure() {
  count++;
//  byte firstTime = measure.humi == 0;
  
  measure.nodeid = NODEID;
  measure.lobat = rf12_lowbat();

#if LDR
  if ((count % 2) == 0) {
     measure.light = ldr.anaRead();
  }
#endif
#if I2C
  SHT21.readSensor();
  measure.humi = SHT21.humi;
  measure.temp = SHT21.temp;
  delay(32);
  BMP085.readSensor();
  measure.pressure = (BMP085.press*10) + 16;
#endif

#if DS18B20
  sensors.requestTemperatures();
  measure.temp = sensors.getTempCByIndex(0);
#endif
//  measure.battvol = readVcc();

}

