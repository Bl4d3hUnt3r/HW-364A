#include <ESP8266WiFi.h>
#include <espnow.h>
#include <U8G2lib.h>

// Define PinOut
#define OLED_SDA 14                  // D6
#define OLED_SCL 12                  // D5
#define OLED_RESET     U8X8_PIN_NONE // Reset pin
#define STATE_BUTTON_PIN 2           // GPIO 2 (D4)
#define SEND_BUTTON_PIN 13           // GPIO 13 (D7)
#define GREEN_LED_PIN 5              // GPIO 5 (D1)
#define RED_LED_PIN 4                // GPIO 4 (D2)

// Define the state
uint8_t state;

bool ledState = false;  

// OLED display
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, OLED_SCL, OLED_SDA);

// Menu variables
int currentState = 0;               // Current state of the menu
int currentMenuItem = 0;            // Current menu item (initialize to 1)

// Menu items
const char* menuItems[] = {
  "Status",
  "Jetzt nicht",
  "Leise durch",
  "in Ordnung"
};

const int numMenuItems = sizeof(menuItems) / sizeof(menuItems[0]);

// ESPNow variables
uint8_t broadcastAddress[] = {0x98, 0xF4, 0xAB, 0xBC, 0xCE, 0x25};

unsigned long lastFlashTime = 0;
const long flashInterval = 500; // 500ms interval for flashing

void flashLEDs() {

  unsigned long currentTime = millis();

  if (currentTime - lastFlashTime >= flashInterval) {

    lastFlashTime = currentTime;

    ledState = !ledState;  // Toggle the LED state

    digitalWrite(RED_LED_PIN, ledState);

    digitalWrite(GREEN_LED_PIN, !ledState);

  }

}


void renderMenu() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(40, 16);
  u8g2.print(menuItems[0]);

  u8g2.setCursor(40, 32);
  u8g2.print(menuItems[1]);

  u8g2.setCursor(40, 48);
  u8g2.print(menuItems[2]);

  u8g2.setCursor(40, 64);
  u8g2.print(menuItems[3]);

  u8g2.sendBuffer();
}

void handleSendPress() {

  if (currentState == 5) {

    currentState = currentMenuItem + 1;  // Assuming menuItems are 0-indexed

    uint8_t stateToSend = (uint8_t)currentState;

    esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));

    Serial.print("Sent state to A: ");

    Serial.println(currentState);

  }

}

void handleStatePress() {
  if (currentState == 4) {
    currentState = 5;
    uint8_t stateToSend = (uint8_t)currentState;
    esp_now_send(broadcastAddress, &stateToSend, sizeof(uint8_t));
    Serial.println("Transitioned to state 5, sent to device A");
  } else if (currentState == 5) {
    currentMenuItem = (currentMenuItem + 1) % numMenuItems;
  }
  renderMenu();
}

void updateLEDs() {
  switch (currentState) {
    case 0:  // Idle
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
    case 1:  // Waiting
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, HIGH);
      break;
    case 2:  // Active
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
    case 4:  // Call notification
      flashLEDs();
      break;
    case 5:  // User is interacting
    default: // For any other states
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(GREEN_LED_PIN, LOW);
      break;
  }
}



void OnDataRecv(uint8_t* mac, uint8_t* incomingData, uint8_t len) {
  if (len != 1) {
    Serial.println("Invalid message length");
    return;
  }

  uint8_t receivedState = incomingData[0];

  Serial.print("Received message: state = ");
  Serial.println(receivedState);

  // Handle incoming message
  if (receivedState == 4) {  // Call state
    if (currentState == 0) {  // Only react if we're in idle state
      currentState = 4;
      Serial.println("Call received. LEDs flashing.");
    }
  } else {
    Serial.println("Unexpected state received");
  }

  // Update OLED display based on current state
  renderMenu();

  // Update LEDs based on the new state
  updateLEDs();
}

void setup() {
  Serial.begin(74880); // Initialize serial monitor at 74880 baud rate

  pinMode(STATE_BUTTON_PIN, INPUT);
  pinMode(SEND_BUTTON_PIN, INPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);

  u8g2.begin();

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != 0) {
    Serial.println("ESPNow initialization failed");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("Setup complete");
}

void loop() {
  // Read the state of the buttons
  int stateButtonState = digitalRead(STATE_BUTTON_PIN);
  int sendButtonState = digitalRead(SEND_BUTTON_PIN);

  // Handle button presses
  if (stateButtonState == LOW) {
    if (currentState == 4) {
      currentState = 5;  // Transition from "call" state to "interacting" state
    }
    handleStatePress();
  }
  if (sendButtonState == LOW) {
    if (currentState == 4) {
      currentState = 5;  // Transition from "call" state to "interacting" state
    }
    handleSendPress();
  }

  // Update LEDs based on current state
  updateLEDs();

  // Render menu
  renderMenu();

  delay(100); // Small delay to prevent bouncing
}
