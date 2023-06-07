#include "DEV_Config.h"
#include "EPD.h"
#include "GUI_Paint.h"
#include "imagedata.h"
#include <stdlib.h>
#include <WiFi.h>
#include "time.h"
#include <TimeLib.h>

// Wifi Credentials
const char* ssid     = "Internet 🚀";
const char* password = "keinkreativesPasswort123";

// NTP Server und Zeit
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
struct tm timeinfo;
char timeHourMinuteSecond[50]; 

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
void isActiveAlarm(boolean isActive) {
    if(isActive) {
        Paint_DrawCircle(20, 20, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_FULL);
        //Paint_DrawImage(indicator_no_alarm, 20, 20, 25, 25);                  // TODO: Icon einfügen funktioniert nicht so ganz idk warum
        Paint_DrawString_EN(31, 14, "in 8:21 hours", &Font12, WHITE, BLACK);
    } else {
        Paint_DrawCircle(20, 20, 3, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        Paint_DrawString_EN(31, 14, "no alarm", &Font12, WHITE, BLACK);
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

void setup() {
    // Wifi connection
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    
    // Initialisierung der Zeit
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

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
    // Zeit wird aktualisiert
    if(!getLocalTime(&timeinfo)){
        Serial.println("Failed to obtain time");
        return;
    }
    
    // mögliches Format: "%A, %B %d %Y %H:%M:%S"
    strftime(timeHourMinuteSecond, sizeof(timeHourMinuteSecond), "%H:%M:%S", &timeinfo);
    String asString(timeHourMinuteSecond);
    
    // Hier wird das neue Bild erstellt...
    Paint_NewImage(BlackImage, EPD_3IN52_WIDTH, EPD_3IN52_HEIGHT, 270, WHITE);
    // ... mit der Hintergrundfarbe weiß...
    Paint_Clear(WHITE);
    // ... und dem Text folgenden Text beschrieben:
    Paint_DrawString_EN(26, getHorizontalCenter(67), timeHourMinuteSecond, &FontRoboto48, WHITE, BLACK);
    // ... und dem Indikator, je nach dem ob der Alarm aktiviert ist oder nicht
    isActiveAlarm(true);
    // und hier wird das Bild dann auf das Display geladen und dargestellt
    EPD_3IN52_display(BlackImage);
   
    quickRefresh();
}
