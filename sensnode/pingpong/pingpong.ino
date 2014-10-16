#include <JeeLib.h>

MilliTimer sendTimer;
char payload[] = "Hello!";
byte needToSend;

static byte ledpin = 9;

static void sendLed (byte on) {
    pinMode(ledpin, OUTPUT);
    digitalWrite(ledpin, on);
}

static void receiveLed (byte on) {
    pinMode(ledpin, OUTPUT);
    digitalWrite(ledpin, !on); // inverse, because LED is tied to VCC
}

void setup () {
    Serial.begin(57600);
    Serial.println(57600);
    Serial.println("Send and Receive");
    rf12_initialize(1, RF12_433MHZ, 33);
}

void loop () {
    if (rf12_recvDone() && rf12_crc == 0) {
        receiveLed(1);
        Serial.print("OK ");
        for (byte i = 0; i < rf12_len; ++i)
            Serial.print(rf12_data[i]);
        Serial.println();
        delay(100); // otherwise led blinking isn't visible
        receiveLed(0);
    }

    if (sendTimer.poll(3000))
        needToSend = 1;

    if (needToSend && rf12_canSend()) {
        needToSend = 0;

        sendLed(1);
        rf12_sendStart(0, payload, sizeof payload);
        delay(100); // otherwise led blinking isn't visible
        sendLed(0);
    }
}