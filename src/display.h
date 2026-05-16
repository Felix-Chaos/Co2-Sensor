#pragma once
#include <TFT_eSPI.h>
#include "config.h"

#define BG          0x0000
#define CARD        0x18E3
#define CARD_HI     0x2945
#define WHITE       0xFFFF
#define GRAY        0xB596
#define DIM         0x7BCF
#define DARK        0x4A49
#define C_BLUE      0x34DF
#define C_GREEN     0x07E0
#define C_YELLOW    0xFE60
#define C_ORANGE    0xFC00
#define C_RED       0xF800
#define C_PURPLE    0x897F
#define C_DKRED     0x8000
#define C_DKORANGE  0x8200
#define C_DKBLUE    0x1148
#define C_DKGREEN   0x03E0
#define SW          240
#define SH          135

// Alert types
enum AlertType { AT_NONE=0, AT_INFO=1, AT_WARN=2, AT_ALARM=3 };
// Alert IDs
enum AlertId { AID_DOOR=0, AID_STAIRS, AID_WIN1_GOOD, AID_WIN1_CLOSE,
               AID_WIN2_GOOD, AID_WIN2_CLOSE, AID_CO2, AID_TEMP, AID_HUM };

class DisplayManager {
public:
    TFT_eSPI tft;
    TFT_eSprite spr;

    float co2=0, temperature=-99, humidity=-1;
    bool stairMotion=false, doorOpen=false;
    bool window1Open=false, window2Open=false;
    bool heating=false;
    bool mqttConnected=false, wifiConnected=false;
    int hour=12, minute=0, weekday=0, day=1, month=0;
    bool nightMode=false, needsRedraw=true;
    int screen=0;
    uint8_t brightness=BRIGHTNESS_DAY;
    int animFrame=0;

    // Alert system
    bool alertActive=false;
    AlertType alertType=AT_NONE;
    AlertId alertId=AID_DOOR;
    char alertTitle[24]="";
    char alertMsg[40]="";

    float pCo2=-1,pTemp=-99,pHum=-1;
    bool pSt=0,pDo=0,pW1=0,pW2=0,pH=0;
    int pScr=-1;

    DisplayManager() : spr(&tft) {}

    void begin() {
        tft.init(); tft.setRotation(1); tft.fillScreen(BG);
        spr.createSprite(SW, SH);
        spr.setTextDatum(TL_DATUM);
        pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
        setBrightness(BRIGHTNESS_DAY);
    }

    void setBrightness(uint8_t v) { brightness=v; analogWrite(TFT_BACKLIGHT_PIN,v); }

    void updateBrightness() {
        bool night=(hour>=NIGHT_START_HOUR||hour<NIGHT_END_HOUR);
        if(night!=nightMode){nightMode=night;setBrightness(night?BRIGHTNESS_NIGHT:BRIGHTNESS_DAY);}
    }

    void nextScreen(){screen=(screen+1)%NUM_SCREENS;needsRedraw=true;}
    void prevScreen(){screen=(screen-1+NUM_SCREENS)%NUM_SCREENS;needsRedraw=true;}

    void setAlert(AlertType t, AlertId id, const char* title, const char* msg) {
        alertActive=true; alertType=t; alertId=id;
        strncpy(alertTitle,title,23); alertTitle[23]=0;
        strncpy(alertMsg,msg,39); alertMsg[39]=0;
        needsRedraw=true;
    }
    void clearAlert() { alertActive=false; alertType=AT_NONE; needsRedraw=true; }

    // ---- Colors ----
    uint16_t co2Color(float p){if(p<=0)return DIM;if(p<CO2_GOOD)return C_GREEN;if(p<CO2_MODERATE)return C_YELLOW;if(p<CO2_POOR)return C_ORANGE;return C_RED;}
    const char* co2Label(float p){if(p<=0)return"---";if(p<CO2_GOOD)return"GOOD";if(p<CO2_MODERATE)return"OK";if(p<CO2_POOR)return"POOR";return"BAD";}
    uint16_t tempColor(float t){if(t<=-40)return DIM;if(t>=TEMP_COMFORT_LOW&&t<=TEMP_COMFORT_HIGH)return C_GREEN;if(t>=TEMP_WARN_LOW&&t<=TEMP_WARN_HIGH)return C_YELLOW;return C_RED;}
    uint16_t humColor(float h){if(h<0)return DIM;if(h>=HUM_COMFORT_LOW&&h<=HUM_COMFORT_HIGH)return C_GREEN;if(h>=HUM_WARN_LOW&&h<=HUM_WARN_HIGH)return C_YELLOW;return C_RED;}

