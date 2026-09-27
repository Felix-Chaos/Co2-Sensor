#pragma once
#include <TFT_eSPI.h>
#include "config.h"

extern unsigned long win1OpenTime;
extern unsigned long win2OpenTime;
extern unsigned long doorOpenTime;
extern bool alertsMuted;

// Palette — dark "midnight slate" base with a soft, less-saturated semantic
// accent set (easier on the eyes than pure RGB primaries at close range).
#define BG          0x0000   // #000000
#define CARD        0x10A3   // #12161F
#define CARD_HI     0x2147   // #232B3A
#define WHITE       0xFFFF   // #FFFFFF
#define GRAY        0xAD97   // #A8B0BE
#define DIM         0x73F1   // #747C8A
#define DARK        0x31C9   // #333B49
#define ACCENT      0x4E98   // #4FD1C5 — brand teal, used for active/highlight accents
#define C_BLUE      0x4D5F   // #4FA8FF
#define C_GREEN     0x3693   // #34D399
#define C_YELLOW    0xFDE4   // #FBBF24
#define C_ORANGE    0xFC87   // #FB923C
#define C_RED       0xFACB   // #FF5A5A
#define C_PURPLE    0xA45F   // #A78BFA
#define C_DKRED     0x58A2   // #5A1414
#define C_DKORANGE  0x5982   // #5A3310
#define C_DKYELLOW  0x49E1   // #4A3C08
#define C_DKBLUE    0x1149   // #142A4A
#define C_DKGREEN   0x09E5   // #0F3D2E
#define SW          240
#define SH          135

