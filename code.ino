// ============================================================
//  EV Charging Station — Sky Blue v3 (Fully Responsive)
// ============================================================

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <base64.h>

const char* ssid     = "Asritha";
const char* password = "9346421050";
const char* smtpServer    = "smtp.gmail.com";
const int   smtpPort      = 465;
const char* senderEmail   = "napariyojana@gmail.com";
const char* senderPass    = "jjoykzsfunsjuzij";
const char* receiverEmail = "asrithakakaraparthi123@gmail.com";
const char* sheetsURL     = "https://script.google.com/macros/s/AKfycbxJgGIt2_SnFfKyk76ACnRe_RMhrNxwH8oX2wcyN1uI6AzEXUSnckrrGnfRrU9t5PRh/exec";

const int gateIrPin=D3,pad1IrPin=D0,pad2IrPin=D2,pad3IrPin=D1,servoPin=D4;
const float costPerMinute=2.5;
const int gateOpenAngle=90,gateClosedAngle=0;

ESP8266WebServer server(80);
LiquidCrystal_I2C lcd(0x27,16,2);
Servo gateServo;

bool pad1_occupied=false,pad2_occupied=false,pad3_occupied=false;
unsigned long pad1_startTime=0,pad2_startTime=0,pad3_startTime=0;
String currentStatusMessage="Initializing...";
String lastBillMessages[3]={"","",""};
float  lastBillAmounts[3]={0,0,0};
unsigned long lastDurations[3]={0,0,0};

String lcdLine0="",lcdLine1="";
bool lcdTempActive=false;
unsigned long lcdTempUntil=0;

void lcdWrite(String l0,String l1){
  while(l0.length()<16)l0+=' '; while(l1.length()<16)l1+=' ';
  if(l0==lcdLine0&&l1==lcdLine1)return;
  lcdLine0=l0;lcdLine1=l1;
  lcd.clear();lcd.setCursor(0,0);lcd.print(l0);lcd.setCursor(0,1);lcd.print(l1);
}
void lcdTempMsg(String l0,String l1,unsigned long d){
  lcdTempActive=true;lcdTempUntil=millis()+d;lcdLine0="";lcdLine1="";
  lcd.clear();lcd.setCursor(0,0);lcd.print(l0);lcd.setCursor(0,1);lcd.print(l1);
}
void updateLcdDisplay(){
  if(lcdTempActive){if(millis()<lcdTempUntil)return;lcdTempActive=false;}
  int fr=3-(pad1_occupied+pad2_occupied+pad3_occupied);
  lcdWrite("Pads Free:"+String(fr),"Pads Full:"+String(3-fr));
  currentStatusMessage="Free:"+String(fr)+" Charging:"+String(3-fr);
}

String notifMessages[8],notifTypes[8];
int notifHead=0,notifCount=0;
void pushNotif(String msg,String type="info"){
  int idx=(notifHead+notifCount)%8;
  notifMessages[idx]=msg;notifTypes[idx]=type;
  if(notifCount<8)notifCount++;else notifHead=(notifHead+1)%8;
}

struct Task{char type;String subject,body,extra1,extra2,extra3;};
const int TQS=12; Task taskQueue[TQS];
int taskHead=0,taskCount=0; bool taskRunning=false;

void enqueueEmail(String s,String b){
  if(taskCount>=TQS)return;
  taskQueue[(taskHead+taskCount)%TQS]={'E',s,b,"","",""};taskCount++;
}
void enqueueSheets(String ev,String p,String st,String bi,String ip){
  if(taskCount>=TQS)return;
  taskQueue[(taskHead+taskCount)%TQS]={'S',ev,p,st,bi,ip};taskCount++;
}

enum EmailState{ES_IDLE,ES_CONNECT,ES_GREET,ES_EHLO,ES_AUTH,ES_USER,ES_PASS,
                ES_FROM,ES_TO,ES_DATA,ES_HEADERS,ES_BODY,ES_QUIT,ES_DONE};
EmailState emailState=ES_IDLE;
unsigned long emailTimer=0;int ehloLines=0;
String pendingSubject,pendingBody;
BearSSL::WiFiClientSecure* smtpClient=nullptr;

String smtpRL(){
  String l="";unsigned long t=millis();
  while(smtpClient->available()&&(millis()-t<30)){char c=smtpClient->read();if(c=='\n')return l;if(c!='\r')l+=c;}
  return l;
}
void runEmailSM(){
  if(emailState==ES_IDLE)return;
  if(millis()-emailTimer>15000){pushNotif("Email timeout","error");if(smtpClient){smtpClient->stop();delete smtpClient;smtpClient=nullptr;}emailState=ES_IDLE;taskRunning=false;return;}
  switch(emailState){
    case ES_CONNECT:if(!smtpClient)smtpClient=new BearSSL::WiFiClientSecure();smtpClient->setInsecure();if(smtpClient->connect(smtpServer,smtpPort))emailState=ES_GREET;break;
    case ES_GREET:if(smtpClient->available()){smtpRL();smtpClient->println("EHLO esp8266");ehloLines=0;emailState=ES_EHLO;}break;
    case ES_EHLO:if(smtpClient->available()){smtpRL();if(++ehloLines>=8){smtpClient->println("AUTH LOGIN");emailState=ES_AUTH;}}break;
    case ES_AUTH:if(smtpClient->available()){smtpRL();smtpClient->println(base64::encode(senderEmail));emailState=ES_USER;}break;
    case ES_USER:if(smtpClient->available()){smtpRL();smtpClient->println(base64::encode(senderPass));emailState=ES_PASS;}break;
    case ES_PASS:if(smtpClient->available()){smtpRL();smtpClient->println("MAIL FROM:<"+String(senderEmail)+">");emailState=ES_FROM;}break;
    case ES_FROM:if(smtpClient->available()){smtpRL();smtpClient->println("RCPT TO:<"+String(receiverEmail)+">");emailState=ES_TO;}break;
    case ES_TO:if(smtpClient->available()){smtpRL();smtpClient->println("DATA");emailState=ES_DATA;}break;
    case ES_DATA:if(smtpClient->available()){smtpRL();smtpClient->println("From: EV <"+String(senderEmail)+">");smtpClient->println("To: "+String(receiverEmail));smtpClient->println("Subject: "+pendingSubject);smtpClient->println("MIME-Version: 1.0");smtpClient->println("Content-Type: text/plain");smtpClient->println();emailState=ES_HEADERS;}break;
    case ES_HEADERS:smtpClient->println(pendingBody);smtpClient->println(".");emailState=ES_BODY;break;
    case ES_BODY:if(smtpClient->available()){smtpRL();smtpClient->println("QUIT");emailState=ES_QUIT;}break;
    case ES_QUIT:if(smtpClient->available()){smtpRL();emailState=ES_DONE;}break;
    case ES_DONE:pushNotif("Email sent","success");smtpClient->stop();delete smtpClient;smtpClient=nullptr;emailState=ES_IDLE;taskRunning=false;break;
    default:break;
  }
}
void sendSheetsNow(String ev,String p,String st,String bi,String ip){
  BearSSL::WiFiClientSecure sc;sc.setInsecure();sc.setTimeout(4000);
  HTTPClient http;http.begin(sc,sheetsURL);http.addHeader("Content-Type","application/json");http.setTimeout(4000);
  http.POST("{\"event\":\""+ev+"\",\"pad\":\""+p+"\",\"status\":\""+st+"\",\"bill\":\""+bi+"\",\"ip\":\""+ip+"\"}");
  http.end();pushNotif(ev+" logged","info");taskRunning=false;
}
void runTaskQueue(){
  if(taskRunning){if(emailState!=ES_IDLE)runEmailSM();return;}
  if(taskCount==0)return;
  Task& t=taskQueue[taskHead];taskHead=(taskHead+1)%TQS;taskCount--;taskRunning=true;
  if(t.type=='E'){pendingSubject=t.subject;pendingBody=t.body;emailState=ES_CONNECT;emailTimer=millis();}
  else sendSheetsNow(t.subject,t.body,t.extra1,t.extra2,t.extra3);
}