    // ---- Icons ----
    void iconFlame(int x,int y,uint16_t c){
        uint16_t inner=(c==DARK)?CARD:C_YELLOW;
        spr.fillTriangle(x+5,y,x+1,y+10,x+9,y+10,c);spr.fillCircle(x+5,y+9,4,c);
        spr.fillTriangle(x+5,y+4,x+3,y+10,x+7,y+10,inner);spr.fillCircle(x+5,y+9,2,inner);
    }
    void iconDoor(int x,int y,uint16_t c){spr.drawRect(x,y,9,13,c);spr.fillCircle(x+6,y+7,1,c);}
    void iconWindow(int x,int y,uint16_t c){spr.drawRect(x,y,12,11,c);spr.drawFastHLine(x,y+5,12,c);spr.drawFastVLine(x+6,y,11,c);}
    void iconStairs(int x,int y,uint16_t c){for(int i=0;i<4;i++){spr.fillRect(x+i*3,y+i*3,4,2,c);if(i<3)spr.fillRect(x+i*3+3,y+i*3+1,1,3,c);}}
    void iconTherm(int x,int y,uint16_t c){spr.fillCircle(x+3,y+11,3,c);spr.fillRect(x+1,y,4,10,c);spr.fillRect(x+2,y+1,2,6,BG);}
    void iconDrop(int x,int y,uint16_t c){spr.fillCircle(x+4,y+8,4,c);spr.fillTriangle(x+4,y,x,y+8,x+8,y+8,c);}
    void iconCheck(int x,int y,uint16_t c){
        spr.drawLine(x,y+8,x+5,y+14,c);spr.drawLine(x+1,y+8,x+6,y+14,c);
        spr.drawLine(x+5,y+14,x+14,y,c);spr.drawLine(x+6,y+14,x+15,y,c);
    }
    void iconExclaim(int x,int y,uint16_t c){
        spr.fillTriangle(x+8,y,x,y+16,x+16,y+16,c);
        spr.fillTriangle(x+8,y+3,x+3,y+15,x+13,y+15,BG);
        spr.fillRect(x+7,y+6,2,5,c);spr.fillRect(x+7,y+12,2,2,c);
    }

    void drawStatusDot(int x,int y,bool on,uint16_t c){spr.fillCircle(x,y,4,on?c:DARK);}
    void drawDots(int y){int cx=SW/2;for(int i=0;i<NUM_SCREENS;i++){int dx=cx+(i-1)*10;if(i==screen)spr.fillCircle(dx,y,3,WHITE);else spr.drawCircle(dx,y,3,DARK);}}

    // ---- Bottom bar ----
    void drawBottomBar() {
        int y=SH-13;
        spr.drawFastHLine(0,y-2,SW,CARD_HI);
        spr.setTextFont(1);
        spr.setTextColor(wifiConnected?C_GREEN:C_RED,BG);spr.drawString(wifiConnected?"WiFi":"NoWF",3,y);
        spr.setTextColor(mqttConnected?C_GREEN:C_RED,BG);spr.drawString(mqttConnected?"MQTT":"NoMQ",34,y);
        drawDots(y+4);
        char tb[6];snprintf(tb,6,"%02d:%02d",hour,minute);
        spr.setTextColor(DIM,BG);spr.drawString(tb,SW-32,y);
    }