// CO2 gauge: 260° arc with the gap at the bottom, scaled 400 (outdoor) → 2000 ppm
#define GAUGE_A0    50
#define GAUGE_A1    310
#define GAUGE_MIN   400.0f
#define GAUGE_MAX   2000.0f

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

    // Temporary on-screen message (button feedback)
    bool toastActive=false;
    char toastMsg[32]="";
    unsigned long toastUntil=0;

    // Full-brightness boost (combo button hold)
    bool boosted=false;
    unsigned long boostUntil=0;

    // CO2 rate-of-change trend (sampled every few minutes, see updateTrend())
    float co2TrendRef=-1;
    int co2Trend=0; // -1 falling, 0 flat, 1 rising

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
        if(night!=nightMode){nightMode=night;if(!boosted)setBrightness(night?BRIGHTNESS_NIGHT:BRIGHTNESS_DAY);}
    }

    void showToast(const char* msg, unsigned long durMs) {
        strncpy(toastMsg,msg,31); toastMsg[31]=0;
        toastActive=true; toastUntil=millis()+durMs;
        needsRedraw=true;
    }
    void updateToast() {
        if(toastActive && (long)(millis()-toastUntil)>=0){toastActive=false;needsRedraw=true;}
    }

    void boostBrightness(unsigned long durMs) {
        boosted=true; boostUntil=millis()+durMs;
        setBrightness(BRIGHTNESS_DAY);
        needsRedraw=true;
    }
    void updateBoost() {
        if(boosted && (long)(millis()-boostUntil)>=0){
            boosted=false;
            setBrightness(nightMode?BRIGHTNESS_NIGHT:BRIGHTNESS_DAY);
            needsRedraw=true;
        }
    }

    void updateTrend() {
        if (co2 <= 0) return;
        if (co2TrendRef < 0) { co2TrendRef = co2; return; }
        float diff = co2 - co2TrendRef;
        int newTrend = (diff > 30) ? 1 : (diff < -30) ? -1 : 0;
        if (newTrend != co2Trend) { co2Trend = newTrend; needsRedraw = true; }
        co2TrendRef = co2;
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

    // ---- Small icons (chips, alert badges) ----
    void iconFlame(int x,int y,uint16_t c,uint16_t off=CARD){
        uint16_t inner=(c==DARK)?off:C_YELLOW;
        spr.fillTriangle(x+5,y,x+1,y+10,x+9,y+10,c);spr.fillCircle(x+5,y+9,4,c);
        spr.fillTriangle(x+5,y+4,x+3,y+10,x+7,y+10,inner);spr.fillCircle(x+5,y+9,2,inner);
    }
    void iconDoor(int x,int y,uint16_t c){spr.drawRect(x,y,9,13,c);spr.drawRect(x+1,y+1,7,11,c);spr.fillCircle(x+6,y+7,1,c);}
    void iconStairs(int x,int y,uint16_t c){for(int i=0;i<4;i++)spr.fillRect(x+i*3,y+9-i*3,3,3+i*3,c);}
    void iconTherm(int x,int y,uint16_t c){spr.fillCircle(x+3,y+11,3,c);spr.fillRect(x+1,y,4,10,c);spr.fillRect(x+2,y+1,2,6,BG);}
    void iconDrop(int x,int y,uint16_t c){spr.fillCircle(x+4,y+8,4,c);spr.fillTriangle(x+4,y,x,y+8,x+8,y+8,c);}
    void iconCheck(int x,int y,uint16_t c){
        spr.drawWideLine(x+1,y+9,x+6,y+14,3,c,CARD);
        spr.drawWideLine(x+6,y+14,x+15,y+2,3,c,CARD);
    }

    // ---- Large icons (tiles) ----
    // Casement window 24x22; when open the right sash is drawn swung inward.
    void iconWindowL(int x,int y,bool open,uint16_t c){
        spr.drawRect(x,y,24,22,c); spr.drawRect(x+1,y+1,22,20,c);
        spr.fillRect(x+11,y,2,22,c);
        if(!open){
            spr.fillRect(x,y+10,24,2,c);
        } else {
            spr.fillRect(x+2,y+10,9,2,c);
            for(int t=0;t<2;t++){
                spr.drawLine(x+13,y+2+t,x+20,y+6+t,c);
                spr.drawFastVLine(x+20-t,y+6,11,c);
                spr.drawLine(x+13,y+19-t,x+20,y+16-t,c);
            }
        }
    }
    // Door 16x24; when open the leaf swings out from the left hinge.
    void iconDoorL(int x,int y,bool open,uint16_t c){
        spr.drawRect(x,y,16,24,c); spr.drawRect(x+1,y+1,14,22,c);
        if(!open){ spr.fillCircle(x+11,y+13,2,c); }
        else {
            spr.fillTriangle(x+2,y+2,x+9,y+5,x+2,y+22,c);
            spr.fillTriangle(x+9,y+5,x+9,y+24,x+2,y+22,c);
        }
    }
    void iconFlameL(int x,int y,uint16_t c,uint16_t inner){
        spr.fillTriangle(x+10,y,x+2,y+16,x+18,y+16,c);
        spr.fillCircle(x+10,y+16,8,c);
        spr.fillTriangle(x+10,y+9,x+6,y+18,x+14,y+18,inner);
        spr.fillCircle(x+10,y+18,4,inner);
    }
    void iconStairsL(int x,int y,uint16_t c){for(int i=0;i<4;i++)spr.fillRect(x+i*6,y+15-i*5,6,5+i*5,c);}

    void drawDots(int y){int cx=SW/2;for(int i=0;i<NUM_SCREENS;i++){int dx=cx+(i-1)*10;if(i==screen)spr.fillCircle(dx,y,3,ACCENT);else spr.drawCircle(dx,y,3,DARK);}}

    void drawTrendArrow(int x,int y,int trend,uint16_t c){
        if(trend>0)spr.fillTriangle(x,y+6,x+6,y+6,x+3,y,c);
        else if(trend<0)spr.fillTriangle(x,y,x+6,y,x+3,y+6,c);
    }

    // ---- Bottom bar ----
    void drawBottomBar() {
        int y=SH-13;
        spr.drawFastHLine(0,y-2,SW,CARD_HI);
        spr.setTextFont(1);
        spr.setTextColor(wifiConnected?C_GREEN:C_RED,BG);spr.drawString(wifiConnected?"WiFi":"NoWF",3,y);
        spr.setTextColor(mqttConnected?C_GREEN:C_RED,BG);spr.drawString(mqttConnected?"MQTT":"NoMQ",34,y);
        drawDots(y+4);
        if(alertsMuted){spr.setTextColor(C_PURPLE,BG);spr.drawString("DND",155,y);}
        char tb[6];snprintf(tb,6,"%02d:%02d",hour,minute);
        spr.setTextColor(DIM,BG);spr.drawString(tb,SW-32,y);
    }

    // ---- Toast (temporary button-feedback banner) ----
    void drawToast() {
        int h=18,y=SH-16-h;
        spr.fillRoundRect(4,y,SW-8,h,3,CARD_HI);
        spr.setTextFont(2);spr.setTextColor(WHITE,CARD_HI);
        int tw=spr.textWidth(toastMsg);
        spr.drawString(toastMsg,(SW-tw)/2,y+2);
    }

    // ================ ALERT OVERLAY ================
    void drawAlertScreen() {
        uint16_t accent;
        const char* typeLabel;
        if (alertType == AT_ALARM)     { accent=C_RED;    typeLabel="ALARM"; }
        else if (alertType == AT_WARN) { accent=C_ORANGE; typeLabel="WARNING"; }
        else                           { accent=C_BLUE;   typeLabel="INFO"; }

        spr.fillSprite(BG);
        if (alertType == AT_ALARM) {
            // ---- Lighthouse pulse effect ----
            int cx = SW/2, cy = SH/2 - 8;
            for (int ring = 0; ring < 4; ring++) {
                int r = (animFrame * 6 + ring * 30) % 120;
                if (r > 0 && r < 120) {
                    uint16_t col = (r < 40) ? C_RED : (r < 70) ? C_DKRED : 0x4000;
                    spr.drawCircle(cx, cy, r, col);
                    spr.drawCircle(cx, cy, r+1, col);
                    if (r < 30) spr.drawCircle(cx, cy, r+2, col);
                }
            }
            int coreSize = 8 + (animFrame % 6 < 3 ? 4 : 0);
            spr.fillCircle(cx, cy, coreSize, C_RED);
            spr.fillCircle(cx, cy, coreSize - 3, C_DKRED);

            spr.setTextColor(WHITE, BG);
            spr.setTextFont(4);
            int tw = spr.textWidth(typeLabel);
            spr.drawString(typeLabel, (SW-tw)/2, 2);

            spr.setTextFont(2); spr.setTextColor(C_RED, BG);
            tw = spr.textWidth(alertTitle);
            spr.drawString(alertTitle, (SW-tw)/2, cy + coreSize + 6);

            spr.setTextFont(1); spr.setTextColor(GRAY, BG);
            tw = spr.textWidth(alertMsg);
            spr.drawString(alertMsg, (SW-tw)/2, cy + coreSize + 24);
        } else {
            // Severity pill, top-center
            spr.setTextFont(1);
            int tw = spr.textWidth(typeLabel);
            int pw = tw+16, px=(SW-pw)/2;
            spr.fillRoundRect(px,4,pw,14,7,accent);
            spr.setTextColor(BG,accent);
            spr.drawString(typeLabel,px+8,7);

            // Icon badge
            int cx=SW/2, cy=50, rad=24;
            spr.fillSmoothCircle(cx,cy,rad,accent,BG);
            spr.fillSmoothCircle(cx,cy,rad-3,CARD,accent);
            switch(alertId) {
                case AID_STAIRS: iconStairsL(cx-12,cy-10,accent); break;
                case AID_WIN1_GOOD: case AID_WIN2_GOOD: iconCheck(cx-8,cy-8,accent); break;
                case AID_WIN1_CLOSE: case AID_WIN2_CLOSE: iconWindowL(cx-12,cy-11,true,accent); break;
                case AID_CO2: spr.setTextFont(2);spr.setTextColor(accent,CARD);spr.drawString("CO2",cx-13,cy-8); break;
                case AID_TEMP: iconTherm(cx-3,cy-8,accent); break;
                case AID_HUM: iconDrop(cx-4,cy-7,accent); break;
                default: break;
            }

            spr.setTextFont(2); spr.setTextColor(WHITE, BG);
            tw = spr.textWidth(alertTitle);
            spr.drawString(alertTitle, (SW-tw)/2, cy+rad+5);

            spr.setTextFont(1); spr.setTextColor(GRAY, BG);
            tw = spr.textWidth(alertMsg);
            spr.drawString(alertMsg, (SW-tw)/2, cy+rad+23);
        }

        // Button hints, placed over the physical buttons
        spr.setTextFont(1); spr.setTextColor(DIM,BG);
        spr.drawString("< 1 day",4,108);
        const char* wk="1 week >"; int tw2=spr.textWidth(wk);
        spr.drawString(wk,SW-4-tw2,108);

        drawBottomBar();
    }

    // ================ HELPERS ================
    void formatDuration(unsigned long openTime, char* out, size_t len) {
        if (openTime == 0) { snprintf(out, len, "open"); return; }
        unsigned long s = (millis() - openTime) / 1000;
        if (s < 60)        snprintf(out, len, "%lus", s);
        else if (s < 3600) snprintf(out, len, "%lum", s / 60);
        else               snprintf(out, len, "%luh%02lu", s / 3600, (s / 60) % 60);
    }

    // Window airing state: 0 closed, 1 just opened, 2 good airing, 3 open too long
    int windowState(bool open, unsigned long t) {
        if (!open) return 0;
        if (t == 0) return 1;
        unsigned long e = millis() - t;
        if (e > WIN_CLOSE_MS) return 3;
        if (e > WIN_GOOD_MS) return 2;
        return 1;
    }
    void windowColors(int s, uint16_t &fg, uint16_t &bg) {
        switch (s) {
            case 1:  fg=C_BLUE;   bg=C_DKBLUE;   break;
            case 2:  fg=C_GREEN;  bg=C_DKGREEN;  break;
            case 3:  fg=C_ORANGE; bg=C_DKORANGE; break;
            default: fg=DIM;      bg=CARD;       break;
        }
    }

    // Window tile: color tracks airing state, big timer while open, pulsing border when open too long
    void drawWindowTile(int x,int y,int w,int h,const char* label,bool open,unsigned long openTime){
        int s=windowState(open,openTime); uint16_t fg,bg; windowColors(s,fg,bg);
        spr.fillRoundRect(x,y,w,h,6,bg);
        if(s==3 && (millis()/1000)%2){spr.drawRoundRect(x,y,w,h,6,fg);spr.drawRoundRect(x+1,y+1,w-2,h-2,5,fg);}
        spr.setTextFont(1);spr.setTextColor(fg,bg);spr.drawString(label,x+5,y+4);
        iconWindowL(x+(w-24)/2,y+7,open,s?fg:DARK);
        char tb[10];
        if(open){formatDuration(openTime,tb,sizeof(tb));spr.setTextFont(2);}
        else    {snprintf(tb,sizeof(tb),"closed");spr.setTextFont(1);}
        int tw=spr.textWidth(tb);
        spr.drawString(tb,x+(w-tw)/2,open?y+h-19:y+h-13);
    }

    // Compact status chip (door / heat / stairs): lit with its color when active
    void drawChip(int x,int y,int w,int h,int kind,bool on,const char* txt,uint16_t fg,uint16_t dkbg){
        uint16_t bg=on?dkbg:CARD, c=on?fg:DARK;
        spr.fillRoundRect(x,y,w,h,4,bg);
        switch(kind){
            case 0: iconDoor(x+5,y+2,c); break;
            case 1: iconFlame(x+4,y+1,c,bg); break;
            case 2: iconStairs(x+4,y+2,c); break;
        }
        if(on && txt[0]){spr.setTextFont(1);spr.setTextColor(fg,bg);spr.drawString(txt,x+18,y+5);}
    }

    // Large sensor tile for the Home screen
    void drawSensorTile(int x,int y,int w,int h,int kind,bool on,uint16_t fg,uint16_t dkbg,const char* value){
        uint16_t bg=on?dkbg:CARD, c=on?fg:DARK;
        spr.fillRoundRect(x,y,w,h,6,bg);
        int cx=x+w/2, iy=y+8;
        switch(kind){
            case 0: iconDoorL(cx-8,iy,on,c); break;
            case 1: iconFlameL(cx-10,iy,c,on?C_YELLOW:bg); break;
            case 2: iconStairsL(cx-12,iy+3,c); break;
        }
        spr.setTextFont(2);spr.setTextColor(on?fg:DIM,bg);
        int tw=spr.textWidth(value);spr.drawString(value,cx-tw/2,y+h-19);
    }

    int gaugeAngle(float p){
        float f=(p-GAUGE_MIN)/(GAUGE_MAX-GAUGE_MIN);
        f=constrain(f,0.0f,1.0f);
        return GAUGE_A0+(int)(f*(GAUGE_A1-GAUGE_A0));
    }

    // CO2 arc gauge: dim zone track (good/ok/poor/bad) + bright value arc + knob, readout in the middle
    void drawGauge(int cx,int cy,int r,int ir){
        int a1=gaugeAngle(CO2_GOOD),a2=gaugeAngle(CO2_MODERATE),a3=gaugeAngle(CO2_POOR);
        spr.drawArc(cx,cy,r,ir,GAUGE_A0,a1,C_DKGREEN,BG,false);
        spr.drawArc(cx,cy,r,ir,a1,a2,C_DKYELLOW,BG,false);
        spr.drawArc(cx,cy,r,ir,a2,a3,C_DKORANGE,BG,false);
        spr.drawArc(cx,cy,r,ir,a3,GAUGE_A1,C_DKRED,BG,false);

        uint16_t cc=co2Color(co2);
        if(co2>0){
            int av=gaugeAngle(co2); if(av<GAUGE_A0+2)av=GAUGE_A0+2;
            spr.drawSmoothArc(cx,cy,r,ir,GAUGE_A0,av,cc,BG,true);
            float rad=av*DEG_TO_RAD, rm=(r+ir)/2.0f;
            spr.fillSmoothCircle(cx-(int)(rm*sinf(rad)),cy+(int)(rm*cosf(rad)),(r-ir)/2+2,WHITE,cc);
        }

        spr.setTextFont(1);spr.setTextColor(DIM,BG);
        int tw=spr.textWidth("CO2");spr.drawString("CO2",cx-tw/2,cy-30);

        char buf[8];if(co2>0)snprintf(buf,8,"%d",(int)co2);else snprintf(buf,8,"---");
        spr.setTextFont(4);spr.setTextColor(co2>0?WHITE:DIM,BG);
        tw=spr.textWidth(buf);spr.drawString(buf,cx-tw/2,cy-19);

        spr.setTextFont(1);spr.setTextColor(DIM,BG);
        tw=spr.textWidth("ppm");
        int px=cx-tw/2-(co2Trend?5:0);
        spr.drawString("ppm",px,cy+10);
        if(co2Trend)drawTrendArrow(px+tw+4,cy+10,co2Trend,co2Trend>0?C_ORANGE:C_GREEN);

        spr.setTextFont(2);spr.setTextColor(cc,BG);
        const char* ql=co2Label(co2);
        tw=spr.textWidth(ql);spr.drawString(ql,cx-tw/2,cy+28);
    }

    // ================ SCREEN 0 — Dashboard ================
    void drawScreen0() {
        drawGauge(61,61,57,48);

        const int px=124, pw=SW-px-2;

        // Climate card: temperature | humidity
        spr.fillRoundRect(px,2,pw,42,6,CARD);
        spr.drawFastVLine(px+pw/2,8,30,CARD_HI);
        spr.setTextFont(1);spr.setTextColor(DIM,CARD);
        spr.drawString("TEMP",px+6,6);spr.drawString("HUM",px+pw/2+6,6);
        char t[8];
        spr.setTextFont(4);spr.setTextColor(tempColor(temperature),CARD);
        if(temperature>-40)snprintf(t,8,"%.1f",temperature);else snprintf(t,8,"--");
        spr.drawString(t,px+5,16);
        spr.setTextColor(humColor(humidity),CARD);
        if(humidity>=0)snprintf(t,8,"%.0f",humidity);else snprintf(t,8,"--");
        spr.drawString(t,px+pw/2+6,16);
        int hw=spr.textWidth(t);
        spr.setTextFont(2);spr.setTextColor(DIM,CARD);spr.drawString("%",px+pw/2+8+hw,24);

        // Window tiles
        int tw=(pw-4)/2;
        drawWindowTile(px,48,tw,52,"W1",window1Open,win1OpenTime);
        drawWindowTile(px+tw+4,48,tw,52,"W2",window2Open,win2OpenTime);

        // Door / heat / stairs chips
        int cw=(pw-6)/3, cy=103;
        char d[10]="";
        if(doorOpen)formatDuration(doorOpenTime,d,sizeof(d));
        drawChip(px,cy,cw,16,0,doorOpen,d,C_ORANGE,C_DKORANGE);
        drawChip(px+cw+3,cy,cw,16,1,heating,"on",C_RED,C_DKRED);
        drawChip(px+2*(cw+3),cy,cw,16,2,stairMotion,"!",C_YELLOW,C_DKYELLOW);
    }

    // ================ SCREEN 1 — Clock ================
    void drawScreen1() {
        spr.setTextFont(7);char tb[6];snprintf(tb,6,"%02d:%02d",hour,minute);
        int tw=spr.textWidth(tb);spr.setTextColor(WHITE,BG);spr.drawString(tb,(SW-tw)/2,8);
        const char*days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        const char*mons[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        char db[20];snprintf(db,20,"%s, %d %s",days[weekday%7],day,mons[month%12]);
        spr.setTextFont(2);spr.setTextColor(GRAY,BG);tw=spr.textWidth(db);spr.drawString(db,(SW-tw)/2,62);

        int y=88,pw=74,gap=4,px=4;
        char v[12];
        uint16_t c[3]={co2Color(co2),tempColor(temperature),humColor(humidity)};
        if(co2>0)snprintf(v,12,"%dppm",(int)co2);else snprintf(v,12,"---");
        for(int i=0;i<3;i++){
            if(i==1){if(temperature>-40)snprintf(v,12,"%.1fC",temperature);else snprintf(v,12,"--.-");}
            if(i==2){if(humidity>=0)snprintf(v,12,"%.0f%%",humidity);else snprintf(v,12,"--%%");}
            spr.fillRoundRect(px,y,pw,24,6,CARD);
            spr.fillSmoothCircle(px+10,y+12,3,c[i],CARD);
            spr.setTextColor(c[i],CARD);
            spr.drawString(v,px+18,y+4);
            px+=pw+gap;
        }
    }

    // ================ SCREEN 2 — Home (large sensor tiles) ================
    void drawScreen2() {
        char v[14];
        int w3=(SW-4-8)/3;
        drawSensorTile(2,2,w3,56,1,heating,C_RED,C_DKRED,heating?"Heating":"Idle");
        if(doorOpen){char d[10];formatDuration(doorOpenTime,d,sizeof(d));snprintf(v,14,"Open %s",d);}
        else snprintf(v,14,"Closed");
        drawSensorTile(2+w3+4,2,w3,56,0,doorOpen,C_ORANGE,C_DKORANGE,v);
        drawSensorTile(2+2*(w3+4),2,w3,56,2,stairMotion,C_YELLOW,C_DKYELLOW,stairMotion?"Motion":"Clear");

        int w2=(SW-4-4)/2;
        drawWindowTile(2,62,w2,56,"W1",window1Open,win1OpenTime);
        drawWindowTile(2+w2+4,62,w2,56,"W2",window2Open,win2OpenTime);
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
            if(toastActive)drawToast();
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
