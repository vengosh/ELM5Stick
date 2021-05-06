#include <M5StickC.h>
#include "ELMduino.h"
#include <BluetoothSerial.h>

BluetoothSerial SerialBT;
#define ELM_PORT   SerialBT
ELM327 myELM327;


uint32_t rpm = 0;
float engineCoolantTemp = 0.000f;
bool elmConnected = false;


void stickPrint(String inpt, bool rawValues = false) {
    M5.Axp.SetLDO2(true);
    M5.Axp.SetLDO3(true);
    M5.Lcd.fillScreen(TFT_BLACK);
    M5.Lcd.setCursor(0, 0, 1);
    M5.Lcd.setTextColor(TFT_YELLOW);
    M5.Lcd.println("ELM DEVICE: " + String(elmConnected ? "CONNECTED" : "DISCONNECTED"));
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.println("RPM: " + String(rpm));
    M5.Lcd.setTextColor(TFT_ORANGE);
    M5.Lcd.println("COOLANT TEMP: " + String(engineCoolantTemp));
    M5.Lcd.println("\r\n");
    M5.Lcd.setTextColor(TFT_MAGENTA);
    if(!rawValues) M5.Lcd.println(inpt);
}

void elmConnect() 
{
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
    if(myELM327.connected)
        elmConnected = true;
    delay(2000);
}

void setup()
{
    M5.begin();
    M5.Lcd.setRotation(3);    
    stickPrint("SETUP COMPLETED, PRESS B TO CONNECT!");
}


void loop()
{
    myELM327.connected ? elmConnected = true : elmConnected = false;
    M5.update();
    if (M5.Axp.GetBtnPress() == 0x02)
    {
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
            stickPrint("ERROR UPDATED!");
        }
    }
    delay(100);
}

