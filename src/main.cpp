// ============================================================================
//  TTGO T-Display CO2 Sensor — v3 with Alert System
// ============================================================================
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include <Wire.h>
#include <time.h>
#include <Preferences.h>
#include "config.h"
#include "display.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
SCD30 scd30;
DisplayManager display;
Preferences prefs;

unsigned long lastSensorRead=0,lastMqttPublish=0;
unsigned long lastTimeUpdate=0,lastDisplayUpdate=0,lastDevicePublish=0;
float lastValidCO2=0;

bool btnLeftLast=HIGH,btnRightLast=HIGH;
unsigned long btnLeftTime=0,btnRightTime=0;

// Source entity IDs
char srcTemp[128]="",srcHum[128]="",srcStair[128]="";
char srcDoor[128]="",srcWin1[128]="",srcWin2[128]="",srcHeat[128]="";

// Window open timers
unsigned long win1OpenTime=0, win2OpenTime=0;
bool win1WasOpen=false, win2WasOpen=false;

// Door open tracking
unsigned long doorOpenTime=0;
bool doorWasOpen=false;

// Timed alert tracking (20s auto-dismiss)
unsigned long doorAlertTime=0;
unsigned long stairAlertTime=0;
bool stairWasActive=false;
unsigned long co2InfoTime=0;
bool co2InfoWasActive=false;
unsigned long win1GoodAlertTime=0;
bool win1GoodWasActive=false;
unsigned long win2GoodAlertTime=0;
bool win2GoodWasActive=false;

#define TIMED_ALERT_MS 20000  // 20 seconds

// Alert dismiss tracking (epoch seconds)
time_t dismissUntil[ALERT_ID_COUNT] = {0};

// Alert toggles
bool enableAlertDoor = true;
bool enableAlertStairs = true;
bool enableAlertWindows = true;
bool enableAlertCO2 = true;
bool enableAlertTempHum = true;

// ============================================================================
//  Preferences
// ============================================================================
void loadPrefs() {
    prefs.begin("co2cfg",true);
    strncpy(srcTemp,prefs.getString("sTemp","none").c_str(),127);
    strncpy(srcHum,prefs.getString("sHum","none").c_str(),127);
    strncpy(srcStair,prefs.getString("sStair","none").c_str(),127);
    strncpy(srcDoor,prefs.getString("sDoor","none").c_str(),127);
    strncpy(srcWin1,prefs.getString("sWin1","none").c_str(),127);
    strncpy(srcWin2,prefs.getString("sWin2","none").c_str(),127);
    strncpy(srcHeat,prefs.getString("sHeat","none").c_str(),127);
    enableAlertDoor = (prefs.getString("alDoor", "1") == "1");
    enableAlertStairs = (prefs.getString("alStairs", "1") == "1");
    enableAlertWindows = (prefs.getString("alWindows", "1") == "1");
    enableAlertCO2 = (prefs.getString("alCO2", "1") == "1");
    enableAlertTempHum = (prefs.getString("alTempHum", "1") == "1");
    prefs.end();
}
void savePref(const char*k,const char*v){prefs.begin("co2cfg",false);prefs.putString(k,v);prefs.end();}

// ============================================================================
//  WiFi & NTP
// ============================================================================
void setupWiFi(){WiFi.mode(WIFI_STA);WiFi.begin(WIFI_SSID,WIFI_PASSWORD);int a=0;while(WiFi.status()!=WL_CONNECTED&&a<40){delay(500);a++;}display.wifiConnected=(WiFi.status()==WL_CONNECTED);}
void setupTime(){configTime(GMT_OFFSET_SEC,DAYLIGHT_OFFSET_SEC,NTP_SERVER);}
void updateTime(){struct tm ti;if(getLocalTime(&ti,100)){display.hour=ti.tm_hour;display.minute=ti.tm_min;display.weekday=ti.tm_wday;display.day=ti.tm_mday;display.month=ti.tm_mon;display.updateBrightness();display.needsRedraw=true;}}

void publishDeviceState();

