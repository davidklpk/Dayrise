#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "imagedata.h"
#include <stdlib.h>
#include <HardwareSerial.h>

// UART2 für serielle Kommunikation (das ist der Slave)
HardwareSerial SerialPort(2); 

// Der Schwarz-Weiß-Bildspeicher
UBYTE *BlackImage;

/**
 * @brief Vollständiges Refreshen des Displays.
 * Das Display wird vollständig refresht. Dadurch flackert das Display kurz auf.
 * Kann daher nicht bei schnellen Änderungen auf dem Display verwendet werden.
 */
void fullRefresh() {
    printf("Display wird vollständig refresht. Flackern möglich.\r\n");
    //EPD_3IN52_display_NUM(EPD_3IN52_WHITE);
    EPD_3IN52_lut_GC();
    EPD_3IN52_refresh();
}

/**
 * @brief Display wird schnell und nur selektiv refresht.
 * Das Display wird schnell und nur selektiv refresht. Dadurch flackert das Display nicht.
 * Kann daher bei schnellen Änderungen auf dem Display verwendet werden.
 */
void quickRefresh() {
    printf("Display wird schnell refresht. Flackern ausgeschlossen.\r\n");
    EPD_3IN52_lut_DU();
    EPD_3IN52_refresh();
}

/**
 * @brief Zentriert den Text horizontal.
 * 
 * @param fontSize in Pixel
 * @return int y-Position in Pixel
 */
int getHorizontalCenter(int fontSize) {
    return (EPD_3IN52_WIDTH/2)-(fontSize/2);
}

/**
 * @brief Get the Vertical Center object
 * 
 * @param fontHeight Die Höhe des Fonts (siehe in /fonts Ordner, dort ist diese Konfiguriert)
 * @param letters Anzahl der Biuchstaben
 * @return int 
 */
int getVerticalCenter(int fontHeight, int letters) {
    return (EPD_3IN52_WIDTH/2)-((fontHeight*letters)/2);
}

void displaySplashScreen() {
    Paint_NewImage(BlackImage, EPD_3IN52_WIDTH, EPD_3IN52_HEIGHT, 270, WHITE);
    Paint_Clear(WHITE);
    Paint_DrawBitMap(dayrise_splashscreen);
    EPD_3IN52_display(BlackImage);
    fullRefresh();
    delay(2000);
    fullRefresh();
}

/**
 * @brief Zeigt Indikator auf Display an, ob ein Alarm aktiv ist oder nicht.
 * 
 * @param isActive true = Alarm aktiv, false = Alarm nicht aktiv
 */
void setActiveAlarm(boolean isActive) {
    if(isActive) {
        Paint_DrawCircle(35, 22, 4, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        //Paint_DrawImage(indicator_no_alarm, 20, 20, 25, 25);                  // TODO: Icon einfügen funktioniert nicht so ganz idk warum
        Paint_DrawString_EN(48, 14, "in 8:21 hours", &FontRoboto13, WHITE, BLACK);
    } else {
        Paint_DrawCircle(35, 22, 4, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawString_EN(48, 14, "no alarm", &FontRoboto13, WHITE, BLACK);
    }
}

void printAlarmTimeTitle(char *title) {
    Paint_DrawString_EN(35, 22, title, &FontRoboto13, WHITE, BLACK);
}

/**
 * @brief Setzt das Display in den Sleep-Modus.
 * Dabei wird der Inhalt gelöscht und das Display schaltet sich ab.
 * Die Stromversorgung wird auf 0 gesetzt.
 */
void setDisplayToSleep() {
    printf("Clearen des Displays\r\n");
    EPD_3IN52_Clear();
    
    printf("Goto Sleep...\r\n");
    EPD_3IN52_sleep();

    free(BlackImage);
    BlackImage = NULL;
    DEV_Delay_ms(2000); 

    printf("5V wird geschlossen und Stromversorgung auf 0 gesetzt\r\n");
}

/**
 * @brief Get the Value object
 * 
 * https://arduino.stackexchange.com/questions/1013/how-do-i-split-an-incoming-string
 * 
 * @param data 
 * @param separator 
 * @param index 
 * @return String 
 */
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void printTime(String currentTime, String alarmTime) {
    Paint_DrawString_EN(35, getHorizontalCenter(80), currentTime.c_str(), &FontRoboto72, WHITE, BLACK);
    //TODO: Verbleibende Zeit
}


/**
 * @brief Erhält Daten vom Master, die mit "|" getrennt sind. Als erstes kommt der Control-Bit, dann die Daten.
 * 
 * 0 = Aktuelle Uhrzeit (Format: 0|hh:mm|?hh:mm)
 * 1 = Weckzeit einstellen (Format: 1|hh:mm|0||1)  
 * 2 = Error Handling (Format: 2|hh:mm|0||1) 
 * 
 */
void receiveControlBits()
{
    if (SerialPort.available())
    {
        String msg = SerialPort.readString();
        String bit = getValue(msg, '|', 0);

        Serial.print("Received: ");
        Serial.println(bit);

        if (bit == "0")
        {
            printTime(getValue(msg, '|', 1), "00:00");
        }
        else if (bit == "1")
        {
        }
        else if (bit == "2")
        {
        }
        else if (bit == "3")
        {
        }
    }
}

void setup() {
    // Wifi connection
    Serial.begin(115200);
    SerialPort.begin(15200, SERIAL_8N1, 16, 17);

    // ESP32 und EPD werden initialisiert
    DEV_Module_Init();
    EPD_3IN52_Init();
    fullRefresh();

    EPD_3IN52_SendCommand(0x50);    // KEINE Ahnung was die Hexadezimalzahlen für Befehle sein sollen, aber ist wichtig
    EPD_3IN52_SendData(0x17);       // KEINE Ahnung was die Hexadezimalzahlen für Befehle sein sollen, aber ist wichtig

    // Der schwarz-weiß-Bildspeicher wird initialisiert (was genau passiert: keine Ahnung, aber das braucht es)
    UWORD Imagesize = ((EPD_3IN52_WIDTH % 8 == 0)? (EPD_3IN52_WIDTH / 8 ): (EPD_3IN52_WIDTH / 8 + 1)) * EPD_3IN52_HEIGHT;
    if((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) {
        printf("Failed to apply for black memory...\r\n");
    }

    displaySplashScreen();
    //setDisplayToSleep();
}

void loop()
{   
    // Hier wird das neue Bild erstellt...
    Paint_NewImage(BlackImage, EPD_3IN52_WIDTH, EPD_3IN52_HEIGHT, 270, WHITE);
    // ... mit der Hintergrundfarbe weiß...
    Paint_Clear(WHITE);
    // ... und dem Text folgenden Text beschrieben:
    receiveControlBits();
    //!!!!!!!!!!!!!!Paint_DrawString_EN(35, getHorizontalCenter(80), timeHourMinuteSecond, &FontRoboto72, WHITE, BLACK);
    // ... und dem Indikator, je nach dem ob der Alarm aktiviert ist oder nicht
    //setActiveAlarm(true);
    //printAlarmTimeTitle("Set alarm by using the .");
    // und hier wird das Bild dann auf das Display geladen und dargestellt
    EPD_3IN52_display(BlackImage);
   
    quickRefresh();
}