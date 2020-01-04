

#include <WifiEspNow.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

static const uint8_t rx_addr[] = {0x4E, 0x11, 0xAE, 0x0D, 0xD7, 0x47};
static const uint8_t tx_addr[] = {0x4E, 0x11, 0xAE, 0x0D, 0xD4, 0x52};

enum headlights_status {HEADLIGHTS_OFF=0, HEADLIGHTS_HALF_BEAM, HEADLIGHTS_FULL_BEAM};
enum position_lights_status {POSITION_LIGHTS_OFF=0, POSITION_LIGHTS_ON};
enum led_ramp_status {LED_RAMP_OFF=0, LED_RAMP_HALF_BEAM, LED_RAMP_FULL_BEAM};
enum winch_status {WINCH_OFF=0, WINCH_IN, WINCH_OUT};

union txrx_packet {
  struct {
    headlights_status headlights : 2;
    position_lights_status position_lights: 2;
    led_ramp_status led_ramp: 2;
    winch_status winch: 2;
  };
  uint8_t bytes[1];
};

void printPacket(txrx_packet p) {
  Serial.printf("headlights: %d\n", p.headlights);
  Serial.printf("position lights: %d\n", p.position_lights);
  Serial.printf("led ramp: %d\n", p.led_ramp);
  Serial.printf("winch: %d\n", p.winch);
  Serial.println();

}

void printFrame(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg) {
  digitalWrite(LED_BUILTIN, LOW);
  /*Serial.printf("Message from %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  for (int i = 0; i < (int)count; ++i) {
    Serial.print(static_cast<char>(buf[i]));
  }
  Serial.println();
*/
  txrx_packet p;
  memcpy(p.bytes, buf, 1);
  printPacket(p);
  digitalWrite(LED_BUILTIN, HIGH);
}





/*************************** T X   C O D E *******************************/
#if defined(IS_TX)


volatile static txrx_packet state;
volatile static uint32_t counter = 0;
volatile static bool led_ramp_button_pressed = false;
volatile static bool headlights_button_pressed = false;

static uint32_t last_pressed;

#define LED_RAMP_BUTTON 4   // D2
#define HEADLIGHTS_BUTTON 12// D6
#define WINCH_IN_BUTTON 13  // D7
#define WINCH_OUT_BUTTON 14 // D5

ICACHE_RAM_ATTR void handle_led_ramp_btn(){
  counter = 0;
  led_ramp_button_pressed = true;
}

ICACHE_RAM_ATTR void handle_headlights_btn(){
  counter = 0;
  headlights_button_pressed = true;
}


void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  state.led_ramp = LED_RAMP_OFF;
  state.headlights = HEADLIGHTS_OFF;
  state.winch = WINCH_OFF;

  pinMode(LED_RAMP_BUTTON, INPUT_PULLUP);
  pinMode(HEADLIGHTS_BUTTON, INPUT_PULLUP);
  pinMode(WINCH_IN_BUTTON, INPUT_PULLUP);
  pinMode(WINCH_OUT_BUTTON, INPUT_PULLUP);


  attachInterrupt(digitalPinToInterrupt(LED_RAMP_BUTTON), handle_led_ramp_btn, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HEADLIGHTS_BUTTON), handle_headlights_btn, CHANGE);


  last_pressed = millis();

  Serial.begin(115200);
  Serial.println();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESPNOW", nullptr, 3);
  WiFi.softAPdisconnect(false);

  Serial.print("TX MAC address: ");
  Serial.println(WiFi.softAPmacAddress());

  bool ok = WifiEspNow.begin();
  if (!ok) {
    Serial.println("WifiEspNow.begin() failed");
    ESP.restart();
  }
  WifiEspNow.onReceive(printFrame, nullptr);
  ok = WifiEspNow.addPeer(rx_addr);
  if (!ok) {
    Serial.println("WifiEspNow.addPeer() failed");
    ESP.restart();
  }
}


void loop() {

  txrx_packet state_copy;

  state_copy.led_ramp = state.led_ramp;
  state_copy.winch = state.winch;
  state_copy.headlights = state.headlights;
  state_copy.position_lights = state.position_lights;

  if (led_ramp_button_pressed){
    if (millis() - last_pressed > 250) {
      last_pressed = millis();
      if (state.led_ramp == LED_RAMP_OFF) {
        state.led_ramp = LED_RAMP_HALF_BEAM;
      } else if (state.led_ramp == LED_RAMP_HALF_BEAM) {
        state.led_ramp = LED_RAMP_FULL_BEAM;
      } else {
        state.led_ramp = LED_RAMP_OFF;
      }
    }
    led_ramp_button_pressed = false;
  }

  if (headlights_button_pressed){
    if (millis() - last_pressed > 250) {
      last_pressed = millis();
      if (state.headlights == HEADLIGHTS_OFF) {
        state.headlights = HEADLIGHTS_HALF_BEAM;
      } else if (state.headlights == HEADLIGHTS_HALF_BEAM) {
        state.headlights = HEADLIGHTS_FULL_BEAM;
      } else {
        state.headlights = HEADLIGHTS_OFF;
      }
    }
    headlights_button_pressed = false;
  }

  if (digitalRead(WINCH_IN_BUTTON) == LOW){
      state.winch = WINCH_IN;
  } else if (digitalRead(WINCH_OUT_BUTTON)==LOW){
      state.winch = WINCH_OUT;
  } else {
      state.winch = WINCH_OFF;
  }

  if ( (state_copy.led_ramp != state.led_ramp)
    || (state_copy.winch != state.winch)
    || (state_copy.headlights != state.headlights)
    || (state_copy.position_lights != state.position_lights)) {


    state_copy.led_ramp = state.led_ramp;
    state_copy.winch = state.winch;
    state_copy.headlights = state.headlights;
    state_copy.position_lights = state.position_lights;

      digitalWrite(LED_BUILTIN, LOW);

      WifiEspNow.send(rx_addr, state_copy.bytes, 1);
      Serial.println("TX Sending:");
      printPacket(state_copy);
      Serial.print("ISR Counter: ");
      Serial.println(counter);

      digitalWrite(LED_BUILTIN, HIGH);

  }

}








