#include <LowPower.h>
#include <SoftwareSerial.h>
#include "telnum.h"

#define MODEM 1
#define DEBUG 0
#define DEBUG_BAUDRATE 115200
 
void DPRINT(char* s) {
    if (DEBUG)
        Serial.print(s);
}
void DPRINTLN(char *s) {
    if (DEBUG)
        Serial.println(s);
}

void DFLUSH() {
    if (DEBUG)
        Serial.flush();
}

#define MODEM_TX_PIN        10
#define MODEM_RX_PIN        11
#define MODEM_KEY_PIN       12
#define BOARD_LED_PIN       13
#define DOOR_SWITCH_PIN      9

#define REPLY_REGISTERED    "+CREG: 1"

#define MV_PER_ADC_STEP     13 // assuming 4.096 VCC

// actual sleep time is somewhere around 9.047 - 9.05 seconds.
#define TICKS_PER_DAY 	    (9550L) // 9.04712s per tick
#define TICKS_PER_WEEK      (TICKS_PER_DAY * 7)
#define TICKS_PER_MONTH     (TICKS_PER_DAY * 30)

SoftwareSerial* modem;

void sleep(int ms) {
  period_t f;
  if (ms >= 8000) f = SLEEP_8S;
  else if (ms >= 4000) f = SLEEP_4S;
  else if (ms >= 2000) f = SLEEP_2S;
  else if (ms >= 1000) f = SLEEP_1S;
  else if (ms >= 500)  f = SLEEP_500MS;
  else if (ms >= 250)  f = SLEEP_250MS;
  else if (ms >= 120)  f = SLEEP_120MS;
  else if (ms >= 60)   f = SLEEP_60MS;
  else if (ms >= 30)   f = SLEEP_30MS;
  else f = SLEEP_15MS;

  LowPower.powerDown(f, ADC_OFF, BOD_OFF);
}

void blink(int count)
{
  DPRINTLN("Blink");
  for (int i=0; i<count; i++) {
    digitalWrite(BOARD_LED_PIN, HIGH);
    sleep(60);
    digitalWrite(BOARD_LED_PIN, LOW);
    sleep(250);
  }
}

void send(char* s) {
  modem->print(s);
  DPRINT("> ");
  DPRINTLN(s);
}

bool waitfor(char* string, int seconds) {
  char line[40];
  int len = 0;
  unsigned long start = millis();
  unsigned long limit = start + seconds * 1000;
  
  DPRINT("** waitfor: ");
  DPRINTLN(string);

  while (1) {
    while (modem->available()) {
      int c = modem->read();
      if (c == '\r')
        break;
      else
        line[len++] = c;
      delay(10);
    }
    line[len] = 0;

    unsigned long now = millis();
    
    if (now > limit) {
      sprintf(line, "Timeout! %lu > %lu", now, limit);
      DPRINTLN(line);
      return false;
    }
    char* l = line;

    while(*l && isspace(*l)) l++;
    if (*l) {
      DPRINT("< ");
      DPRINTLN(l);
      if (!strncmp(l, string, strlen(string))) {
        DPRINTLN("*** String match");
        return true;
      }
      len = 0;
    }
  }
}
  
void modem_on()
{
  pinMode(MODEM_KEY_PIN, OUTPUT);
  digitalWrite(MODEM_RX_PIN, HIGH);
  pinMode(MODEM_RX_PIN, INPUT);
  modem = new SoftwareSerial(MODEM_RX_PIN, MODEM_TX_PIN);
  modem->begin(9600);
  while (1) {
    DPRINTLN("Power pin pull-down (on)");  
    digitalWrite(MODEM_KEY_PIN, LOW);
    delay(1000);
    DPRINTLN("Power pin release");
    digitalWrite(MODEM_KEY_PIN, HIGH);

    if (!waitfor("RDY", 10)) { blink(2); continue; }
    if (!waitfor("Call Ready", 10)) { blink(3); continue; }
    send("\rAT\r");

    if (!waitfor("OK", 10)) { blink(5); continue; }
    if (!waitfor("+CREG: 1", 30)) { blink(4); continue; }
    break;
  }
}