const unsigned long DB=50;
bool lP1=false,lP2=false,lP3=false,lG=false,s1=false,s2=false,s3=false,sG=false;
unsigned long dt1=0,dt2=0,dt3=0,dtG=0;
bool debounce(bool raw,bool&last,bool&stable,unsigned long&ts){
  if(raw!=last){last=raw;ts=millis();}
  if((millis()-ts)>DB&&raw!=stable){stable=raw;return true;}
  return false;
}
bool gateIsOpen=false; unsigned long gateOpenUntil=0;
String localIP(){return WiFi.localIP().toString();}

void setup(){
  Serial.begin(115200);Wire.begin(D5,D6);
  lcd.begin(16,2);lcd.backlight();lcd.clear();
  lcd.setCursor(0,0);lcd.print("Welcome to EV");
  lcd.setCursor(0,1);lcd.print("Charging Station");
  lcdLine0="Welcome to EV   ";lcdLine1="Charging Station";
  delay(2000);connectToWiFi();
  pinMode(gateIrPin,INPUT_PULLUP);pinMode(pad1IrPin,INPUT_PULLUP);
  pinMode(pad2IrPin,INPUT_PULLUP);pinMode(pad3IrPin,INPUT_PULLUP);
  gateServo.attach(servoPin);gateServo.write(gateClosedAngle);
  setupRoutes();server.begin();
  enqueueEmail("EV Station Started","Online: http://"+localIP());
  enqueueSheets("SYSTEM_START","","All Free","",localIP());
  pushNotif("System ready","success");updateLcdDisplay();
}

void loop(){
  server.handleClient();runTaskQueue();updateLcdDisplay();
  if(gateIsOpen&&millis()>gateOpenUntil){gateServo.write(gateClosedAngle);gateIsOpen=false;}
  bool rG=(digitalRead(gateIrPin)==LOW),r1=(digitalRead(pad1IrPin)==LOW);
  bool r2=(digitalRead(pad2IrPin)==LOW),r3=(digitalRead(pad3IrPin)==LOW);

  if(debounce(r1,lP1,s1,dt1)){
    if(s1){pad1_occupied=true;pad1_startTime=millis();lcdTempMsg("Pad 1 Occupied","Charging...",3000);
      enqueueEmail("Pad 1 Occupied","Pad 1 occupied\r\nhttp://"+localIP());
      enqueueSheets("PAD_OCCUPIED","Pad 1","Occupied","",localIP());pushNotif("Pad 1 Occupied","warning");
    }else{
      pad1_occupied=false;unsigned long dur=(millis()-pad1_startTime)/1000;
      float bill=(dur/60.0)*costPerMinute;String bs="Rs."+String(bill,2);
      lastBillAmounts[0]=bill;lastDurations[0]=dur;
      lastBillMessages[0]="Pad 1: "+String(dur/60)+"m "+String(dur%60)+"s|"+bs;
      currentStatusMessage="Pad 1 Free. Bill: "+bs;lcdTempMsg("Thank You!","Pad 1 Free",3000);
      enqueueEmail("Pad 1 Free|"+bs,"Dur:"+String(dur/60)+"m "+String(dur%60)+"s Bill:"+bs);
      enqueueSheets("PAD_FREE","Pad 1","Free",bs,localIP());pushNotif("Pad 1 Free "+bs,"success");
    }
  }
  if(debounce(r2,lP2,s2,dt2)){
    if(s2){pad2_occupied=true;pad2_startTime=millis();lcdTempMsg("Pad 2 Occupied","Charging...",3000);
      enqueueEmail("Pad 2 Occupied","Pad 2 occupied\r\nhttp://"+localIP());
      enqueueSheets("PAD_OCCUPIED","Pad 2","Occupied","",localIP());pushNotif("Pad 2 Occupied","warning");
    }else{
      pad2_occupied=false;unsigned long dur=(millis()-pad2_startTime)/1000;
      float bill=(dur/60.0)*costPerMinute;String bs="Rs."+String(bill,2);
      lastBillAmounts[1]=bill;lastDurations[1]=dur;
      lastBillMessages[1]="Pad 2: "+String(dur/60)+"m "+String(dur%60)+"s|"+bs;
      currentStatusMessage="Pad 2 Free. Bill: "+bs;lcdTempMsg("Thank You!","Pad 2 Free",3000);
      enqueueEmail("Pad 2 Free|"+bs,"Dur:"+String(dur/60)+"m "+String(dur%60)+"s Bill:"+bs);
      enqueueSheets("PAD_FREE","Pad 2","Free",bs,localIP());pushNotif("Pad 2 Free "+bs,"success");
    }
  }
  if(debounce(r3,lP3,s3,dt3)){
    if(s3){pad3_occupied=true;pad3_startTime=millis();lcdTempMsg("Pad 3 Occupied","Charging...",3000);
      enqueueEmail("Pad 3 Occupied","Pad 3 occupied\r\nhttp://"+localIP());
      enqueueSheets("PAD_OCCUPIED","Pad 3","Occupied","",localIP());pushNotif("Pad 3 Occupied","warning");
    }else{
      pad3_occupied=false;unsigned long dur=(millis()-pad3_startTime)/1000;
      float bill=(dur/60.0)*costPerMinute;String bs="Rs."+String(bill,2);
      lastBillAmounts[2]=bill;lastDurations[2]=dur;
      lastBillMessages[2]="Pad 3: "+String(dur/60)+"m "+String(dur%60)+"s|"+bs;
      currentStatusMessage="Pad 3 Free. Bill: "+bs;lcdTempMsg("Thank You!","Pad 3 Free",3000);
      enqueueEmail("Pad 3 Free|"+bs,"Dur:"+String(dur/60)+"m "+String(dur%60)+"s Bill:"+bs);
      enqueueSheets("PAD_FREE","Pad 3","Free",bs,localIP());pushNotif("Pad 3 Free "+bs,"success");
    }
  }
  int occ=pad1_occupied+pad2_occupied+pad3_occupied;
  static bool wasFull=false;
  if(occ==3&&!wasFull){wasFull=true;currentStatusMessage="Station Full!";
    enqueueEmail("Station Full!","All 3 pads occupied. http://"+localIP());
    enqueueSheets("ALL_FULL","All","Full","",localIP());pushNotif("Station Full!","error");
  }
  if(occ<3)wasFull=false;
  if(debounce(rG,lG,sG,dtG)&&sG){
    if(occ<3&&!gateIsOpen){
      int fp=!pad1_occupied?1:(!pad2_occupied?2:3);
      currentStatusMessage="Welcome! Pad "+String(fp);
      lcdTempMsg("Welcome!","Pad: "+String(fp),4000);
      gateServo.write(gateOpenAngle);gateIsOpen=true;gateOpenUntil=millis()+4000;
    }else if(occ==3){currentStatusMessage="Station Full!";lcdTempMsg("Station Full!","Please wait",3000);}
  }
}

