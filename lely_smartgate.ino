// This #include statement was automatically added by the Particle IDE.

#define ENCODER_OPTIMIZE_INTERRUPTS
#include <Arduino.h>
#include <Encoder.h>
#include <MQTT.h>

SYSTEM_THREAD(ENABLED);

#include "Particle.h"
#include "math.h"
#include "TM1637Display.h"


// Setup display TM1637
// Module connection pins (Digital Pins) // TM1637
#define CLK D1
#define DIO D0

// The amount of time (in milliseconds) between tests // TM1637
#define TEST_DELAY   2000

const uint8_t SEG_DONE[] = { // TM1637
	SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
	SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
	SEG_C | SEG_E | SEG_G,                           // n
	SEG_A | SEG_D | SEG_E | SEG_F | SEG_G            // E
	};

TM1637Display display(CLK, DIO); // TM1637

// This example requires Particle system firmware 0.6.1 or later
// Display 7 segment
//Adafruit_7segment matrix = Adafruit_7segment();

// Rotary encoder
Encoder enc1(D2, D3);

// MQTT
void callback(char* topic, byte* payload, unsigned int length);
MQTT client("IP", PORT, callback);

// Global variabless
bool enc_pushed = false;
int enc_value = 0;
int enc_value_old = 0;
int counter_max = 20;
int counter_value = 0;
int counter_set = 0;
int btn_pushed_n = 0;
bool switch_pushed = false;

String dbg_message = "";

unsigned long enc_changed_ts = 0;
bool enc_changed = false;

bool msg_sent = false;

void setup() {
    pinMode(D2, INPUT_PULLUP);
    pinMode(D3, INPUT_PULLUP);
    pinMode(D5, INPUT_PULLUP);
    
    attachInterrupt(D4, encoder_push, RISING); //RISING, FALLING, CHANGE
    attachInterrupt(D5, switch_activated, FALLING); //RISING, FALLING, CHANGE
    
    Particle.publish("Countdowner gestartet.");
}

uint8_t data[] = { 0x49, 0x49, 0x49, 0x49 };

void loop() {
    enc_value = enc1.read();
    
    display.setBrightness(0x0f);
    
    if(enc_value_old != enc_value){
        
        // Here save timestamp when encoder value changed
        enc_changed_ts = millis();
        enc_changed = true;
        
        /*(counter_value)
        |   x     x     x
        |  x x   x x   x
        | x   x x   x x
        |x_____x_____x__________________(enc_value)
        |      0
        */
        counter_value = counter_max - abs(counter_max - round((abs(enc_value)/4) % 40)); // here mapping function to stay within 0..20
        
        dbg_message = "Encoder: " + String(enc_value) + " " + String(counter_value);
        // Particle.publish(String(enc_value) + " " + String(counter_value));
        enc_value_old = enc_value;
        
        // Show value on display
        display_hour(counter_value);
    }
    
    if(switch_pushed == true){
        btn_pushed_n = btn_pushed_n + 1; // Software entprellen
        dbg_message = dbg_message + "; Button pushed: " + String(btn_pushed_n) + " times";
        switch_pushed = false;
        Particle.publish("Switch-Event");
        Particle.variable("Counter", btn_pushed_n);
    }
    
    // if encoder value has not changed since a while, display btn_pushed_n value again
    if(millis() - enc_changed_ts > 4000){
        //Particle.publish("encoder not changed since 4 seconds");
        enc_changed = false;
        
        display_hour(btn_pushed_n);
    }
  
  
    if((enc_pushed == true) && (millis() - enc_changed_ts < 4000)){
        dbg_message = dbg_message + "; Encoder pushed: " + String(enc_pushed);

        enc1.write(0);
        btn_pushed_n = 0; // reset
        enc_pushed = false;
        msg_sent = false;
        
        counter_set = counter_value;
        
        display_minutes(counter_set);
        Particle.publish("Countdown-Wert gesetzt.");
    }
    
    // if set counter value reached -> send mqtt message
    if((btn_pushed_n == counter_set) && (counter_set != 0) && (msg_sent == false)){
        client.connect("redbear_" + String(Time.now()), "username", "password");
        client.publish("topic","fertig");
        client.publish("topic", String(counter_set));
        client.disconnect();
        Particle.publish("#Events == #Countdown-Wert");
        
        msg_sent = true;
    }
    
    if(dbg_message != ""){
        // Particle.publish(dbg_message);
        dbg_message = "";
    }
    
    delay(1000);
}

// Interrupt function for encoder button pushed event
void encoder_push(){
    enc_pushed = true;
}

// Interrupt function for switch event
void switch_activated(){
    // Here we don't count the detected switch events, but only set the variable to true. 
    // With this we achieve a software debouncing of the switch.
    switch_pushed = true;
    
    //btn_pushed_n = btn_pushed_n + 1;
}

// Display the minutes values (last two digits)
void display_minutes(int value){
    if (value < 10) {
		data[2] = 0x00; // blank
	}
	else {
		data[2] = display.encodeDigit(value / 10);				
	}
	data[3] = display.encodeDigit(value % 10);	
	
	display.setSegments(data);
}

// Display the hour values (first two digits)
void display_hour(int value){
    if (value < 10) {
        data[0] = 0x00; // set blank
	}
	else {
	    data[0] = display.encodeDigit(value / 10);
	}
	data[1] = display.encodeDigit(value % 10) | 0x80;  // mit | 0x80 wird ein Bit gesetzt um den Doppelpunkt anzuzeigen. Funktioniert nur bei 2. Segment
	
	display.setSegments(data);
}

// receive message
void callback(char* topic, byte* payload, unsigned int length) {
    char p[length + 1];
    memcpy(p, payload, length);
    p[length] = NULL;
    
    Particle.publish("Message received");
}
