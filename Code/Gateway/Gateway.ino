#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <Notecard.h>

#define PRODUCT_UID "com.xxx.xxxxxxx:xxxxxx"
Notecard notecard;

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
  }

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

void postData(String key, String value){
  int current_time = 0;
  J *rsp = notecard.requestAndResponse(notecard.newRequest("card.time"));
    if (rsp != NULL)
    {
        current_time = JGetNumber(rsp, "time");
        notecard.deleteResponse(rsp);
    }
  String name = "UID123/"+key+".json";
  J *post = NoteNewRequest("web.post");
  JAddStringToObject(post, "route", "ModularIoT");
  JAddStringToObject(post, "name", name.c_str());

  J *body = JCreateObject();
  JAddNumberToObject(body, "value", value.toFloat());
  JAddNumberToObject(body, "timestamp", current_time);
  JAddItemToObject(post, "body", body);
  NoteRequest(post);
}

void putData(String key, String value){
  String name = "UID123/"+key+".json";
  J *put =  notecard.newRequest("web.put");
  JAddStringToObject(put, "route", "ModularIoT");
  JAddStringToObject(put, "name", name.c_str());

  J *body = JCreateObject();
  JAddStringToObject(body, "value", value.c_str());
  JAddItemToObject(put, "body", body);
  notecard.sendRequest(put);
  delay(1000);
}

class MyCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    Serial.print("Received message from BLE Client: ");
    Serial.println(value.c_str());
    String Value = value.c_str();
    //String actuators[] = {"BUTTON1", "BUTTON2"};
    int firstColon = Value.indexOf(':');
    if (firstColon != -1) {
      String key = Value.substring(0, firstColon);
      String value = Value.substring(firstColon + 1);
      Serial.println(value);
      delay(333);
      putData(key,value);
      //delay(1000);
    } 

  }
};


void getData() {
  String myStrings[] = {"BUTTON1", "BUTTON2"};
  int arraySize = sizeof(myStrings) / sizeof(myStrings[0]);

  for (int i = 0; i < arraySize; i++) {
    String name = "UID123/"+myStrings[i]+".json";

    J *get = NoteNewRequest("web.get");
    JAddStringToObject(get, "route", "ModularIoT");
    JAddStringToObject(get, "name", name.c_str());

    J *rsp = notecard.requestAndResponse(get);
    if (rsp != NULL)
    {  
        J* body = JGetObject(rsp, "body");
        int value = JGetNumber(body, "value");
        notecard.deleteResponse(rsp);
        String message = myStrings[i] + ":" + value;
        Serial.println(message);
        pCharacteristic->setValue(message.c_str());
        pCharacteristic->notify();
    }
  }
  //delay(2000);
  
}

void setup() {
  Serial.begin(115200);
  notecard.setDebugOutputStream(Serial);
  notecard.begin();

  /*J *wifi = notecard.newRequest("card.wifi");
  JAddStringToObject(wifi, "ssid", "<SSID>");
  JAddStringToObject(wifi, "password", "<PASSWORD>");
  notecard.sendRequestWithRetry(wifi, 5);
  delay(2000);*/

  J *req = notecard.newRequest("hub.set");
  JAddStringToObject(req, "product", PRODUCT_UID);
  JAddStringToObject(req, "mode", "continuous");
  JAddBoolToObject(req, "sync", true);
  JAddNumberToObject(req, "inbound", 1);
  JAddNumberToObject(req, "outbound", 1);
  notecard.sendRequestWithRetry(req, 5);
  delay(2000);

  // Initialize WiFi
  BLEDevice::init("ESP32");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService* pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
          BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_NOTIFY |
          BLECharacteristic::PROPERTY_INDICATE);

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
  pCharacteristic->setValue("Default");
  pService->start();

  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read and write it from your phone!");

}

void loop() {
  // Handle BLE server
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }
  getData();
  delay(3000);
  
}