// ============================================================================
//  MQTT Discovery
// ============================================================================
void publishDiscovery() {
    char t[128],p[512];
    const char*D=MQTT_CLIENT_ID,*A=MQTT_AVAILABILITY_TOPIC;
    const char*devF="\"dev\":{\"ids\":[\"co2_sensor_ttgo\"],\"name\":\"CO2 Sensor TTGO\",\"mf\":\"DIY\",\"mdl\":\"TTGO+SCD30\"}";
    const char*devR="\"dev\":{\"ids\":[\"co2_sensor_ttgo\"]}";

    snprintf(t,128,"homeassistant/sensor/%s/co2/config",D);
    snprintf(p,512,"{\"name\":\"CO2\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.co2 }}\",\"uniq_id\":\"%s_co2\",\"dev_cla\":\"carbon_dioxide\",\"unit_of_meas\":\"ppm\",\"avty_t\":\"%s\",%s}",MQTT_STATE_TOPIC,D,A,devF);
    mqtt.publish(t,p,true);

    snprintf(t,128,"homeassistant/sensor/%s/rssi/config",D);
    snprintf(p,512,"{\"name\":\"WiFi Signal\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.rssi }}\",\"uniq_id\":\"%s_rssi\",\"dev_cla\":\"signal_strength\",\"unit_of_meas\":\"dBm\",\"ent_cat\":\"diagnostic\",\"avty_t\":\"%s\",%s}",MQTT_DEVICE_TOPIC,D,A,devR);
    mqtt.publish(t,p,true);

    snprintf(t,128,"homeassistant/number/%s/brightness/config",D);
    snprintf(p,512,"{\"name\":\"Brightness\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.brightness }}\",\"cmd_t\":\"%s\",\"uniq_id\":\"%s_bright\",\"min\":0,\"max\":255,\"step\":5,\"ent_cat\":\"config\",\"icon\":\"mdi:brightness-6\",\"avty_t\":\"%s\",%s}",MQTT_DEVICE_TOPIC,MQTT_CMD_BRIGHTNESS,D,A,devR);
    mqtt.publish(t,p,true);

    snprintf(t,128,"homeassistant/select/%s/screen/config",D);
    snprintf(p,512,"{\"name\":\"Screen\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.screen }}\",\"cmd_t\":\"%s\",\"uniq_id\":\"%s_screen\",\"options\":[\"Dashboard\",\"Clock\",\"HA Detail\"],\"ent_cat\":\"config\",\"icon\":\"mdi:monitor\",\"avty_t\":\"%s\",%s}",MQTT_DEVICE_TOPIC,MQTT_CMD_SCREEN,D,A,devR);
    mqtt.publish(t,p,true);

    struct{const char*n;const char*s;const char*ic;}sr[]={
        {"Temperature Source","src_temp","mdi:thermometer"},{"Humidity Source","src_hum","mdi:water-percent"},
        {"Stairs Source","src_stair","mdi:stairs"},{"Door Source","src_door","mdi:door-open"},
        {"Window 1 Source","src_win1","mdi:window-open"},{"Window 2 Source","src_win2","mdi:window-open"},
        {"Heating Source","src_heat","mdi:fire"},
    };
    for(int i=0;i<7;i++){
        snprintf(t,128,"homeassistant/text/%s/%s/config",D,sr[i].s);
        snprintf(p,512,"{\"name\":\"%s\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.%s }}\",\"cmd_t\":\"cmnd/co2_sensor/%s\",\"uniq_id\":\"%s_%s\",\"ent_cat\":\"config\",\"icon\":\"%s\",\"min\":0,\"max\":127,\"avty_t\":\"%s\",%s}",sr[i].n,MQTT_SOURCES_TOPIC,sr[i].s,sr[i].s,D,sr[i].s,sr[i].ic,A,devR);
        mqtt.publish(t,p,true);
    }

    struct {const char* n; const char* tp; const char* id;} sw[] = {
        {"Door Alert", MQTT_CMD_ALERT_DOOR, "door"},
        {"Stairs Alert", MQTT_CMD_ALERT_STAIRS, "stairs"},
        {"Windows Alert", MQTT_CMD_ALERT_WINDOWS, "windows"},
        {"CO2 Alert", MQTT_CMD_ALERT_CO2, "co2"},
        {"Temp/Hum Alert", MQTT_CMD_ALERT_TEMPHUM, "temphum"}
    };
    for(int i=0;i<5;i++){
        snprintf(t,128,"homeassistant/switch/%s/alert_%s/config",D,sw[i].id);
        snprintf(p,512,"{\"name\":\"%s\",\"stat_t\":\"%s\",\"val_tpl\":\"{{ value_json.al_%s }}\",\"cmd_t\":\"%s\",\"uniq_id\":\"%s_al_%s\",\"ent_cat\":\"config\",\"icon\":\"mdi:bell\",\"avty_t\":\"%s\",%s}",sw[i].n,MQTT_DEVICE_TOPIC,sw[i].id,sw[i].tp,D,sw[i].id,A,devR);
        mqtt.publish(t,p,true);
    }
}