    // ================ ALERT OVERLAY ================
    void drawAlertScreen() {
        uint16_t bgA, bgB, textCol, iconCol;
        const char* typeLabel;

        if (alertType == AT_ALARM) {
            bgA=C_RED; bgB=C_DKRED; textCol=WHITE; iconCol=WHITE; typeLabel="ALARM";
        } else if (alertType == AT_WARN) {
            bgA=C_ORANGE; bgB=C_DKORANGE; textCol=WHITE; iconCol=WHITE; typeLabel="WARNING";
        } else {
            bgA=C_BLUE; bgB=C_DKBLUE; textCol=WHITE; iconCol=WHITE; typeLabel="INFO";
        }

        if (alertType == AT_ALARM) {
            // ---- Lighthouse pulse effect ----
            spr.fillSprite(BG);
            int cx = SW/2, cy = SH/2 - 8;
            // Expanding rings from center
            for (int ring = 0; ring < 4; ring++) {
                int phase = (animFrame * 6 + ring * 30) % 120;
                int r = phase;
                if (r > 0 && r < 120) {
                    // Fade: bright near center, dim at edges
                    uint16_t col = (r < 40) ? C_RED : (r < 70) ? C_DKRED : 0x4000;
                    spr.drawCircle(cx, cy, r, col);
                    spr.drawCircle(cx, cy, r+1, col);
                    if (r < 30) spr.drawCircle(cx, cy, r+2, col);
                }
            }
            // Solid red core
            int coreSize = 8 + (animFrame % 6 < 3 ? 4 : 0); // pulsing
            spr.fillCircle(cx, cy, coreSize, C_RED);
            spr.fillCircle(cx, cy, coreSize - 3, C_DKRED);

            // Type label on top
            spr.setTextColor(WHITE, BG);
            spr.setTextFont(4);
            int tw = spr.textWidth(typeLabel);
            spr.drawString(typeLabel, (SW-tw)/2, 2);

            // Title + message below center
            spr.setTextFont(2); spr.setTextColor(C_RED, BG);
            tw = spr.textWidth(alertTitle);
            spr.drawString(alertTitle, (SW-tw)/2, cy + coreSize + 6);

            spr.setTextFont(1); spr.setTextColor(GRAY, BG);
            tw = spr.textWidth(alertMsg);
            spr.drawString(alertMsg, (SW-tw)/2, cy + coreSize + 24);
        } else {
            // ---- Solid background for WARNING / INFO ----
            spr.fillSprite(bgA);
            uint16_t bg = bgA;

            spr.setTextColor(textCol, bg);
            spr.setTextFont(4);
            int tw = spr.textWidth(typeLabel);
            spr.drawString(typeLabel, (SW-tw)/2, 6);

            int icx=SW/2-20, icy=38;
            switch(alertId) {
                case AID_STAIRS: iconStairs(icx,icy,iconCol); iconStairs(icx+1,icy,iconCol); break;
                case AID_WIN1_GOOD: case AID_WIN2_GOOD: iconCheck(icx,icy,iconCol); break;
                case AID_WIN1_CLOSE: case AID_WIN2_CLOSE: iconWindow(icx,icy,iconCol); iconWindow(icx+1,icy,iconCol); break;
                case AID_CO2: spr.setTextFont(2);spr.setTextColor(textCol,bg);spr.drawString("CO2",icx,icy); break;
                case AID_TEMP: iconTherm(icx,icy,iconCol); iconTherm(icx+1,icy,iconCol); break;
                case AID_HUM: iconDrop(icx,icy,iconCol); iconDrop(icx+1,icy,iconCol); break;
                default: break;
            }

            spr.setTextFont(2); spr.setTextColor(textCol, bg);
            spr.drawString(alertTitle, icx+24, icy+2);

            spr.setTextFont(2); spr.setTextColor(textCol, bg);
            tw = spr.textWidth(alertMsg);
            spr.drawString(alertMsg, (SW-tw)/2, 70);
        }

        // Button hints (Right side, top and bottom buttons)
        spr.setTextFont(1); spr.setTextColor(WHITE); // transparent background
        spr.drawString("Ignore Day >", SW - 80, 20);
        spr.drawString("Ignore Week >", SW - 85, 100);

        // Bottom bar
        spr.fillRect(0, SH-16, SW, 16, BG);
        drawBottomBar();
    }