void connectToWiFi(){
  lcd.clear();lcd.setCursor(0,0);lcd.print("Connecting WiFi");
  lcdLine0="Connecting WiFi ";lcdLine1="                ";
  WiFi.begin(ssid,password);int d=0;
  while(WiFi.status()!=WL_CONNECTED){
    delay(500);lcd.setCursor(d%16,1);lcd.print(".");
    d++;if(d>15){lcd.setCursor(0,1);lcd.print("                ");d=0;}
  }
  String ip=WiFi.localIP().toString();
  lcd.clear();lcd.setCursor(0,0);lcd.print("Connected!");lcd.setCursor(0,1);lcd.print(ip);
  lcdLine0="Connected!      ";lcdLine1=ip;
  while(lcdLine1.length()<16)lcdLine1+=' ';
  currentStatusMessage="Connected! IP: "+ip;delay(2000);
}

// ── send chunk helper ──
void sc(const String& s){server.sendContent(s);}
void sc(const char* s){server.sendContent(s);}

// ════════════════════════════════════════════════════════════
//  MAIN PAGE
// ════════════════════════════════════════════════════════════
void handleRoot(){
  String ip=WiFi.localIP().toString();
  unsigned long now=millis();
  int occ=pad1_occupied+pad2_occupied+pad3_occupied;
  int fr=3-occ;
  unsigned long upSec=now/1000;
  float b0=lastBillAmounts[0],b1=lastBillAmounts[1],b2=lastBillAmounts[2];
  float mx=max(max(b0,b1),max(b2,0.1f));
  float totalRev=b0+b1+b2;
  int bp[3]={int((b0/mx)*100),int((b1/mx)*100),int((b2/mx)*100)};

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/html","");
  yield();

  // ── HEAD ──
  sc(F("<!DOCTYPE html><html lang='en'><head>"
  "<meta charset='UTF-8'>"
  "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'>"
  "<title>EV Charging Station</title>"
  "<link href='https://fonts.googleapis.com/css2?family=Plus+Jakarta+Sans:wght@400;500;600;700;800&family=JetBrains+Mono:wght@400;500&display=swap' rel='stylesheet'>"
  ));

  // ── CSS ──
  sc(F("<style>"
  ":root{"
    "--sky-50:#f0f9ff;--sky-100:#e0f2fe;--sky-200:#bae6fd;--sky-300:#7dd3fc;"
    "--sky-400:#38bdf8;--sky-500:#0ea5e9;--sky-600:#0284c7;--sky-700:#0369a1;--sky-800:#075985;"
    "--white:#ffffff;--slate-50:#f8fafc;--slate-700:#334155;--slate-900:#0f172a;"
    "--amber:#f59e0b;--green:#10b981;--rose:#f43f5e;--violet:#7c3aed;"
    "--card:rgba(255,255,255,0.82);--bdr:rgba(14,165,233,0.18);--bdr2:rgba(14,165,233,0.10);"
    "--txt:#0c2d48;--txt2:#2e6fa3;--txt3:#64b5d9;--txt4:#a8d8ef;"
    "--sh:0 1px 12px rgba(14,165,233,0.08),0 4px 20px rgba(14,165,233,0.06);"
    "--sh2:0 4px 24px rgba(14,165,233,0.14),0 8px 40px rgba(14,165,233,0.08);"
    "--r:20px;--r2:14px;--r3:10px}"
  "*{box-sizing:border-box;margin:0;padding:0}"
  "html{font-size:16px}"
  "body{font-family:'Plus Jakarta Sans',sans-serif;min-height:100vh;"
    "background:#daf0fd;"
    "background-image:"
      "radial-gradient(ellipse 80% 60% at 20% 10%,rgba(186,230,253,0.7) 0%,transparent 60%),"
      "radial-gradient(ellipse 60% 80% at 80% 90%,rgba(125,211,252,0.5) 0%,transparent 60%),"
      "radial-gradient(ellipse 50% 50% at 50% 50%,rgba(224,242,254,0.4) 0%,transparent 70%);"
    "color:var(--txt);overflow-x:hidden;padding-bottom:32px}"
  "::-webkit-scrollbar{width:6px;height:6px}"
  "::-webkit-scrollbar-track{background:transparent}"
  "::-webkit-scrollbar-thumb{background:rgba(14,165,233,0.25);border-radius:3px}"
  "::-webkit-scrollbar-thumb:hover{background:rgba(14,165,233,0.45)}"
  /* floating blobs */
  ".blobs{position:fixed;inset:0;pointer-events:none;z-index:0;overflow:hidden}"
  ".bl{position:absolute;border-radius:50%;filter:blur(80px)}"
  ".bl1{width:600px;height:600px;background:radial-gradient(circle,rgba(186,230,253,.5),transparent 70%);"
    "top:-200px;left:-150px;animation:fb 22s ease-in-out infinite}"
  ".bl2{width:500px;height:500px;background:radial-gradient(circle,rgba(125,211,252,.4),transparent 70%);"
    "bottom:-150px;right:-100px;animation:fb2 28s ease-in-out infinite}"
  ".bl3{width:350px;height:350px;background:radial-gradient(circle,rgba(224,242,254,.6),transparent 70%);"
    "top:45%;left:45%;animation:fb3 18s ease-in-out infinite}"
  "@keyframes fb{0%,100%{transform:translate(0,0) scale(1)}50%{transform:translate(30px,40px) scale(1.08)}}"
  "@keyframes fb2{0%,100%{transform:translate(0,0)}50%{transform:translate(-25px,-35px)}}"
  "@keyframes fb3{0%,100%{transform:translate(0,0)}33%{transform:translate(20px,-20px)}66%{transform:translate(-20px,10px)}}"
  /* layout */
  ".wrap{position:relative;z-index:1;display:flex;min-height:100vh}"
  /* sidebar */
  ".sb{width:240px;min-width:240px;padding:20px 14px;display:flex;flex-direction:column;gap:4px;"
    "position:sticky;top:0;height:100vh;overflow-y:auto;flex-shrink:0}"
  ".sb-card{background:var(--card);backdrop-filter:blur(20px);-webkit-backdrop-filter:blur(20px);"
    "border-radius:var(--r);border:1px solid var(--bdr);box-shadow:var(--sh2);"
    "padding:20px 16px;display:flex;flex-direction:column;gap:4px;flex:1}"
  ".logo{display:flex;align-items:center;gap:11px;padding-bottom:16px;"
    "border-bottom:1px solid var(--bdr2);margin-bottom:8px}"
  ".logo-ic{width:42px;height:42px;border-radius:13px;flex-shrink:0;"
    "background:linear-gradient(135deg,#38bdf8,#0369a1);"
    "display:flex;align-items:center;justify-content:center;font-size:20px;"
    "box-shadow:0 4px 14px rgba(3,105,161,.3)}"
  ".logo-txt{font-size:1rem;font-weight:800;color:var(--sky-700);line-height:1.15}"
  ".logo-sub{font-size:.6rem;font-weight:700;text-transform:uppercase;letter-spacing:.1em;color:var(--txt3)}"
  ".nav-sec{font-size:.6rem;font-weight:800;text-transform:uppercase;letter-spacing:.1em;"
    "color:var(--txt4);padding:10px 8px 3px}"
  ".nav-a{display:flex;align-items:center;gap:9px;padding:9px 10px;border-radius:var(--r3);"
    "font-size:.82rem;font-weight:700;color:var(--txt2);text-decoration:none;transition:all .2s;cursor:pointer}"
  ".nav-a:hover{background:rgba(14,165,233,.1);color:var(--sky-600);transform:translateX(2px)}"
  ".nav-a.on{background:linear-gradient(135deg,rgba(56,189,248,.18),rgba(14,165,233,.08));"
    "color:var(--sky-700);border:1px solid rgba(14,165,233,.2)}"
  ".nav-ico{width:30px;height:30px;border-radius:8px;display:flex;align-items:center;"
    "justify-content:center;font-size:13px;background:rgba(14,165,233,.1);flex-shrink:0;transition:all .2s}"
  ".nav-a.on .nav-ico{background:linear-gradient(135deg,var(--sky-400),var(--sky-600))}"
  ".sb-bot{margin-top:auto;background:linear-gradient(135deg,rgba(14,165,233,.08),rgba(56,189,248,.04));"
    "border:1px solid var(--bdr2);border-radius:var(--r3);padding:12px}"
  ".live-row{display:flex;align-items:center;gap:6px;font-size:.75rem;font-weight:700;color:var(--txt2)}"
  ".ldot{width:7px;height:7px;border-radius:50%;background:#10b981;flex-shrink:0;"
    "box-shadow:0 0 0 0 rgba(16,185,129,.5);animation:lp 1.8s ease infinite}"
  "@keyframes lp{0%,100%{box-shadow:0 0 0 0 rgba(16,185,129,.4)}60%{box-shadow:0 0 0 6px rgba(16,185,129,0)}}"
  ".sb-ip{font-family:'JetBrains Mono',monospace;font-size:.68rem;color:var(--txt3);margin-top:4px}"
  ".sb-up{font-size:.68rem;color:var(--txt3);margin-top:2px;font-weight:600}"
  /* main */
  ".main{flex:1;min-width:0;padding:20px 20px 20px 6px;display:flex;flex-direction:column;gap:16px}"
  /* topbar */
  ".tbar{background:var(--card);backdrop-filter:blur(18px);-webkit-backdrop-filter:blur(18px);"
    "border-radius:var(--r);border:1px solid var(--bdr);padding:14px 22px;"
    "display:flex;align-items:center;justify-content:space-between;box-shadow:var(--sh);"
    "animation:aFd .4s ease both}"
  "@keyframes aFd{from{opacity:0;transform:translateY(-8px)}to{opacity:1;transform:translateY(0)}}"
  ".tbar-t{font-size:1.05rem;font-weight:800;color:var(--txt)}"
  ".tbar-t em{color:var(--sky-500);font-style:normal}"
  ".tbar-r{display:flex;align-items:center;gap:8px}"
  ".chip{padding:5px 12px;border-radius:50px;font-size:.7rem;font-weight:800;"
    "display:inline-flex;align-items:center;gap:5px;white-space:nowrap}"
  ".chip-g{background:rgba(16,185,129,.1);color:#059669;border:1px solid rgba(16,185,129,.22)}"
  ".chip-b{background:rgba(14,165,233,.08);color:var(--sky-700);border:1px solid var(--bdr);"
    "font-family:'JetBrains Mono',monospace}"
  /* kpi grid */
  ".kgrid{display:grid;grid-template-columns:repeat(4,1fr);gap:14px;"
    "animation:aFu .4s .05s ease both}"
  "@keyframes aFu{from{opacity:0;transform:translateY(12px)}to{opacity:1;transform:translateY(0)}}"
  ".kcard{background:var(--card);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);"
    "border-radius:var(--r);border:1px solid var(--bdr);padding:18px 16px;"
    "box-shadow:var(--sh);transition:transform .25s,box-shadow .25s;position:relative;overflow:hidden;"
    "cursor:default}"
  ".kcard:hover{transform:translateY(-4px);box-shadow:var(--sh2)}"
  ".kcard::after{content:'';position:absolute;inset:0;border-radius:var(--r);"
    "background:linear-gradient(135deg,rgba(255,255,255,.5),transparent);pointer-events:none}"
  ".k-ico{width:38px;height:38px;border-radius:11px;display:flex;align-items:center;"
    "justify-content:center;font-size:17px;margin-bottom:12px;position:relative;z-index:1}"
  ".k-sky{background:linear-gradient(135deg,#e0f2fe,#bae6fd);box-shadow:0 2px 10px rgba(14,165,233,.2)}"
  ".k-amb{background:linear-gradient(135deg,#fef3c7,#fde68a);box-shadow:0 2px 10px rgba(245,158,11,.2)}"
  ".k-grn{background:linear-gradient(135deg,#d1fae5,#a7f3d0);box-shadow:0 2px 10px rgba(16,185,129,.2)}"
  ".k-vio{background:linear-gradient(135deg,#ede9fe,#ddd6fe);box-shadow:0 2px 10px rgba(124,58,237,.2)}"
  ".k-val{font-size:1.9rem;font-weight:900;color:var(--txt);line-height:1;position:relative;z-index:1}"
  ".k-lbl{font-size:.62rem;font-weight:800;text-transform:uppercase;letter-spacing:.08em;"
    "color:var(--txt3);margin-top:3px;position:relative;z-index:1}"
  ".k-sub{font-size:.7rem;color:var(--txt4);margin-top:1px;font-weight:600;position:relative;z-index:1}"
  ".k-bg{position:absolute;bottom:-10px;right:-6px;font-size:58px;opacity:.06;pointer-events:none}"
  /* mid row */
  ".mgrid{display:grid;grid-template-columns:220px 1fr;gap:14px;"
    "animation:aFu .4s .12s ease both}"
  /* donut */
  ".dcard{background:var(--card);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);"
    "border-radius:var(--r);border:1px solid var(--bdr);padding:20px 16px;"
    "box-shadow:var(--sh);display:flex;flex-direction:column;align-items:center}"
  ".sec-ttl{font-size:.62rem;font-weight:800;text-transform:uppercase;letter-spacing:.1em;"
    "color:var(--txt3);margin-bottom:14px;align-self:flex-start;"
    "display:flex;align-items:center;gap:5px}"
  ".sec-ttl::before{content:'';width:3px;height:12px;border-radius:2px;display:block;"
    "background:linear-gradient(to bottom,var(--sky-300),var(--sky-600))}"
  ".dring{position:relative;width:140px;height:140px}"
  ".dring svg{transform:rotate(-90deg)}"
  ".dc{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center}"
  ".dc-n{font-size:1.9rem;font-weight:900;color:var(--txt);line-height:1}"
  ".dc-l{font-size:.6rem;font-weight:800;text-transform:uppercase;letter-spacing:.06em;color:var(--txt3)}"
  ".dleg{display:flex;flex-direction:column;gap:7px;margin-top:14px;width:100%}"
  ".dleg-r{display:flex;align-items:center;justify-content:space-between;font-size:.74rem;font-weight:700}"
  ".dleg-l{display:flex;align-items:center;gap:6px;color:var(--txt2)}"
  ".dleg-dot{width:8px;height:8px;border-radius:2px}"
  ".dleg-v{font-weight:800;color:var(--txt);font-size:.74rem}"
  /* pad cards */
  ".pgrid{display:flex;flex-direction:column;gap:10px}"
  ".pcard{background:var(--card);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);"
    "border-radius:var(--r2);border:1px solid var(--bdr);padding:14px 18px;"
    "box-shadow:var(--sh);transition:all .3s;"
    "display:grid;grid-template-columns:auto 1fr auto;align-items:center;gap:14px}"
  ".pcard:hover{box-shadow:var(--sh2);transform:translateX(3px);border-color:rgba(56,189,248,.35)}"
  ".pcard.chg{border-color:rgba(245,158,11,.3);background:rgba(255,253,243,.85)}"
  ".pnum{width:40px;height:40px;border-radius:11px;display:flex;align-items:center;"
    "justify-content:center;font-size:.95rem;font-weight:900;color:#fff;flex-shrink:0;"
    "background:linear-gradient(135deg,var(--sky-300),var(--sky-600));"
    "box-shadow:0 3px 10px rgba(14,165,233,.28);transition:all .3s}"
  ".pnum.chg{background:linear-gradient(135deg,#fbbf24,#d97706);"
    "box-shadow:0 3px 14px rgba(245,158,11,.4);animation:pglow 2s ease infinite}"
  "@keyframes pglow{0%,100%{box-shadow:0 3px 10px rgba(245,158,11,.35)}50%{box-shadow:0 3px 20px rgba(245,158,11,.65)}}"
  ".pinf .pname{font-size:.85rem;font-weight:800;color:var(--txt)}"
  ".pinf .pdet{font-size:.7rem;color:var(--txt3);font-weight:600;margin-top:2px;font-family:'JetBrains Mono',monospace}"
  ".pbar{height:3px;border-radius:2px;background:rgba(125,211,252,.2);margin-top:6px;overflow:hidden}"
  ".pbar-f{height:100%;border-radius:2px;"
    "background:linear-gradient(90deg,var(--sky-300),var(--sky-500),#10b981);"
    "background-size:200% 100%;animation:pbscan 2s linear infinite}"
  "@keyframes pbscan{0%{background-position:200% 0}100%{background-position:-200% 0}}"
  ".pright{display:flex;flex-direction:column;align-items:flex-end;gap:4px;flex-shrink:0}"
  ".pill{padding:3px 9px;border-radius:50px;font-size:.62rem;font-weight:800;letter-spacing:.04em;white-space:nowrap}"
  ".pill-f{background:rgba(16,185,129,.1);color:#059669;border:1px solid rgba(16,185,129,.22)}"
  ".pill-c{background:rgba(245,158,11,.1);color:#b45309;border:1px solid rgba(245,158,11,.28);"
    "animation:pipc 1.6s ease infinite}"
  "@keyframes pipc{0%,100%{box-shadow:0 0 0 0 rgba(245,158,11,.25)}55%{box-shadow:0 0 0 4px rgba(245,158,11,0)}}"
  ".pbill{font-family:'JetBrains Mono',monospace;font-size:.72rem;font-weight:700;color:var(--sky-600)}"
  /* bottom grid */
  ".bgrid{display:grid;grid-template-columns:1fr 1fr;gap:14px;"
    "animation:aFu .4s .2s ease both}"
  ".bcard{background:var(--card);backdrop-filter:blur(16px);-webkit-backdrop-filter:blur(16px);"
    "border-radius:var(--r);border:1px solid var(--bdr);padding:20px 18px;box-shadow:var(--sh)}"
  /* bar chart */
  ".bchart{display:flex;align-items:flex-end;gap:10px;height:120px;margin-top:14px}"
  ".bcol{flex:1;display:flex;flex-direction:column;align-items:center;gap:4px;min-width:0}"
  ".bouter{width:100%;flex:1;display:flex;align-items:flex-end;background:rgba(125,211,252,.12);"
    "border-radius:8px 8px 0 0;overflow:hidden}"
  ".bfill{width:100%;border-radius:8px 8px 0 0;min-height:4px;position:relative;overflow:hidden;"
    "background:linear-gradient(to top,var(--sky-700),var(--sky-400));transition:height 1s ease}"
  ".bfill::after{content:'';position:absolute;inset:0;"
    "background:linear-gradient(90deg,transparent,rgba(255,255,255,.3),transparent);"
    "animation:bshine 2.5s ease infinite}"
  "@keyframes bshine{0%{transform:translateX(-100%)}100%{transform:translateX(100%)}}"
  ".bfill.chg{background:linear-gradient(to top,#d97706,#fbbf24)}"
  ".blbl{font-size:.65rem;font-weight:800;color:var(--txt3);font-family:'JetBrains Mono',monospace;text-align:center}"
  ".bamt{font-size:.65rem;font-weight:800;color:var(--sky-600);font-family:'JetBrains Mono',monospace}"
  ".baxis{display:flex;justify-content:space-around;border-top:1px solid rgba(125,211,252,.2);"
    "padding-top:6px;margin-top:6px}"
  ".bax{font-size:.6rem;font-weight:700;color:var(--txt4)}"
  ".stline{display:flex;align-items:center;gap:6px;margin-top:10px;padding:8px 10px;"
    "background:rgba(14,165,233,.06);border-radius:8px;border:1px solid var(--bdr2)}"
  ".stline-lbl{font-size:.65rem;font-weight:700;color:var(--txt3)}"
  ".stline-val{font-size:.65rem;font-weight:700;color:var(--sky-600)}"
  /* sessions */
  ".slist{display:flex;flex-direction:column;gap:8px;margin-top:14px}"
  ".sitem{display:flex;align-items:center;justify-content:space-between;"
    "padding:10px 12px;border-radius:var(--r3);background:rgba(14,165,233,.05);"
    "border:1px solid rgba(56,189,248,.15);transition:all .2s}"
  ".sitem:hover{background:rgba(14,165,233,.1);transform:translateX(2px)}"
  ".sleft{display:flex;align-items:center;gap:9px}"
  ".sico{width:30px;height:30px;border-radius:8px;flex-shrink:0;"
    "background:linear-gradient(135deg,#e0f2fe,#bae6fd);"
    "display:flex;align-items:center;justify-content:center;font-size:13px}"
  ".stxt{font-size:.78rem;font-weight:700;color:var(--txt)}"
  ".ssub{font-size:.64rem;color:var(--txt3);font-weight:600}"
  ".sbill{font-family:'JetBrains Mono',monospace;font-size:.78rem;font-weight:800;color:var(--sky-600)}"
  ".sright{display:flex;align-items:center;gap:7px}"
  ".cbtn{background:rgba(14,165,233,.1);color:var(--sky-600);border:1px solid rgba(14,165,233,.2);"
    "padding:3px 8px;border-radius:6px;font-size:.62rem;font-weight:800;"
    "cursor:pointer;text-decoration:none;transition:all .2s;white-space:nowrap}"
  ".cbtn:hover{background:rgba(14,165,233,.22)}"
  ".empty{text-align:center;padding:24px 10px;color:var(--txt4);font-size:.8rem;font-weight:600}"
  /* toasts */
  "#tc{position:fixed;top:16px;right:16px;z-index:9999;"
    "display:flex;flex-direction:column;gap:7px;max-width:290px;pointer-events:none}"
  ".toast{display:flex;align-items:flex-start;gap:9px;padding:11px 14px;border-radius:12px;"
    "background:rgba(255,255,255,.95);backdrop-filter:blur(16px);"
    "border:1px solid var(--bdr);box-shadow:var(--sh2);"
    "pointer-events:auto;font-size:.75rem;font-weight:600;"
    "animation:tin .3s cubic-bezier(.22,1,.36,1),tout .3s ease 3.7s forwards}"
  ".ts{border-left:3px solid #10b981}.tw{border-left:3px solid #f59e0b}"
  ".te{border-left:3px solid #f43f5e}.ti{border-left:3px solid var(--sky-500)}"
  ".t-ttl{font-size:.75rem;font-weight:800;color:var(--txt);margin-bottom:1px}"
  ".t-msg{font-size:.68rem;color:var(--txt2)}"
  "@keyframes tin{from{transform:translateX(110%);opacity:0}to{transform:translateX(0);opacity:1}}"
  "@keyframes tout{from{opacity:1}to{opacity:0;transform:translateX(16px)}}"
  /* RESPONSIVE */
  "@media(max-width:1100px){.kgrid{grid-template-columns:repeat(2,1fr)}}"
  "@media(max-width:900px){"
    ".sb{display:none}"
    ".main{padding:14px}"
    ".kgrid{grid-template-columns:repeat(2,1fr)}"
    ".mgrid{grid-template-columns:1fr}"
    ".dcard{flex-direction:row;align-items:flex-start;gap:20px}"
    ".bgrid{grid-template-columns:1fr}"
  "}"
  "@media(max-width:560px){"
    ".kgrid{grid-template-columns:1fr 1fr}"
    ".k-val{font-size:1.5rem}"
    ".tbar-t{font-size:.9rem}"
    ".bgrid{grid-template-columns:1fr}"
  "}"
  "@media(max-width:400px){.kgrid{grid-template-columns:1fr}}"
  "</style>"));
  yield();

  // ── JS ──
  sc(F("<script>"
  "const ico={success:'✅',warning:'⚠️',error:'🚨',info:'💧'};"
  "function sT(m,t){"
    "const c=document.getElementById('tc'),e=document.createElement('div');"
    "e.className='toast t'+(t==='info'?'i':t[0]);"
    "e.innerHTML='<span style=\"font-size:.9rem\">'+ico[t]+'</span>'"
      "+'<div><div class=\"t-ttl\">'+t+'</div><div class=\"t-msg\">'+m+'</div></div>';"
    "c.appendChild(e);"
    "setTimeout(()=>{e.style.opacity='0';e.style.transform='translateX(16px)';"
    "e.style.transition='all .3s';setTimeout(()=>e.remove(),300);},4000);}"
  "async function pN(){try{const d=await(await fetch('/api/notifications')).json();d.forEach(n=>sT(n.msg,n.type));}catch(e){}}"
  "async function uSt(){try{const d=await(await fetch('/api/system-status')).text();const e=document.getElementById('stxt');if(e)e.textContent=d;}catch(e){}}"
  "async function uBl(){try{const d=await(await fetch('/api/bills')).text();const e=document.getElementById('bl-in');if(e&&e.innerHTML!==d)e.innerHTML=d;}catch(e){}}"
  "async function uPs(){try{"
    "const d=await(await fetch('/status')).json();"
    "let o=0;"
    "['pad1','pad2','pad3'].forEach((p,i)=>{"
      "const el=document.getElementById('pc-'+p);if(!el)return;"
      "const nb=el.querySelector('.pnum'),pill=el.querySelector('.pill'),bw=el.querySelector('.pbar'),ks=document.getElementById('ks'+i);"
      "if(d[p+'_occupied']){"
        "o++;el.className='pcard chg';"
        "if(nb)nb.className='pnum chg';"
        "if(pill){pill.className='pill pill-c';pill.textContent='Charging';}"
        "if(bw&&!bw.querySelector('.pbar-f'))bw.innerHTML='<div class=\"pbar-f\"></div>';"
        "if(ks)ks.textContent='Charging';"
      "}else{"
        "el.className='pcard';"
        "if(nb)nb.className='pnum';"
        "if(pill){pill.className='pill pill-f';pill.textContent='Available';}"
        "if(bw)bw.innerHTML='';"
        "if(ks)ks.textContent='Available';"
      "}});"
    "const fe=document.getElementById('kfr'),oe=document.getElementById('kocc'),dn=document.getElementById('dcn');"
    "if(fe)fe.textContent=3-o;if(oe)oe.textContent=o;if(dn)dn.textContent=3-o;"
    "uDon(d);"
  "}catch(e){}}"
  "function uDon(d){"
    "const c=251.2,s=c/3,oc=[d.pad1_occupied,d.pad2_occupied,d.pad3_occupied];"
    "const cl=['#38bdf8','#0ea5e9','#0369a1'];"
    "document.querySelectorAll('.dseg').forEach((sg,i)=>{"
      "sg.setAttribute('stroke',oc[i]?'#f59e0b':cl[i]);"
      "sg.setAttribute('stroke-dasharray',s.toFixed(1)+' '+(c-s).toFixed(1));"
      "sg.setAttribute('stroke-dashoffset',(-(s*i)).toFixed(1));});}"
  "async function uTm(){try{"
    "const d=await(await fetch('/api/timers')).json();"
    "['pad1','pad2','pad3'].forEach(p=>{"
      "const te=document.getElementById('t-'+p),be=document.getElementById('b-'+p);"
      "if(te&&d[p])te.textContent=d[p].time+'  elapsed';"
      "if(be&&d[p])be.textContent='Rs.'+d[p].bill;"
    "});}catch(e){}}"
  "function uClk(){const n=new Date();"
    "const s=n.toLocaleTimeString('en-IN',{hour:'2-digit',minute:'2-digit',second:'2-digit'});"
    "const e=document.getElementById('clk');if(e)e.textContent=s;}"
  "let iv;"
  "function go(){uSt();uBl();uPs();uTm();pN();uClk();"
    "iv=setInterval(()=>{uSt();uBl();uPs();uTm();pN();uClk();},1500);}"
  "document.addEventListener('DOMContentLoaded',go);"
  "document.addEventListener('visibilitychange',()=>{if(document.hidden)clearInterval(iv);else go();});"
  "</script></head><body>"));
  yield();

  // ── BODY ──
  sc("<div class='blobs'><div class='bl bl1'></div><div class='bl bl2'></div><div class='bl bl3'></div></div>");
  sc("<div id='tc'></div>");
  sc("<div class='wrap'>");

  // SIDEBAR
  sc("<aside class='sb'><div class='sb-card'>");
  sc("<div class='logo'><div class='logo-ic'>&#9889;</div>"
     "<div><div class='logo-txt'>EV Station</div><div class='logo-sub'>Dashboard</div></div></div>");
  sc("<div class='nav-sec'>Navigation</div>");
  sc("<a class='nav-a on' href='/'><div class='nav-ico'>&#128202;</div>Dashboard</a>");
  sc("<a class='nav-a' href='/status'><div class='nav-ico'>&#128225;</div>API Status</a>");
  sc("<a class='nav-a' href='https://docs.google.com/spreadsheets/d/1h6nOv1JqRG0wRDfAeUf4oeaSesskI6LgHRsCG4g9xhY/edit?usp=sharing' target='_blank'><div class='nav-ico'>&#128203;</div>Sheets Log</a>");
  sc("<a class='nav-a' href='http://10.159.208.112/' target='_blank'><div class='nav-ico'>&#127760;</div>VAWT- Dashboard</a>");
  sc("<div class='nav-sec'>System</div>");
  sc("<div class='sb-bot'>");
  sc("<div class='live-row'><div class='ldot'></div>Connected &mdash; Live</div>");
  sc("<div class='sb-ip'>" + ip + "</div>");
  sc("<div class='sb-up'>Uptime: " + String(upSec/3600) + "h " + String((upSec%3600)/60) + "m " + String(upSec%60) + "s</div>");
  sc("</div></div></aside>");
  yield();

  // MAIN
  sc("<div class='main'>");

  // TOPBAR
  sc("<div class='tbar'>");
  sc("<div class='tbar-t'>&#9889; <em>Charging</em> Station Monitor</div>");
  sc("<div class='tbar-r'>");
  sc("<div class='chip chip-g'><span style='width:6px;height:6px;border-radius:50%;background:#10b981;display:inline-block;animation:lp 1.6s infinite'></span>Live</div>");
  sc("<div class='chip chip-b' id='clk'>--:--:--</div>");
  sc("</div></div>");

  // KPI
  sc("<div class='kgrid'>");
  sc("<div class='kcard'><div class='k-ico k-sky'>&#9889;</div>");
  sc("<div class='k-val' id='kfr'>" + String(fr) + "</div>");
  sc("<div class='k-lbl'>Pads Free</div><div class='k-sub'>Available now</div>");
  sc("<div class='k-bg'>&#9889;</div></div>");

  sc("<div class='kcard'><div class='k-ico k-amb'>&#128267;</div>");
  sc("<div class='k-val' id='kocc'>" + String(occ) + "</div>");
  sc("<div class='k-lbl'>Charging</div><div class='k-sub'>Active sessions</div>");
  sc("<div class='k-bg'>&#128267;</div></div>");

  sc("<div class='kcard'><div class='k-ico k-grn'>&#128176;</div>");
  sc("<div class='k-val' style='font-size:1.4rem'>Rs." + String(totalRev,1) + "</div>");
  sc("<div class='k-lbl'>Revenue</div><div class='k-sub'>Total earned</div>");
  sc("<div class='k-bg'>&#128176;</div></div>");

  sc("<div class='kcard'><div class='k-ico k-vio'>&#128246;</div>");
  sc("<div class='k-val' style='font-size:1.35rem'>" + String(WiFi.RSSI()) + "</div>");
  sc("<div class='k-lbl'>WiFi dBm</div><div class='k-sub'>" + String(ssid) + "</div>");
  sc("<div class='k-bg'>&#128246;</div></div>");
  sc("</div>"); // kgrid
  yield();

  // MID ROW — donut + pads
  sc("<div class='mgrid'>");

  // DONUT
  sc("<div class='dcard'><div class='sec-ttl'>Pad Occupancy</div>");
  sc("<div class='dring'>");
  sc("<svg width='140' height='140' viewBox='0 0 100 100'>");
  sc("<circle fill='none' stroke='#e0f2fe' stroke-width='14' cx='50' cy='50' r='40'/>");
  float sg=251.2f/3.0f;
  for(int i=0;i<3;i++){
    bool po=(i==0)?pad1_occupied:(i==1)?pad2_occupied:pad3_occupied;
    String cols[]={"#38bdf8","#0ea5e9","#0369a1"};
    sc("<circle class='dseg' fill='none' stroke-width='14' stroke-linecap='butt' cx='50' cy='50' r='40'");
    sc(" stroke='" + String(po?"#f59e0b":cols[i].c_str()) + "'");
    sc(" stroke-dasharray='" + String(sg,1) + " " + String(251.2f-sg,1) + "'");
    sc(" stroke-dashoffset='" + String(-sg*i,1) + "'/>");
  }
  sc("</svg>");
  sc("<div class='dc'><div class='dc-n' id='dcn'>" + String(fr) + "</div><div class='dc-l'>Free</div></div>");
  sc("</div>"); // dring
  sc("<div class='dleg'>");
  String dcols[]={"#38bdf8","#0ea5e9","#0369a1"};
  bool poccs[]={pad1_occupied,pad2_occupied,pad3_occupied};
  for(int i=0;i<3;i++){
    sc("<div class='dleg-r'><div class='dleg-l'>");
    sc("<div class='dleg-dot' style='background:" + dcols[i] + "'></div>Pad " + String(i+1));
    sc("</div><div class='dleg-v' id='ks" + String(i) + "'>" + String(poccs[i]?"Charging":"Available") + "</div></div>");
  }
  sc("</div></div>"); // dleg + dcard
  yield();

  // PAD CARDS
  sc("<div class='pgrid'>");
  for(int p=1;p<=3;p++){
    bool po=(p==1)?pad1_occupied:(p==2)?pad2_occupied:pad3_occupied;
    unsigned long pst=(p==1)?pad1_startTime:(p==2)?pad2_startTime:pad3_startTime;
    unsigned long pdur=po?(now-pst)/1000:0;
    float pbill=po?(pdur/60.0f)*costPerMinute:0;
    String pid="pad"+String(p);
    sc("<div class='pcard" + String(po?" chg":"") + "' id='pc-" + pid + "'>");
    sc("<div class='pnum" + String(po?" chg":"") + "'>" + String(p) + "</div>");
    sc("<div class='pinf'><div class='pname'>Charging Pad " + String(p) + "</div>");
    sc("<div class='pdet' id='t-" + pid + "'>");
    sc(po ? String(pdur/60)+"m "+String(pdur%60)+"s  elapsed" : "Ready to charge");
    sc("</div><div class='pbar'>");
    if(po) sc("<div class='pbar-f'></div>");
    sc("</div></div>");
    sc("<div class='pright'>");
    sc("<span class='pill " + String(po?"pill-c":"pill-f") + "'>" + String(po?"Charging":"Available") + "</span>");
    sc("<div class='pbill' id='b-" + pid + "'>" + String(po?"Rs."+String(pbill,2):"--") + "</div>");
    sc("</div></div>");
  }
  sc("</div>"); // pgrid
  sc("</div>"); // mgrid
  yield();

  // BOTTOM ROW
  sc("<div class='bgrid'>");

  // BAR CHART
  sc("<div class='bcard'><div class='sec-ttl'>Session Revenue</div>");
  sc("<div class='bchart'>");
  float bills[3]={b0,b1,b2};
  for(int i=0;i<3;i++){
    bool po=(i==0)?pad1_occupied:(i==1)?pad2_occupied:pad3_occupied;
    sc("<div class='bcol'>");
    sc("<div class='bamt'>Rs." + String(bills[i],1) + "</div>");
    sc("<div class='bouter'><div class='bfill" + String(po?" chg":"") + "' style='height:" + String(bp[i]) + "%'></div></div>");
    sc("<div class='blbl'>Pad " + String(i+1) + "</div>");
    sc("</div>");
  }
  sc("</div>"); // bchart
  sc("<div class='baxis'>");
  for(int i=0;i<3;i++){
    unsigned long dm=(i==0)?lastDurations[0]/60:(i==1)?lastDurations[1]/60:lastDurations[2]/60;
    sc("<div class='bax'>" + String(dm) + " min</div>");
  }
  sc("</div>");
  sc("<div class='stline'><span class='stline-lbl'>Status:</span><span class='stline-val' id='stxt'>" + currentStatusMessage + "</span></div>");
  sc("</div>"); // bcard
  yield();

  // SESSIONS
  sc("<div class='bcard'><div class='sec-ttl'>Recent Sessions</div>");
  sc("<div class='slist' id='bl-in'>");
  bool anyS=false;
  for(int i=0;i<3;i++){
    if(lastBillMessages[i]!=""){
      anyS=true;
      int bar=lastBillMessages[i].indexOf('|');
      String si=lastBillMessages[i].substring(0,bar);
      String ba=lastBillMessages[i].substring(bar+1);
      sc("<div class='sitem'><div class='sleft'><div class='sico'>&#9889;</div>");
      sc("<div><div class='stxt'>" + si + "</div></div></div>");
      sc("<div class='sright'><span class='sbill'>" + ba + "</span>");
      sc("<a href='/reset?pad=" + String(i+1) + "' class='cbtn'>&#10005; Clear</a>");
      sc("</div></div>");
    }
  }
  if(!anyS) sc("<div class='empty'>&#127774; No completed sessions yet</div>");
  sc("</div></div>"); // slist + bcard

  sc("</div>"); // bgrid
  sc("</div>"); // main
  sc("</div>"); // wrap
  sc("</body></html>");
  server.sendContent("");
}