void publishSources(){
    if(!mqtt.connected())return;char p[768];
    snprintf(p,768,"{\"src_temp\":\"%s\",\"src_hum\":\"%s\",\"src_stair\":\"%s\",\"src_door\":\"%s\",\"src_win1\":\"%s\",\"src_win2\":\"%s\",\"src_heat\":\"%s\"}",srcTemp,srcHum,srcStair,srcDoor,srcWin1,srcWin2,srcHeat);
    mqtt.publish(MQTT_SOURCES_TOPIC,p,true);
}

// ============================================================================
//  MQTT Callback
// ============================================================================
void mqttCallback(char*topic,byte*payload,unsigned int length){
    char msg[128];int len=min((unsigned int)127,length);memcpy(msg,payload,len);msg[len]='\0';
    String sT(topic);String sML(msg);sML.toLowerCase();
    if(sML=="unknown"||sML=="unavailable")return;
    bool active=(sML=="on"||sML=="open"||sML=="true"||sML=="detected"||sML=="1"||sML=="heat"||sML=="heating");

    if(sT==MQTT_IN_STAIR)display.stairMotion=active;
    else if(sT==MQTT_IN_DOOR)display.doorOpen=active;
    else if(sT==MQTT_IN_WIN1)display.window1Open=active;
    else if(sT==MQTT_IN_WIN2)display.window2Open=active;
    else if(sT==MQTT_IN_HEAT)display.heating=active;
    else if(sT==MQTT_IN_TEMP){float v=atof(msg);if(v>-40&&v<80)display.temperature=v;}
    else if(sT==MQTT_IN_HUM){float v=atof(msg);if(v>0&&v<=100)display.humidity=v;}
    else if(sT==MQTT_CMD_BRIGHTNESS){int v=atoi(msg);if(v>=0&&v<=255)display.setBrightness(v);}
    else if(sT==MQTT_CMD_SCREEN){if(String(msg)=="Dashboard")display.screen=0;else if(String(msg)=="Clock")display.screen=1;else if(String(msg)=="HA Detail")display.screen=2;}
    else if(sT=="cmnd/co2_sensor/src_temp"){strncpy(srcTemp,msg,127);savePref("sTemp",msg);publishSources();}
    else if(sT=="cmnd/co2_sensor/src_hum"){strncpy(srcHum,msg,127);savePref("sHum",msg);publishSources();}
    else if(sT=="cmnd/co2_sensor/src_stair"){strncpy(srcStair,msg,127);savePref("sStair",msg);publishSources();}
    else if(sT=="cmnd/co2_sensor/src_door"){strncpy(srcDoor,msg,127);savePref("sDoor",msg);publishSources();}
    else if(sT=="cmnd/co2_sensor/src_win1"){strncpy(srcWin1,msg,127);savePref("sWin1",msg);publishSources();}
    else if(sT=="cmnd/co2_sensor/src_win2"){strncpy(srcWin2,msg,127);savePref("sWin2",msg);publishSources();}
    else if(sT=="cmnd/co2_sensor/src_heat"){strncpy(srcHeat,msg,127);savePref("sHeat",msg);publishSources();}
    else if(sT==MQTT_CMD_ALERT_DOOR){enableAlertDoor=active;savePref("alDoor",active?"1":"0");publishDeviceState();}
    else if(sT==MQTT_CMD_ALERT_STAIRS){enableAlertStairs=active;savePref("alStairs",active?"1":"0");publishDeviceState();}
    else if(sT==MQTT_CMD_ALERT_WINDOWS){enableAlertWindows=active;savePref("alWindows",active?"1":"0");publishDeviceState();}
    else if(sT==MQTT_CMD_ALERT_CO2){enableAlertCO2=active;savePref("alCO2",active?"1":"0");publishDeviceState();}
    else if(sT==MQTT_CMD_ALERT_TEMPHUM){enableAlertTempHum=active;savePref("alTempHum",active?"1":"0");publishDeviceState();}
    display.needsRedraw=true;
}