/*************************** R X  C O D E *******************************/
#elif defined(IS_RX)

#include <Wire.h>
#include <LOLIN_I2C_MOTOR.h>

static LOLIN_I2C_MOTOR winch;

#define HEADLIGHTS_PIN_LEFT 12
#define HEADLIGHTS_PIN_RIGHT 13

volatile static txrx_packet state;

void receiveFrame(const uint8_t mac[6], const uint8_t* buf, size_t count, void* cbarg) {
  digitalWrite(LED_BUILTIN, LOW);


  txrx_packet state_copy;

  memcpy(state_copy.bytes, buf, 1);


  state.led_ramp = state_copy.led_ramp;
  state.winch = state_copy.winch;
  state.headlights = state_copy.headlights;
  state.position_lights = state_copy.position_lights;


  printPacket(state_copy);
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.begin(115200);
  Serial.println();

  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESPNOW", nullptr, 3);
  WiFi.softAPdisconnect(false);

  Serial.print("RX MAC address is:");
  Serial.println(WiFi.softAPmacAddress());

  bool ok = WifiEspNow.begin();
  if (!ok) {
    Serial.println("WifiEspNow.begin() failed");
    ESP.restart();
  }

  WifiEspNow.onReceive(receiveFrame, nullptr);
  ok = WifiEspNow.addPeer(tx_addr);
  if (!ok) {
    Serial.println("WifiEspNow.addPeer() failed");
    ESP.restart();
  }

  state.led_ramp = LED_RAMP_OFF;
  state.winch = WINCH_OFF;
  state.headlights = HEADLIGHTS_OFF;
  state.position_lights = POSITION_LIGHTS_OFF;

  pinMode(HEADLIGHTS_PIN_RIGHT, OUTPUT);
  pinMode(HEADLIGHTS_PIN_LEFT, OUTPUT);
  digitalWrite(HEADLIGHTS_PIN_RIGHT, HIGH);
  digitalWrite(HEADLIGHTS_PIN_LEFT, HIGH);


  while (winch.PRODUCT_ID != PRODUCT_ID_I2C_MOTOR) //wait winch shield ready.
  {
    winch.getInfo();
  }

}


void loop() {

  txrx_packet state_copy;
  state_copy.led_ramp = state.led_ramp;
  state_copy.winch = state.winch;
  state_copy.headlights = state.headlights;
  state_copy.position_lights = state.position_lights;

  //Serial.println("H");
  if (state_copy.headlights == HEADLIGHTS_HALF_BEAM) {
    //Serial.println("HEADLIGHTS_HALF_BEAM");
    analogWrite(HEADLIGHTS_PIN_RIGHT, 500);
    analogWrite(HEADLIGHTS_PIN_LEFT, 500);
  } else if (state_copy.headlights == HEADLIGHTS_FULL_BEAM) {
    //Serial.println("HEADLIGHTS_FULL_BEAM");
    digitalWrite(HEADLIGHTS_PIN_RIGHT, LOW);
    digitalWrite(HEADLIGHTS_PIN_LEFT, LOW);
  } else {
    //Serial.println("HEADLIGHTS_OFF");
    digitalWrite(HEADLIGHTS_PIN_RIGHT, HIGH);
    digitalWrite(HEADLIGHTS_PIN_LEFT, HIGH);
  }

  if (state_copy.led_ramp == LED_RAMP_HALF_BEAM) {
    //Serial.println("LED_RAMP_HALF_BEAM");
    winch.changeDuty(MOTOR_CH_B, 40);
    winch.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CCW);
  } else if (state_copy.led_ramp == LED_RAMP_FULL_BEAM) {
    //Serial.println("LED_RAMP_FULL_BEAM");
    winch.changeDuty(MOTOR_CH_B, 100);
    winch.changeStatus(MOTOR_CH_B, MOTOR_STATUS_CCW);
  } else {
    //Serial.println("LED_RAMP_OFF");
    winch.changeStatus(MOTOR_CH_B, MOTOR_STATUS_STOP);
  }

  if (state_copy.winch == WINCH_IN) {
    //Serial.println("Winch in");
    winch.changeDuty(MOTOR_CH_A, 100);
    winch.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CCW);
  } else if (state_copy.winch == WINCH_OUT) {
    //Serial.println("Winch out");
    winch.changeDuty(MOTOR_CH_A, 100);
    winch.changeStatus(MOTOR_CH_A, MOTOR_STATUS_CW);
  } else {
    //Serial.println("Winch off");
    winch.changeStatus(MOTOR_CH_A, MOTOR_STATUS_SHORT_BRAKE);
  }


}


#endif
