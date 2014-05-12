/*
 * TODO : License and cie
 */
#include <avr/wdt.h>
#include "nRF24L01.h"
#include "RF24.h"

#define VCC _BV(MUX3) | _BV(MUX2) | _BV(MUX1)
#define EXTERNAL 0x01

// Time (ms) to allow the filters to settle before sending data
const int FILTER_SETTLE_TIME = 5000;

// Enable debugging on serial ?
const int DEBUG = 1;

// Speed for the serial comm
const int SERIAL_SPEED = 9600;

// Speed for the nrf module
// RF24_250KBPS / RF24_1MBPS / RF24_2MBPS
// Reduce it to improve reliability
const int NRF_SPEED = RF24_1MBPS;

// PreAmplifier level for the nRF
// Lower this to reduce power consumption. This will reduce range.
const int NRF_PA_LEVEL = RF24_PA_MAX;

// Channel for the nrf module
// 76 is default safe channel in RF24
const int NRF_CHANNEL = 76;

// Set this to 0 to enable voltage measurement.
// Else, set this to your mean voltage.
const int VOLTAGE = 240;

// Number of samples over which the mean must be done for the current measurement
const int NUMBER_SAMPLES_I = 1480;

// Pin for current measurement
const int CURRENT_PIN = A0;

// Pin for voltage measurement
const int VOLTAGE_PIN = 0;

// Calibration factor for the intensity
const double ICAL = 1;

// Radio pipe addresses for the 2 nodes to communicate.
// TODO : Change address
const uint64_t pipe = 0xF0F0F0F0E1LL;


// Struct to send RF data
typedef struct {
    int intensity;
    int voltage;
    int battery;
} PayloadTX;


// Next measurement to be sent
PayloadTX nrf = {0, 0, 0};

// Set up nrf24L01 radio
RF24 radio(9,10)

// Wether we are ready to send the measure or not.
// Store it because millis reinitialize after 50 days.
int settled = 0;


// Get the battery voltage
// source is either VCC (battery) or EXTERNAL (external voltage) or a value for the MUX.
// See http://openenergymonitor.blogspot.it/2012/08/low-level-adc-control-admux.html
int readV(int mux) {
    // Use internal reference. MUX = 1110 is internal 1.1V
    ADMUX = _BV(REFS0) | mux;

    delay(2); // Wait for Vref to settle
    ADCSRA |= _BV(ADSC); // Convert
    while (bit_is_set(ADCSRA,ADSC));
    result = ADCL;
    result |= ADCH << 8;
    result = 1126400L / result; // 1100mV / 1024 ADC steps http://openenergymonitor.org/emon/node/1186
    return result;
}

// Get the measured intensity (root mean squared)
// samples_number is the number of samples for the measurement
// http://openenergymonitor.org/emon/buildingblocks/digital-filters-for-offset-removal
// TODO
int readI(int samples_number) {
    int sample_I = 0;
    int last_sample_I = 0;
    int filtered_I = 0;
    int last_filtered_I = 0;
    int sum_I = 0;

    for (int n = 0; n < samples_number; n++) {
        last_sample_I = sample_I;
        sample_I = analogRead(CURRENT_PIN);
        last_filtered_I = filtered_I;
        filtered_I = 0.996 * (last_filtered_I + sample_I-last_sample_I);

        sumI += filtered_I * filtered_I;
    }

    return ICAL * sqrt(sum_I / samples_number);
}

// Send the data through nRF
// Auto ACK is enabled so if ok is true, transmission was successful 
int sendRfData() {
    bool ok = radio.write(&nrf, sizeof(PayloadTX));

    if(DEBUG) {
        if(ok) {
            Serial.println("Data sent through nRF.")
        } else {
            Serial.println("Data sending failed…");
        }
    }

    // If unable to send data, return -1
    return (int) ok;
}

// Go to suspend for param seconds
// http://www.disk91.com/2014/technology/hardware/arduino-atmega328p-low-power-consumption/
void suspend(int seconds) {
    // TODO
    // + disable ADC and co
//    Sleepy::loseSomeTime(seconds * 1000);
}

void setup() {
    if(DEBUG) {
        Serial.begin(SERIAL_SPEED);
        Serial.println("CitizenWatt sensor.");
        Serial.println("citizenwatt.paris");
        Serial.println("Configuration:");
        Serial.println("===============");
        Serial.print("nRf speed: ");
        if(NRF_SPEED == RF24_250KBPS) Serial.print("250 kbp/s");
        if(NRF_SPEED == RF24_1MBPS) Serial.print("1 mbp/s");
        if(NRF_SPEED == RF24_2MBPS) Serial.print("2 mbp/s");
        Serial.print("nRf PA level: ");
        if(NRF_PA_LEVEL == RF24_PA_MIN) Serial.print("-18dBm");
        if(NRF_PA_LEVEL == RF24_PA_LOW) Serial.print("-12dBm");
        if(NRF_PA_LEVEL == RF24_HIGH) Serial.print("-6dBm");
        if(NRF_PA_LEVEL == RF24_MAX) Serial.print("0dBm");
        Serial.println("===============");
        Serial.println("Starting measuring.");
        Serial.println("I\tV\tBattery");
    }

    // Initialize nRF
    radio.begin();
    radio.setChannel(NRF_CHANNEL);
    // Max number of retries and max delay between them
    radio.setRetries(15, 15);
    // Reduce payload size to improve reliability
    radio.setPayloadSize(8);
    // Set the datarate
    radio.setDataRate(NRF_SPEED);
    // Use the largest CRC
    radio.setCRCLength(RF24_CRC_16);
    // Ensure auto ACK is enabled
    radio.setAutoAck(1);
    // Use the best PA level
    radio.setPALevel(NRF_PA_LEVEL);
    // Open writing pipe
    radio.openWritingPipe(pipe);

    // Enable anti-crash (restart) watchdog
    wdt_enable(WDTO_8S);
}

void loop() {
    // Check that sensor is ready
    if(!settled && millis() > FILTER_SETTLE_TIME) {
        settled = 1;
    }

    if(settled) {
        nrf.intensity = readI(SAMPLES_NUMBER_I);
        if(0 == VOLTAGE) {
            nrf.voltage = readV(EXTERNAL);
        } else {
            nrf.voltage = VOLTAGE;
        }
        nrf.battery = readV(VCC);

        if(DEBUG) {
            Serial.print(nrf.itensity);
            Serial.print(nrf.voltage);
            Serial.print(nrf.battery);
            Serial.println();
        }

        // Send RF data
        send_rf_data();
    }
    // Go to power down mode
    // TODO : suspend(5);
}