    // ================ SCREEN 0 — Dashboard ================
    void drawScreen0() {
        spr.fillRoundRect(2,2,130,52,4,CARD);
        spr.setTextFont(1);spr.setTextColor(DIM,CARD);spr.drawString("CO2",8,6);
        uint16_t cc=co2Color(co2);
        spr.setTextColor(cc,CARD);spr.setTextFont(4);
        char buf[8];if(co2>0)snprintf(buf,8,"%d",(int)co2);else snprintf(buf,8,"---");
        spr.drawString(buf,8,18);int tw2=spr.textWidth(buf);
        spr.setTextFont(2);spr.setTextColor(DIM,CARD);spr.drawString("ppm",10+tw2,24);
        spr.setTextColor(cc,CARD);spr.setTextFont(2);
        const char*ql=co2Label(co2);spr.drawString(ql,128-spr.textWidth(ql),6);
        int bx=8,by2=44,bw=120,bh=6;
        spr.fillRoundRect(bx,by2,bw,bh,2,DARK);
        float fill=constrain(co2/2000.0f,0,1.0f);
        if(fill>0.01f)spr.fillRoundRect(bx,by2,(int)(fill*bw),bh,2,cc);

        spr.fillRoundRect(2,58,64,52,4,CARD);spr.fillRoundRect(68,58,64,52,4,CARD);
        uint16_t tc=tempColor(temperature);iconTherm(8,62,tc);
        spr.setTextFont(2);spr.setTextColor(tc,CARD);
        if(temperature>-40){char t3[8];snprintf(t3,8,"%.1f",temperature);spr.drawString(t3,20,63);}
        else spr.drawString("--.-",20,63);
        spr.setTextColor(DIM,CARD);spr.setTextFont(1);spr.drawString("C",56,65);
        uint16_t hc=humColor(humidity);iconDrop(74,62,hc);
        spr.setTextFont(2);spr.setTextColor(hc,CARD);
        if(humidity>=0){char h3[8];snprintf(h3,8,"%.0f%%",humidity);spr.drawString(h3,88,63);}
        else spr.drawString("--%",88,63);

        int rx=136,ry=2,rh=20;
        spr.fillRoundRect(rx,ry,SW-rx-2,108,4,CARD);
        uint16_t heatC=heating?C_RED:DARK;iconFlame(rx+4,ry+3,heatC);
        spr.setTextFont(2);spr.setTextColor(heating?C_RED:DIM,CARD);spr.drawString(heating?"Heat ON":"Heat",rx+18,ry+2);ry+=rh+2;
        uint16_t doorC=doorOpen?C_ORANGE:DARK;iconDoor(rx+5,ry+2,doorC);
        spr.setTextColor(doorOpen?C_ORANGE:DIM,CARD);spr.drawString(doorOpen?"Door !":"Door",rx+18,ry+2);ry+=rh+2;
        uint16_t w1C=window1Open?C_BLUE:DARK;iconWindow(rx+3,ry+3,w1C);
        spr.setTextColor(window1Open?C_BLUE:DIM,CARD);spr.drawString(window1Open?"Win1 !":"Win 1",rx+18,ry+2);ry+=rh+2;
        uint16_t w2C=window2Open?C_BLUE:DARK;iconWindow(rx+3,ry+3,w2C);
        spr.setTextColor(window2Open?C_BLUE:DIM,CARD);spr.drawString(window2Open?"Win2 !":"Win 2",rx+18,ry+2);ry+=rh+2;
        uint16_t stC=stairMotion?C_YELLOW:DARK;iconStairs(rx+4,ry+3,stC);
        spr.setTextColor(stairMotion?C_YELLOW:DIM,CARD);spr.drawString(stairMotion?"Move!":"Stairs",rx+18,ry+2);
    }

