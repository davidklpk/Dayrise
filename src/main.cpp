#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "images/imagedata.h"
#include <stdlib.h>
#include <HardwareSerial.h>
#include "TickTwo.h"

// UART2 für serielle Kommunikation (dieser Code ist für den Slave)
HardwareSerial SerialPort(2); 

// Der Schwarz-Weiß-Bildspeicher
UBYTE *BlackImage;

// Die Daten, die vom Master gesendet werden
String msg;
String controlBit;
String secondParam;
String thirdParam;

/**
 * @brief Vollständiges Refreshen des Displays.
 * Das Display wird vollständig refresht. Dadurch flackert das Display kurz auf.
 * Kann daher nicht bei schnellen Änderungen auf dem Display verwendet werden.
 */
void fullRefresh() {
    //printf("Display wird vollständig refresht.\r\n");
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
    //printf("Display wird schnell refresht.\r\n");
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
 * @brief Zeigt Indikator und verbleibende Zeit auf Display an, ob ein Alarm aktiv ist oder nicht.
 * Der Indikator ist ein ausgefüllter Kreis (aktiv) oder nur die Kontur eines Kreises (nicht aktiv).
 * 
 * @param isActive true = Alarm aktiv, false = Alarm nicht aktiv
 */
void setActiveAlarm(boolean isActive, String timeRemaining) {
    if(isActive) {
        Paint_DrawCircle(35, 22, 4, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        Paint_DrawString_EN(48, 14, timeRemaining.c_str(), &FontRoboto13, WHITE, BLACK);
    } else {
        Paint_DrawCircle(35, 22, 4, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawString_EN(48, 14, "No alarm", &FontRoboto13, WHITE, BLACK);
    }
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
 * @brief Gitb den Parameter an der angegebenen Stelle zurück von den 
 * Daten die mit "|" getrennt sind.
 * 
 * @param data Der gesendete String 
 * @param index An welcher Stelle nach dem | soll der Parameter zurückgegeben werden
 * @return String 
 */
String getParam(String data, int index)
{
    char separator = '|';
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

/**
 * @brief Zeigt die aktuelle Uhrzeit auf dem Display an.
 * 
 * @param currentTime Die gesendet Uhrzeit vom Master
 * @param alarmTime Die gesendete Alarmzeit vom Master
 */
void printTimeScreen(String currentTime, String alarmTime) {

    Serial.println("printTime with time: " + currentTime + " and alarmTime: " + alarmTime);

    if(alarmTime != "-") {
        setActiveAlarm(true, alarmTime);
    } else {
        setActiveAlarm(false, alarmTime);
    }
    Paint_DrawString_EN(35, getHorizontalCenter(80), currentTime.c_str(), &FontRoboto72, WHITE, BLACK);
}

/**
 * @brief Zeigt die Weckzeiteinstellug auf dem Display an.
 * 
 * @param alarmTime Die gesendete Alarmzeit vom Master
 * @param alarmState Der gesendete Alarmstatus vom Master (0 = in Bearbeitung, 1 = abgeschlossen)
 */
void printAlarmScreen(String alarmTime, boolean alarmState) {

    Serial.println("printAlarm with time: " + alarmTime + " and state: " + alarmState);

    if(alarmState) {
        Serial.println("AlarmState gesetzt");   
        Paint_DrawString_EN(35, 22, "Set alarm", &FontRoboto13, WHITE, BLACK);
        Paint_DrawString_EN(35, getHorizontalCenter(80), alarmTime.c_str(), &FontRoboto72, WHITE, BLACK);
    // } else if(!alarmState) {
    //     String time = "Successfully set at " + alarmTime;
    //     Serial.println("STATE 1");
    //     Paint_DrawString_EN(35, getHorizontalCenter(80), time.c_str(), &FontRoboto13, WHITE, BLACK);
    //     delay(1000);
    }
}

/**
 * @brief Erhält Daten vom Master, die mit "|" getrennt sind. Als erstes kommt der Control-Bit, dann die Daten.
 * 
 * 0 = Aktuelle Uhrzeit (Format: 0|hh:mm|?hh:mm)
 * 1 = Weckzeit einstellen (Format: 1|hh:mm|0||1)  
 */
void receiveControlBits()
{
    if (SerialPort.available()) {
        msg = SerialPort.readStringUntil('\n');


        msg.remove(msg.length()-1, 1);
        controlBit = getParam(msg, 0);
        secondParam = getParam(msg, 1);
        thirdParam = getParam(msg, 2);

        Serial.print("AVAILBABLE Received: " + msg);
    }
    
    if (controlBit == "0") {
        printTimeScreen(secondParam, thirdParam);
    }
    else if (controlBit == "1") {
        printAlarmScreen(secondParam, true);
    }
}

void setup() {
    // Wifi connection
    Serial.begin(115200);
    SerialPort.begin(115200, SERIAL_8N1, 16, 17);
    //tickerObject.start(); 

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
}

void loop()
{   
    // Hier wird das neue Bild erstellt...
    Paint_NewImage(BlackImage, EPD_3IN52_WIDTH, EPD_3IN52_HEIGHT, 270, WHITE);

    // ... mit der Hintergrundfarbe weiß...
    Paint_Clear(WHITE);

    // ... und dem Inhalt von der Funktion receiveControlBits()...
    receiveControlBits();

    // ... und hier wird das Bild dann auf das Display geladen und dargestellt
    EPD_3IN52_display(BlackImage);
   
    // Refresht das Display um auf Änderungen zu reagieren
    quickRefresh();
}