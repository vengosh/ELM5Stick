#include <M5StickC.h>
#include "ELMduino.h"
#include <BluetoothSerial.h>
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include"esp_gap_bt_api.h"
#include "esp_err.h"
#include "FS.h"
#include "SPIFFS.h"
#include <Arduino_JSON.h>

struct Config {
    String REMOVE_BONDED_DEVICES;
    String ELM327_MAC_ADDRESS;
};

Config config;
#define FORMAT_SPIFFS_IF_FAILED true
#define PAIR_MAX_DEVICES 20
uint8_t pairedDeviceBtAddr[PAIR_MAX_DEVICES][6];
char bda_str[18];
BluetoothSerial SerialBT;
#define ELM_PORT SerialBT
ELM327 myELM327;
bool REMOVE_BONDED_DEVICES = false;
uint32_t rpm = 0;
float engineCoolantTemp = 0.000f;
bool elmConnected = false;


bool loadConfiguration(fs::FS& fs, const char* path) {
    File file = fs.open(path);
    if (!file || file.isDirectory()) {
        Serial.println("- failed to open file for reading");
        return false;
    }
    String finalStr = "";
    while (file.available()) {
        finalStr += (char)file.read();
    }
    Serial.println(finalStr);

    JSONVar doc = JSON.parse(finalStr);
    config.REMOVE_BONDED_DEVICES = doc["REMOVE_BONDED_DEVICES"];
    config.ELM327_MAC_ADDRESS = doc["ELM327_MAC_ADDRESS"];
    file.close();
    Serial.println("Config ok");
    return true;
}

void saveConfiguration(fs::FS& fs, const char* path) {
    fs.remove(path);
    File file = fs.open(path, FILE_WRITE);
    if (!file || file.isDirectory()) {
        Serial.println("- failed to open file");
        return;
    }

    JSONVar cfg;
    cfg["REMOVE_BONDED_DEVICES"] = config.REMOVE_BONDED_DEVICES;
    cfg["ELM327_MAC_ADDRESS"] = config.ELM327_MAC_ADDRESS;
    String doc = JSON.stringify(cfg);
    Serial.println(doc);
    if (file.print(doc)) {
        Serial.println("- file written");
    }
    else {
        Serial.println("- write failed");
    }
    file.close();
}

void stickPrint(String inpt, bool rawValues = false) {
    M5.Axp.SetLDO2(true);
    M5.Axp.SetLDO3(true);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0, 1);
    M5.Lcd.println("\r\n");
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.println("ELM DEVICE: " + String(elmConnected ? "CONNECTED" : "DISCONNECTED"));
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.println("RPM: " + String(rpm));
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.println("COOLANT TEMP: " + String(engineCoolantTemp));
    M5.Lcd.println("\r\n");
    M5.Lcd.setTextColor(TFT_MAGENTA);
    if (!rawValues) M5.Lcd.println(inpt);
}


bool initBluetooth()
{
    stickPrint("INIT PROCEDURE");
    delay(1000);
    if (!btStart()) {
        Serial.println("Failed to initialize controller");
        return false;
    }

    if (esp_bluedroid_init() != ESP_OK) {
        Serial.println("Failed to initialize bluedroid");
        return false;
    }

    if (esp_bluedroid_enable() != ESP_OK) {
        Serial.println("Failed to enable bluedroid");
        return false;
    }
    return true;
}

void removeBondedDevices() {
    stickPrint("REMOVING DEVICES");
    delay(1000);
    int count = esp_bt_gap_get_bond_device_num();
    if (!count) {
        Serial.println("No bonded device found.");
    }
    else {
        Serial.print("Bonded device count: "); Serial.println(count);
        if (PAIR_MAX_DEVICES < count) {
            count = PAIR_MAX_DEVICES;
            Serial.print("Reset bonded device count: "); Serial.println(count);
        }
        esp_err_t tError = esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);
        if (ESP_OK == tError) {
            for (int i = 0; i < count; i++) {
                Serial.print("Found bonded device # "); Serial.print(i); Serial.print(" -> ");
                Serial.println(bda2str(pairedDeviceBtAddr[i], bda_str, 18));
                if (REMOVE_BONDED_DEVICES) {
                    esp_err_t tError = esp_bt_gap_remove_bond_device(pairedDeviceBtAddr[i]);
                    if (ESP_OK == tError) {
                        Serial.print("Removed bonded device # ");
                    }
                    else {
                        Serial.print("Failed to remove bonded device # ");
                    }
                    Serial.println(i);
                }
            }
        }
    }
    REMOVE_BONDED_DEVICES = false;
    config.REMOVE_BONDED_DEVICES = "0";
    saveConfiguration(SPIFFS, "/config.cfg");
}

char* bda2str(const uint8_t* bda, char* str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
        bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    return str;
}

void elmConnect() 
{
    stickPrint("CONNECTING...");

    SerialBT.setPin("1234");
    ELM_PORT.begin("ELM5", true);

    if (!ELM_PORT.connect("OBDII"))
    {
        stickPrint("FAIL 1");
        //while (1);
    }

    if (!myELM327.begin(ELM_PORT, true, 2000))
    {
        stickPrint("FAIL 2");
        //while (1);
    }
    if (myELM327.connected) {
        elmConnected = true;
        stickPrint("CONNECTED!!!!");

    }
    delay(2000);
    stickPrint("",true);

}

void setup()
{
    Serial.begin(115200);
    M5.begin();
    M5.Lcd.setRotation(3);
    if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    if (loadConfiguration(SPIFFS, "/config.cfg")) {
        if (config.REMOVE_BONDED_DEVICES == "1")
        {        
            REMOVE_BONDED_DEVICES = true;
            if (initBluetooth()) {
               removeBondedDevices();
            }
        }
        
    }        
    stickPrint("SETUP COMPLETED, \r\n\PRESS B TO CONNECT!");
}

void loop()
{
    M5.update();
    if (elmConnected) {
        myELM327.connected ? elmConnected = true : elmConnected = false;
    }
    
    if (M5.Axp.GetBtnPress() == 0x02)
    {
        config.REMOVE_BONDED_DEVICES = "1";
        config.ELM327_MAC_ADDRESS = "unk";
        saveConfiguration(SPIFFS, "/config.cfg");
        ESP.restart();
    }
    if (M5.BtnA.wasPressed())
    {
        stickPrint(String(myELM327.sendCommand("04")));
        delay(2000);
    }
    if (M5.BtnB.wasPressed())
    {
        if(!elmConnected)
            elmConnect();        
    }

    if (elmConnected)
    {
        float tempRPM = myELM327.rpm();
        float tempCoolantTemp = myELM327.engineCoolantTemp();

        if (myELM327.status == ELM_SUCCESS)
        {
            rpm = (uint32_t)tempRPM;
            engineCoolantTemp = tempCoolantTemp;
            stickPrint("",true);
        }
        else {
            myELM327.printError();
            stickPrint("ERROR!");
        }
    }
    delay(100);
}