// ============================================================================
//  MQTT Connect
// ============================================================================
void connectMQTT(){
    if(mqtt.connected())return;mqtt.setBufferSize(768);
    if(mqtt.connect(MQTT_CLIENT_ID,MQTT_USER,MQTT_PASSWORD,MQTT_AVAILABILITY_TOPIC,1,true,"offline")){
        display.mqttConnected=true;mqtt.publish(MQTT_AVAILABILITY_TOPIC,"online",true);
        publishDiscovery();publishSources();
        mqtt.subscribe(MQTT_IN_TEMP);mqtt.subscribe(MQTT_IN_HUM);mqtt.subscribe(MQTT_IN_STAIR);
        mqtt.subscribe(MQTT_IN_DOOR);mqtt.subscribe(MQTT_IN_WIN1);mqtt.subscribe(MQTT_IN_WIN2);mqtt.subscribe(MQTT_IN_HEAT);
        mqtt.subscribe(MQTT_CMD_BRIGHTNESS);mqtt.subscribe(MQTT_CMD_SCREEN);
        mqtt.subscribe("cmnd/co2_sensor/src_temp");mqtt.subscribe("cmnd/co2_sensor/src_hum");
        mqtt.subscribe("cmnd/co2_sensor/src_stair");mqtt.subscribe("cmnd/co2_sensor/src_door");
        mqtt.subscribe("cmnd/co2_sensor/src_win1");mqtt.subscribe("cmnd/co2_sensor/src_win2");mqtt.subscribe("cmnd/co2_sensor/src_heat");
        mqtt.subscribe(MQTT_CMD_ALERT_DOOR);mqtt.subscribe(MQTT_CMD_ALERT_STAIRS);mqtt.subscribe(MQTT_CMD_ALERT_WINDOWS);
        mqtt.subscribe(MQTT_CMD_ALERT_CO2);mqtt.subscribe(MQTT_CMD_ALERT_TEMPHUM);
        display.needsRedraw=true;
    } else display.mqttConnected=false;
}

void publishSensorData(){if(!mqtt.connected())return;char p[64];snprintf(p,64,"{\"co2\":%.0f}",lastValidCO2);mqtt.publish(MQTT_STATE_TOPIC,p);}
void publishDeviceState(){
    if(!mqtt.connected())return;
    const char*scr[]={"Dashboard","Clock","HA Detail"};char p[512];
    snprintf(p,512,"{\"brightness\":%d,\"screen\":\"%s\",\"rssi\":%d,\"al_door\":\"%s\",\"al_stairs\":\"%s\",\"al_windows\":\"%s\",\"al_co2\":\"%s\",\"al_temphum\":\"%s\"}",
             display.brightness,scr[display.screen%NUM_SCREENS],WiFi.RSSI(),
             enableAlertDoor?"ON":"OFF",enableAlertStairs?"ON":"OFF",enableAlertWindows?"ON":"OFF",enableAlertCO2?"ON":"OFF",enableAlertTempHum?"ON":"OFF");
    mqtt.publish(MQTT_DEVICE_TOPIC,p);
}
void readSensor(){if(!scd30.dataAvailable())return;float c=scd30.getCO2();if((int)c!=CO2_INVALID_READING&&c>0&&c<10000)lastValidCO2=c;display.co2=lastValidCO2;}

// ============================================================================
//  Alert System
// ============================================================================
bool isDismissed(AlertId id) {
    time_t now = time(NULL);
    return (now > 0 && dismissUntil[id] > now);
}

void dismissCurrent(bool forWeek) {
    if (!display.alertActive) return;
    time_t now = time(NULL);
    time_t dur = forWeek ? 7*24*3600 : 24*3600;
    dismissUntil[display.alertId] = now + dur;
    display.clearAlert();
}