// ── API HANDLERS ──
void handleNotifications(){
  String j="[";
  for(int i=0;i<notifCount;i++){int idx=(notifHead+i)%8;if(i>0)j+=",";j+="{\"msg\":\""+notifMessages[idx]+"\",\"type\":\""+notifTypes[idx]+"\"}";}
  j+="]";notifHead=0;notifCount=0;server.send(200,"application/json",j);
}
void handleSystemStatus(){server.send(200,"text/plain",currentStatusMessage);}
void handleBillsAPI(){
  String h="";bool any=false;
  for(int i=0;i<3;i++){
    if(lastBillMessages[i]!=""){
      any=true;int bar=lastBillMessages[i].indexOf('|');
      String si=lastBillMessages[i].substring(0,bar);
      String ba=lastBillMessages[i].substring(bar+1);
      h+="<div class='sitem'><div class='sleft'><div class='sico'>&#9889;</div>"
         "<div><div class='stxt'>"+si+"</div></div></div>"
         "<div class='sright'><span class='sbill'>"+ba+"</span>"
         "<a href='/reset?pad="+String(i+1)+"' class='cbtn'>&#10005; Clear</a>"
         "</div></div>";
    }
  }
  if(!any)h="<div class='empty'>&#127774; No completed sessions yet</div>";
  server.send(200,"text/html",h);
}
void handleTimersAPI(){
  unsigned long now=millis();String j="{";bool f=true;
  auto add=[&](String k,bool occ,unsigned long st){
    if(!occ)return;if(!f)j+=",";f=false;
    unsigned long dur=(now-st)/1000;float bill=(dur/60.0)*costPerMinute;
    j+="\""+k+"\":{\"time\":\""+String(dur/60)+"m "+String(dur%60)+"s\",\"bill\":\""+String(bill,2)+"\"}";
  };
  add("pad1",pad1_occupied,pad1_startTime);
  add("pad2",pad2_occupied,pad2_startTime);
  add("pad3",pad3_occupied,pad3_startTime);
  j+="}";server.send(200,"application/json",j);
}
void handleStatus(){
  server.send(200,"application/json",
    "{\"pad1_occupied\":"+String(pad1_occupied?"true":"false")+
    ",\"pad2_occupied\":"+String(pad2_occupied?"true":"false")+
    ",\"pad3_occupied\":"+String(pad3_occupied?"true":"false")+"}");
}
void handleReset(){
  String p=server.arg("pad");
  if(p=="1"){lastBillMessages[0]="";lastBillAmounts[0]=0;lastDurations[0]=0;}
  else if(p=="2"){lastBillMessages[1]="";lastBillAmounts[1]=0;lastDurations[1]=0;}
  else if(p=="3"){lastBillMessages[2]="";lastBillAmounts[2]=0;lastDurations[2]=0;}
  server.sendHeader("Location","/");server.send(302,"text/plain","");
}
void setupRoutes(){
  server.on("/",HTTP_GET,handleRoot);
  server.on("/reset",HTTP_GET,handleReset);
  server.on("/status",HTTP_GET,handleStatus);
  server.on("/api/system-status",HTTP_GET,handleSystemStatus);
  server.on("/api/bills",HTTP_GET,handleBillsAPI);
  server.on("/api/timers",HTTP_GET,handleTimersAPI);
  server.on("/api/notifications",HTTP_GET,handleNotifications);
}