void modem_pins_low()
{
  pinMode(MODEM_RX_PIN, OUTPUT);
  pinMode(MODEM_TX_PIN, OUTPUT);
  digitalWrite(MODEM_RX_PIN, LOW);
  digitalWrite(MODEM_TX_PIN, LOW);
}    

void modem_off()
{
  while (1) {
    DPRINTLN("Power pin pull-down (off)");
    digitalWrite(MODEM_KEY_PIN, LOW);
    delay(1000);
    DPRINTLN("Power pin release");
    digitalWrite(MODEM_KEY_PIN, HIGH);
    if (!waitfor("NORMAL POWER DOWN", 10))
      DPRINTLN("Power off failed");
    else
      break;
  }
  modem->end();
  delete modem;
  digitalWrite(MODEM_RX_PIN, LOW);
  modem_pins_low();
}

int get_battery()
{
  int v = analogRead(A1);
  int mV = v * MV_PER_ADC_STEP;
  if (DEBUG) {
      char buf[40];
      sprintf(buf, "ADC %d = %d mV", v, mV);
      DPRINTLN(buf);
  }

  return mV;
}

void send_sms(char* string)
{
  modem_on();
  while (1) {
    send("AT+CMGF=1\r");
    if (waitfor("OK", 1))
      break;
  }
  while (1) {
    send("AT+CMGS=\"" TELNUM1 "\"\r");
    waitfor("> ", 1);
    send(string);
    send("\x1A");
    if (waitfor("+CMGS:", 20))
      break;
  }
#ifdef TELNUM2
  while (1) {
    send("AT+CMGS=\"" TELNUM2 "\"\r");
    waitfor("> ", 1);
    send(string);
    send("\x1A");
    if (waitfor("+CMGS:", 20))
      break;
  }
#endif
  modem_off();
}

void send_battery_sms()
{
  char buf[40];
  int mv = get_battery();
  sprintf(buf, "Batteri: %d,%d volt", mv / 1000, mv % 1000 / 100);
  if (DEBUG)
      DPRINTLN(buf);
  else
      send_sms(buf);
}

void setup()
{
  pinMode(BOARD_LED_PIN, OUTPUT);
  pinMode(DOOR_SWITCH_PIN, INPUT);
  modem_pins_low();

  if (DEBUG) {
      Serial.begin(DEBUG_BAUDRATE);
      DPRINTLN("Debug output start");
  }

  if (MODEM) {
      send_battery_sms();
  }
}


void loop()
{
    static long tick = 0;
    static long sent_battery_warning = 0;
    static bool door_sms_sent = false;
    int door = 0;
    digitalWrite(BOARD_LED_PIN, HIGH);

    tick++;
  
    if ((tick % TICKS_PER_WEEK) == 0) {
        if (MODEM)
            send_battery_sms();
    }

    // pullup uses ~125µA, so only connect it while reading pin
    pinMode(DOOR_SWITCH_PIN, INPUT_PULLUP);
    door = digitalRead(DOOR_SWITCH_PIN);
    pinMode(DOOR_SWITCH_PIN, INPUT);

    if (DEBUG) {
        char buf[20];
        sprintf(buf, "Door is %s", door ? "closed" : "open");
        DPRINTLN(buf);
    }
    if (door && MODEM) {
        if (!door_sms_sent) {
            if (DEBUG)
                DPRINTLN(SMS_MESSAGE);
            else
                send_sms(SMS_MESSAGE);
            door_sms_sent = true;
        }
    }
    else {
        door_sms_sent = false;
    }
  
    DFLUSH();
    digitalWrite(BOARD_LED_PIN, LOW);
    sleep(8000);
}




