void checkAlerts() {
    // Only show alerts during day hours
    if (display.nightMode) { display.clearAlert(); return; }

    // Track window open times
    unsigned long now = millis();
    bool justBooted = (now < 15000); // 15s startup grace period

    if (display.window1Open && !win1WasOpen) win1OpenTime = now;
    if (!display.window1Open) win1OpenTime = 0;
    win1WasOpen = display.window1Open;

    if (display.window2Open && !win2WasOpen) win2OpenTime = now;
    if (!display.window2Open) win2OpenTime = 0;
    win2WasOpen = display.window2Open;

    if (display.doorOpen && !doorWasOpen) doorOpenTime = now;
    if (!display.doorOpen) doorOpenTime = 0;
    
    // Check conditions in priority order (highest first)
    // ALARM: Door opened (10-second notification)
    if (display.doorOpen && !doorWasOpen && !justBooted) doorAlertTime = now; // rising edge
    if (!display.doorOpen) doorWasOpen = false;
    if (display.doorOpen) doorWasOpen = true;

    if (enableAlertDoor && doorAlertTime > 0 && (now - doorAlertTime) < DOOR_ALERT_MS && !isDismissed(AID_DOOR)) {
        display.setAlert(AT_ALARM, AID_DOOR, "DOOR OPEN!", "Door was opened!");
        return;
    }
    if (doorAlertTime > 0 && (now - doorAlertTime) >= DOOR_ALERT_MS) doorAlertTime = 0; // auto-clear

    // WARNING: Stair motion (20s notification)
    if (display.stairMotion && !stairWasActive && !justBooted) stairAlertTime = now;
    if (!display.stairMotion) { stairWasActive = false; stairAlertTime = 0; }
    if (display.stairMotion) stairWasActive = true;

    if (enableAlertStairs && stairAlertTime > 0 && (now - stairAlertTime) < TIMED_ALERT_MS && !isDismissed(AID_STAIRS)) {
        display.setAlert(AT_WARN, AID_STAIRS, "STAIRS", "Movement detected!");
        return;
    }
    if (stairAlertTime > 0 && (now - stairAlertTime) >= TIMED_ALERT_MS) stairAlertTime = 0;

    // WARNING: CO2 too high
    if (enableAlertCO2 && display.co2 >= CO2_POOR && !isDismissed(AID_CO2) && !justBooted) {
        display.setAlert(AT_WARN, AID_CO2, "CO2 HIGH!", "Open windows now!");
        return;
    }

    // WARNING: Temp out of warn range
    if (enableAlertTempHum && display.temperature > -40) {
        if ((display.temperature < TEMP_WARN_LOW || display.temperature > TEMP_WARN_HIGH) && !isDismissed(AID_TEMP)) {
            char m[40]; snprintf(m, 40, "Temp: %.1f C", display.temperature);
            display.setAlert(AT_WARN, AID_TEMP, "TEMP!", m);
            return;
        }
    }

    // WARNING: Humidity out of warn range
    if (enableAlertTempHum && display.humidity >= 0) {
        if ((display.humidity < HUM_WARN_LOW || display.humidity > HUM_WARN_HIGH) && !isDismissed(AID_HUM)) {
            char m[40]; snprintf(m, 40, "Humidity: %.0f%%", display.humidity);
            display.setAlert(AT_WARN, AID_HUM, "HUMIDITY!", m);
            return;
        }
    }

    // INFO: CO2 moderate (20s notification)
    bool co2Mod = (display.co2 >= CO2_MODERATE && display.co2 < CO2_POOR);
    if (co2Mod && !co2InfoWasActive && !justBooted) co2InfoTime = now;
    if (!co2Mod) { co2InfoWasActive = false; co2InfoTime = 0; }
    if (co2Mod) co2InfoWasActive = true;

    if (enableAlertCO2 && co2InfoTime > 0 && (now - co2InfoTime) < TIMED_ALERT_MS && !isDismissed(AID_CO2)) {
        display.setAlert(AT_INFO, AID_CO2, "CO2", "Consider ventilating");
        return;
    }
    if (co2InfoTime > 0 && (now - co2InfoTime) >= TIMED_ALERT_MS) co2InfoTime = 0;

    // INFO: Window open > 30 min → close
    if (enableAlertWindows && win1OpenTime > 0 && (now - win1OpenTime) > WIN_CLOSE_MS && !isDismissed(AID_WIN1_CLOSE)) {
        display.setAlert(AT_INFO, AID_WIN1_CLOSE, "WINDOW 1", "Close window!");
        return;
    }
    if (enableAlertWindows && win2OpenTime > 0 && (now - win2OpenTime) > WIN_CLOSE_MS && !isDismissed(AID_WIN2_CLOSE)) {
        display.setAlert(AT_INFO, AID_WIN2_CLOSE, "WINDOW 2", "Close window!");
        return;
    }

    // INFO: Window open > 5 min → thumbs up (20s notification)
    bool win1Good = (win1OpenTime > 0 && (now - win1OpenTime) > WIN_GOOD_MS && (now - win1OpenTime) <= WIN_CLOSE_MS);
    if (win1Good && !win1GoodWasActive && !justBooted) win1GoodAlertTime = now;
    if (!win1Good) { win1GoodWasActive = false; win1GoodAlertTime = 0; }
    if (win1Good) win1GoodWasActive = true;

    if (enableAlertWindows && win1GoodAlertTime > 0 && (now - win1GoodAlertTime) < TIMED_ALERT_MS && !isDismissed(AID_WIN1_GOOD)) {
        display.setAlert(AT_INFO, AID_WIN1_GOOD, "WINDOW 1", "Great ventilation!");
        return;
    }
    if (win1GoodAlertTime > 0 && (now - win1GoodAlertTime) >= TIMED_ALERT_MS) win1GoodAlertTime = 0;

    bool win2Good = (win2OpenTime > 0 && (now - win2OpenTime) > WIN_GOOD_MS && (now - win2OpenTime) <= WIN_CLOSE_MS);
    if (win2Good && !win2GoodWasActive && !justBooted) win2GoodAlertTime = now;
    if (!win2Good) { win2GoodWasActive = false; win2GoodAlertTime = 0; }
    if (win2Good) win2GoodWasActive = true;

    if (enableAlertWindows && win2GoodAlertTime > 0 && (now - win2GoodAlertTime) < TIMED_ALERT_MS && !isDismissed(AID_WIN2_GOOD)) {
        display.setAlert(AT_INFO, AID_WIN2_GOOD, "WINDOW 2", "Great ventilation!");
        return;
    }
    if (win2GoodAlertTime > 0 && (now - win2GoodAlertTime) >= TIMED_ALERT_MS) win2GoodAlertTime = 0;

    // No alert conditions → clear
    display.clearAlert();
}