    // ================ SCREEN 1 — Clock ================
    void drawScreen1() {
        spr.setTextFont(7);char tb[6];snprintf(tb,6,"%02d:%02d",hour,minute);
        int tw=spr.textWidth(tb);spr.setTextColor(WHITE,BG);spr.drawString(tb,(SW-tw)/2,8);
        const char*days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        const char*mons[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        char db[20];snprintf(db,20,"%s, %d %s",days[weekday%7],day,mons[month%12]);
        spr.setTextFont(2);spr.setTextColor(GRAY,BG);tw=spr.textWidth(db);spr.drawString(db,(SW-tw)/2,62);
        int y=88;spr.fillRoundRect(4,y,SW-8,24,4,CARD);
        spr.setTextFont(2);spr.setTextColor(co2Color(co2),CARD);
        char cb[12];if(co2>0)snprintf(cb,12,"%dppm",(int)co2);else snprintf(cb,12,"---");spr.drawString(cb,12,y+3);
        spr.setTextColor(tempColor(temperature),CARD);
        char t2[8];if(temperature>-40)snprintf(t2,8,"%.1fC",temperature);else snprintf(t2,8,"--.-");spr.drawString(t2,95,y+3);
        spr.setTextColor(humColor(humidity),CARD);
        char h2[8];if(humidity>=0)snprintf(h2,8,"%.0f%%",humidity);else snprintf(h2,8,"--%");spr.drawString(h2,170,y+3);
    }

    // ================ SCREEN 2 — HA Detail ================
    void drawScreen2() {
        int rh=20,y=2;
        spr.fillRoundRect(2,2,116,108,4,CARD);
        iconFlame(8,y+3,heating?C_RED:DARK);spr.setTextFont(2);spr.setTextColor(heating?C_RED:DIM,CARD);
        spr.drawString(heating?"Heating ON":"Heating OFF",24,y+2);y+=rh+2;
        iconDoor(9,y+2,doorOpen?C_ORANGE:DARK);spr.setTextColor(doorOpen?C_ORANGE:DIM,CARD);
        spr.drawString(doorOpen?"Door OPEN":"Door Closed",24,y+2);y+=rh+2;
        iconWindow(7,y+3,window1Open?C_BLUE:DARK);spr.setTextColor(window1Open?C_BLUE:DIM,CARD);
        spr.drawString(window1Open?"Win1 OPEN":"Win1 Closed",24,y+2);y+=rh+2;
        iconWindow(7,y+3,window2Open?C_BLUE:DARK);spr.setTextColor(window2Open?C_BLUE:DIM,CARD);
        spr.drawString(window2Open?"Win2 OPEN":"Win2 Closed",24,y+2);y+=rh+2;
        iconStairs(8,y+3,stairMotion?C_YELLOW:DARK);spr.setTextColor(stairMotion?C_YELLOW:DIM,CARD);
        spr.drawString(stairMotion?"Motion!":"Stairs Clear",24,y+2);
        spr.fillRoundRect(122,2,SW-124,108,4,CARD);y=6;
        spr.setTextFont(2);spr.setTextColor(DIM,CARD);spr.drawString("CO2",128,y);
        spr.setTextColor(co2Color(co2),CARD);
        char cb[8];if(co2>0)snprintf(cb,8,"%d",(int)co2);else snprintf(cb,8,"---");spr.drawString(cb,168,y);
        spr.setTextFont(1);spr.setTextColor(DIM,CARD);spr.drawString("ppm",210,y+4);y+=26;
        spr.setTextFont(2);spr.setTextColor(DIM,CARD);spr.drawString("Temp",128,y);
        spr.setTextColor(tempColor(temperature),CARD);
        char t4[8];if(temperature>-40)snprintf(t4,8,"%.1fC",temperature);else snprintf(t4,8,"--.-");spr.drawString(t4,178,y);y+=26;
        spr.setTextColor(DIM,CARD);spr.drawString("Hum",128,y);spr.setTextColor(humColor(humidity),CARD);
        char h4[8];if(humidity>=0)snprintf(h4,8,"%.0f%%",humidity);else snprintf(h4,8,"--%");spr.drawString(h4,178,y);
    }

    // ---- Render ----
    void render() {
        animFrame++;
        if (alertActive) {
            drawAlertScreen();
        } else {
            spr.fillSprite(BG);
            switch(screen){case 0:drawScreen0();break;case 1:drawScreen1();break;case 2:drawScreen2();break;}
            drawBottomBar();
        }
        spr.pushSprite(0,0);
        pCo2=co2;pTemp=temperature;pHum=humidity;
        pSt=stairMotion;pDo=doorOpen;pW1=window1Open;pW2=window2Open;pH=heating;
        pScr=screen;needsRedraw=false;
    }

    bool hasChanges() {
        if(needsRedraw||alertActive) return true;
        if((int)co2!=(int)pCo2) return true;
        if(abs(temperature-pTemp)>0.1f) return true;
        if(abs(humidity-pHum)>0.5f) return true;
        if(stairMotion!=pSt||doorOpen!=pDo||window1Open!=pW1||window2Open!=pW2) return true;
        if(heating!=pH||screen!=pScr) return true;
        return false;
    }
};
