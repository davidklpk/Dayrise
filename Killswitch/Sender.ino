// Imports ---------------------------------------
#include <esp_now.h>
#include <WiFi.h> // for ESP32 boards

// Specify the MAC address of the receiver
uint8_t receiverAddress[] = {0x08, 0x3A, 0x8D, 0x2F, 0x1B, 0x68};

// Define the structure for the message
typedef struct message {
    char text[64];
    int intVal;
} message;

// Declare variables
esp_now_peer_info_t peerInfo;
int Taster = 27;
int value = 1;
message myMessage;

// Callback function for message sent event
void messageSent(const uint8_t *macAddr, esp_now_send_status_t status) {
    Serial.print("Send status: ");
    if(status == ESP_NOW_SEND_SUCCESS) {
        Serial.println("Success");
    }
    else {
        Serial.println("Error");
    }
}

// Button press event handler
void buttonpress() {
  value = 0;
  send();
  delay(500);
}

// Send the message to the receiver
void send() {
  char textMsg[] = "Killswitch hat den Wecker ausgeschaltet";
    memcpy(&myMessage.text, textMsg, sizeof(textMsg));
    myMessage.intVal = value;
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myMessage, sizeof(myMessage));
    if (result != ESP_OK) {
        Serial.println("Sending error");
    }
}

void setup() {
    Serial.begin(9600);
    WiFi.mode(WIFI_STA);
    
    // Initialize ESP-NOW
    if (esp_now_init() == ESP_OK) {
        Serial.println("ESPNow Init success");
    }
    else {
        Serial.println("ESPNow Init fail");
        return;
    }
    
    // Register the messageSent callback function
    esp_now_register_send_cb(messageSent);   

    // Set up the peer info for the receiver
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    // Add the receiver as a peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
        Serial.println("Failed to add peer");
        return;
    }

    // Set the button pin as INPUT_PULLUP
    pinMode(27, INPUT_PULLUP);
}
 
void loop() {
  // Check if the button is pressed
  if(digitalRead(Taster) == 0) {
    buttonpress();
  }
}