// ============================================================================
//  Buttons (context-dependent)
// ============================================================================
void handleButtons() {
    unsigned long now = millis();
    bool l = digitalRead(BTN_LEFT);
    if (l==LOW && btnLeftLast==HIGH && now-btnLeftTime>250) {
        btnLeftTime = now;
        if (display.alertActive) dismissCurrent(false); // dismiss for day
        else display.prevScreen();
    }
    btnLeftLast = l;

    bool r = digitalRead(BTN_RIGHT);
    if (r==LOW && btnRightLast==HIGH && now-btnRightTime>250) {
        btnRightTime = now;
        if (display.alertActive) dismissCurrent(true); // dismiss for week
        else display.nextScreen();
    }
    btnRightLast = r;
}

// ============================================================================
//  Setup & Loop
// ============================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== CO2 Sensor TTGO v3 ===");
    loadPrefs();
    pinMode(BTN_LEFT,INPUT_PULLUP);
    pinMode(BTN_RIGHT,INPUT_PULLUP);
    display.begin(); display.render();
    Wire.begin(21,22);
    if(scd30.begin())scd30.setMeasurementInterval(5);
    setupWiFi();setupTime();
    mqtt.setServer(MQTT_SERVER,MQTT_PORT);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(768);
}

void loop() {
    unsigned long now=millis();
    handleButtons();

    if(WiFi.status()!=WL_CONNECTED){display.wifiConnected=false;static unsigned long lr=0;if(now-lr>30000){lr=now;setupWiFi();}}
    else display.wifiConnected=true;

    if(WiFi.status()==WL_CONNECTED){
        if(!mqtt.connected()){display.mqttConnected=false;static unsigned long lr=0;if(now-lr>10000){lr=now;connectMQTT();}}
        mqtt.loop();
    }

    if(now-lastSensorRead>=SCD30_READ_INTERVAL_MS){lastSensorRead=now;readSensor();}
    if(now-lastMqttPublish>=MQTT_PUBLISH_INTERVAL){lastMqttPublish=now;publishSensorData();}
    if(now-lastDevicePublish>=60000){lastDevicePublish=now;publishDeviceState();}
    if(now-lastTimeUpdate>=60000){lastTimeUpdate=now;updateTime();}

    // Check alerts every second
    static unsigned long lastAlertCheck=0;
    if(now-lastAlertCheck>=1000){lastAlertCheck=now;checkAlerts();}

    if(now-lastDisplayUpdate>=200){
        if(display.hasChanges()){lastDisplayUpdate=now;display.render();}
    }
}
