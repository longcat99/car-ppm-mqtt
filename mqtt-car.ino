//-------------------------------------------------
// Created by 龙猫 on 2022/9/9.
//---------------------------------------------
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESPmDNS.h>      //用于设备域名 MDNS.begin("esp32")
#include <esp_wifi.h>     //用于esp_wifi_restore() 删除保存的wifi信息
#include <HardwareSerial.h> 
HardwareSerial car1(1);  //硬件串口 HardwareSerial 有三个参数(0,1,2)分别对应 "serial" "serial1""serial2"
HardwareSerial car2(2);  //硬件串口 HardwareSerial 有三个参数(0,1,2)分别对应 "serial" "serial1""serial2"
#include <PubSubClient.h>        //另一个MQTT库
//-----------------------------------------------------------
//MQTT 代理
const char *mqtt_broker = "m.lijuan.wang";   //MQTT服务器域名
const char *topic = "/2458968/send";            //主题
const char *mqtt_username = "longcat";       //用户名
const char *mqtt_password = "juan5201314.."; //密码
const int mqtt_port = 1883;                  //端口号
WiFiClient espClient;
PubSubClient client(espClient);

#include <ArduinoJson.h>
//舵机库
#include <ESP32Servo.h>
#define OUT_X 15   //舵机pwm输出引脚
Servo servoX;                    //舵机名字
int pos ;

#define NTP1  "ntp1.aliyun.com"
#define NTP2  "ntp2.aliyun.com"
#define NTP3  "ntp3.aliyun.com"
const unsigned long interval = 300UL;//当前时间设置更新频率
unsigned long previousMillis = 0;


unsigned  long mqtttime;         //接收到的时间戳
unsigned  long cartime;          //本地时间戳
int car_t1;
int car_t2;
int car_t3;
int car_t4;
int car_t5;
int car_t6;
// ########################## 定义 ##########################
#define HOVER_SERIAL_BAUD   115200      // [-] HoverSerial 的波特率（用于与悬浮板通信）
#define SERIAL_BAUD         115200      // [-] 内置串口的波特率（用于串口监视器）
#define START_FRAME         0xABCD       // [-] 为可靠的串行通信启动框架定义
#define TIME_SEND           100         // [ms]发送时间间隔
#define SPEED_MAX_TEST      300         // [-] 测试的最大速度
// #define DEBUG_RX                        // [-] 调试接收到的数据。将所有字节打印到串行（注释掉以禁用）


// 串口带模式命令
typedef struct{
      uint16_t  start;
      int16_t   pitch;      // Angle
      int16_t   dPitch;     // Angle derivative
      int16_t   steer;       // RC Channel 1
      int16_t   speed;       // RC Channel 2
      uint16_t  sensors;    // RC开关和光学餐具柜传感器
      uint16_t  checksum;
    } SerialCommand;
    SerialCommand Command;

//串口反馈结构体
typedef struct{
   uint16_t start;
   int16_t  cmd1;
   int16_t  cmd2;
   int16_t  speedR_meas;
   int16_t  speedL_meas;
   int16_t  batVoltage;
   int16_t  boardTemp;
   uint16_t cmdLed;
   uint16_t checksum;
} SerialFeedback;
SerialFeedback Feedback;
SerialFeedback NewFeedback;

uint8_t idx = 0;                        //新数据指针的索引
uint16_t bufStartFrame;                 //缓冲起始帧
byte *p;                                //新接收数据的指针声明
byte incomingByte;
byte incomingBytePrev;

//---------------------------------------------------------------------- 
const int baudRate = 115200;               //设置波特率
const int car1txPin = 17;                  //串口car1发送引脚
const int car1rxPin = 16;                  //串口car1接收引脚
const int car2txPin = 23;                  //串口car2发送引脚
const int car2rxPin = 22;                  //串口car2接收引脚
const byte DNS_PORT = 53;                  //设置DNS端口号
const int webPort = 80;                    //设置Web端口号
const int resetPin = 0;                    //设置重置按键引脚,用于删除WiFi信息
const int LED = 2;                         //设置LED引脚
const char* AP_SSID  = "NodeMCU-ESP32";    //设置AP热点名称
//const char* AP_PASS  = "";               //设置AP热点密码
const char* HOST_NAME = "MY_ESP32S";       //设置设备名
String scanNetworksID = "";                //用于储存扫描到的WiFi ID
int connectTimeOut_s = 15;                 //WiFi连接超时时间，单位秒
IPAddress apIP(192, 168, 4, 1);            //设置AP的IP地址
String wifi_ssid = "";                     //暂时存储wifi账号密码
String wifi_pass = "";                     //暂时存储wifi账号密码
int carlog = 1;                         //1:打印接收到MQTT参数. 2:打印本地时间戳. 3:打印串口接收到的数据. 0:都不打印



//定义根目录首页网页HTML源代码
#define ROOT_HTML  "<!DOCTYPE html><html><head><title>WIFI Config by lwang</title><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"></head><style type=\"text/css\">.input{display: block; margin-top: 10px;}.input span{width: 100px; float: left; float: left; height: 36px; line-height: 36px;}.input input{height: 30px;width: 200px;}.btn{width: 120px; height: 35px; background-color: #000000; border:0px; color:#ffffff; margin-top:15px; margin-left:100px;}</style><body><form method=\"POST\" action=\"configwifi\"><label class=\"input\"><span>WiFi SSID</span><input type=\"text\" name=\"ssid\" value=\"\"></label><label class=\"input\"><span>WiFi PASS</span><input type=\"text\"  name=\"pass\"></label><input class=\"btn\" type=\"submit\" name=\"submit\" value=\"Submie\"> <p><span> Nearby wifi:</P></form>"
//定义成功页面HTML源代码
#define SUCCESS_HTML  "<html><body><font size=\"10\">successd,wifi connecting...<br />Please close this page manually.</font></body></html>"
 
DNSServer dnsServer;            //创建dnsServer实例
WebServer server(webPort);      //开启web服务, 创建TCP SERVER,参数: 端口号,最大连接数
 
//初始化AP模式
void initSoftAP(){
  WiFi.mode(WIFI_AP);     //配置为AP模式
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));   //设置AP热点IP和子网掩码
  if(WiFi.softAP(AP_SSID)){   //开启AP热点,如需要密码则添加第二个参数
    //打印相关信息
    Serial.println("ESP-32S SoftAP is right.");
    Serial.print("Soft-AP IP address = ");
    Serial.println(WiFi.softAPIP());
    Serial.println(String("MAC address = ")  + WiFi.softAPmacAddress().c_str());
  }else{  //开启热点失败
    Serial.println("开启热点失败");
    delay(1000);
    Serial.println("现在重启...");
    ESP.restart();  //重启复位esp32
  }
}
 
//初始化DNS服务器
void initDNS(){ 
  //判断将所有地址映射到esp32的ip上是否成功
  if(dnsServer.start(DNS_PORT, "*", apIP)){ 
    Serial.println("启动dnsserver成功。");
  }else{
    Serial.println("启动 dnsserver 失败。");
  }
}
 
//初始化WebServer
void initWebServer(){
  //给设备设定域名esp32,完整的域名是esp32.local ??
  if(MDNS.begin("esp32")){
    Serial.println("MDNS 响应程序已启动");
  }
 //必须添加第二个参数HTTP_GET，以下面这种格式去写，否则无法强制门户
  server.on("/", HTTP_GET, handleRoot);                      //  当浏览器请求服务器根目录(网站首页)时调用自定义函数handleRoot处理，设置主页回调函数，必须添加第二个参数HTTP_GET，否则无法强制门户
  server.on("/configwifi", HTTP_POST, handleConfigWifi);     //  当浏览器请求服务器/configwifi(表单字段)目录时调用自定义函数handleConfigWifi处理
  server.onNotFound(handleNotFound);                         //当浏览器请求的网络资源无法在服务器找到时调用自定义函数handleNotFound处理 
  //Tells the server to begin listening for incoming connections.Returns None
  server.begin();                                           //启动TCP SERVER
//server.setNoDelay(true);                                  //关闭延时发送
  Serial.println("网络服务器启动！");
}
 
//扫描WiFi
bool scanWiFi(){
    Serial.println("扫描开始");
  // 扫描附近WiFi
  int n = WiFi.scanNetworks();
  Serial.println("扫描完成");
  if (n == 0) {
    Serial.println("未找到网络");
    scanNetworksID = "未找到网络";
    return false;
  }else{
    Serial.print(n);
    Serial.println("找到的网络");
    for (int i = 0; i < n; ++i) {
          //打印找到的每个网络的 SSID 和 RSSI
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      scanNetworksID += "<P>" + WiFi.SSID(i) + "</P>";
      delay(10);
    }
    return true;
  }
}
 
void connectToWiFi(int timeOut_s){
      Serial.println("进入connectToWiFi()函数");
      //设置为STA模式并连接WIFI
      WiFi.mode(WIFI_STA);
      WiFi.setSleep(false);  //关闭STA模式下wifi休眠，提高响应速度
      WiFi.setAutoConnect(true);//设置自动连接
      //用字符串成员函数c_str()生成一个const char*指针，指向以空字符终止的数组,即获取该字符串的指针。
      if(wifi_ssid !=""){
          Serial.println("用web配置信息连接.");
          
          WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
          wifi_ssid = "";
          wifi_pass = "";
        }else{
           Serial.println("用nvs保存的信息连接.");
           WiFi.begin();//连接上一次连接成功的wifi
        }
      //WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
      int Connect_time = 0; //用于连接计时，如果长时间连接不成功，复位设备
      while (WiFi.status() != WL_CONNECTED) {  //等待WIFI连接成功
        Serial.print(".");
        digitalWrite(LED,!digitalRead(LED));
        delay(500);
        Connect_time ++;
        if (Connect_time > 2*timeOut_s) {  //长时间连接不上，重新进入配网页面
          digitalWrite(LED,LOW);
          Serial.println("");
          Serial.println("WIFI自动连接失败，现在启动AP进行网络配置...");
          wifiConfig();   //转到网页端手动配置wifi
          return;         //跳出 防止无限初始化
          //break;        //跳出 防止无限初始化
        }
      }
      if(WiFi.status() == WL_CONNECTED){
          Serial.println("WIFI连接成功");
          Serial.printf("SSID:%s", WiFi.SSID().c_str());
          Serial.printf(", PSW:%s\r\n", WiFi.psk().c_str());
          Serial.print("LocalIP:");
          Serial.print(WiFi.localIP());
          Serial.print(" ,GateIP:");
          Serial.println(WiFi.gatewayIP());
          Serial.print("WIFI状态为：");
          Serial.print(WiFi.status());
          digitalWrite(LED,HIGH);      
          server.stop();
          
      }
}
 
//用于配置WiFi
void wifiConfig(){
  initSoftAP();
  initDNS();
  initWebServer();
  scanWiFi();
}
 
//处理网站根目录“/”(首页)的访问请求,将显示配置wifi的HTML页面
void handleRoot(){
  if (server.hasArg("selectSSID")){
      server.send(200, "text/html", ROOT_HTML + scanNetworksID + "</body></html>");
    }else{
      server.send(200, "text/html", ROOT_HTML + scanNetworksID + "</body></html>");
    }     
}
 
void handleConfigWifi(){
      //返回http状态
      //server.send(200, "text/html", SUCCESS_HTML);
      if (server.hasArg("ssid")) {//判断是否有账号参数
        Serial.print("got ssid:");
        wifi_ssid = server.arg("ssid");      //获取html表单输入框name名为"ssid"的内容
        // strcpy(sta_ssid, server.arg("ssid").c_str());//将账号参数拷贝到sta_ssid中
        Serial.println(wifi_ssid);
      }else{//没有参数
        Serial.println("错误，未找到 ssid");
        server.send(200, "text/html", "<meta charset='UTF-8'>error, not found ssid");//返回错误页面
        return;
      }
      //密码与账号同理
      if (server.hasArg("pass")) {
        Serial.print("got password:");
        wifi_pass = server.arg("pass");       //获取html表单输入框name名为"pwd"的内容
        //strcpy(sta_pass, server.arg("pass").c_str());
        Serial.println(wifi_pass);
      }else{
        Serial.println("错误，找不到密码");
        server.send(200, "text/html", "<meta charset='UTF-8'>error, not found password");
        return;
      }
      server.send(200, "text/html", "<meta charset='UTF-8'>SSID："+wifi_ssid+"<br />password:"+wifi_pass+"<br />已取得WiFi信息,正在尝试连接,请手动关闭此页面。");//返回保存成功页面      
      delay(2000);    
      WiFi.softAPdisconnect(true);     //参数设置为true，设备将直接关闭接入点模式，即关闭设备所建立的WiFi网络。 
      server.close();                  //关闭web服务       
      WiFi.softAPdisconnect();         //在不输入参数的情况下调用该函数,将关闭接入点模式,并将当前配置的AP热点网络名和密码设置为空值.    
      Serial.println("WiFi Connect SSID:" + wifi_ssid + "  PASS:" + wifi_pass);   
      if(WiFi.status() != WL_CONNECTED){
        Serial.println("开始调用连接函数connectToWiFi()..");
        connectToWiFi(connectTimeOut_s);
      }else{
        Serial.println("提交的配置信息自动连接成功..");
      }    
}
 
// 设置处理404情况的函数'handleNotFound'
void handleNotFound(){            // 当浏览器请求的网络资源无法在服务器找到时通过此自定义函数处理
     handleRoot();                 //访问不存在目录则返回配置页面
//   server.send(404, "text/plain", "404: Not found");   
}
 
//LED闪烁,led为脚号,n为次数,t为时间间隔ms
void blinkLED(int led,int n,int t){
  for(int i=0;i<2*n;i++){
     digitalWrite(led,!digitalRead(led));
     delay(t);
   }   
 }
 
//删除保存的wifi信息,并使LED闪烁5次
void restoreWiFi(){
       delay(500);
       esp_wifi_restore();  //删除保存的wifi信息
       Serial.println("连接信息已清空,准备重启设备..");
       delay(10);           
       blinkLED(LED,5,500); //LED闪烁5次
       digitalWrite(LED,LOW);
  }
 
void checkConnect(bool reConnect){
    if(WiFi.status() != WL_CONNECTED){
      //  Serial.println("WIFI未连接.");
      //  Serial.println(WiFi.status()); 
        if(digitalRead(LED) != LOW){
          digitalWrite(LED,LOW);
        }    
        if(reConnect == true && WiFi.getMode() != WIFI_AP && WiFi.getMode() != WIFI_AP_STA ){
            Serial.println("WIFI未连接.");
            Serial.println("WiFi Mode:");
            Serial.println(WiFi.getMode());
            Serial.println("正在连接WiFi...");
            connectToWiFi(connectTimeOut_s);
        }  
    }else if(digitalRead(LED) != HIGH){
        digitalWrite(LED,HIGH);
    }
  }
//--------------------------------------------------------------------------------------------------
void MQTT(){
    //MQTT客户端开启
   //***************************************************
 client.setServer(mqtt_broker, mqtt_port);

 while (!client.connected()) {
     String client_id = "esp32-client-";
     client_id += String(WiFi.macAddress());
     Serial.printf("客户端%s连接到公共 mqtt 代理\n", client_id.c_str());
     if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
         Serial.println("公共 emqx mqtt broker 已连接");
     } else {
         Serial.print("状态失败");
         Serial.print(client.state());
         delay(2000);
     }    
     //发布和订阅
     client.publish(topic, "这是来自龙猫的ESP32的消息^^");
     client.subscribe(topic);
     //client.setCallback(callback);
     
//****************************************************
//****************************************************         

                        }
}


 //  用于平衡车主板串口控制
void Send(int16_t steer,
          int16_t speed,
          /*uint16_t sensors,*/
          uint8_t dualinput, /*双通道*/
          uint8_t ctrltype, /*控制类型*/
          uint8_t modetype, /*控制模式*/
          uint8_t field /*弱磁*/)
{
  // 创建命令
  Command.start    = (uint16_t)START_FRAME;
  Command.pitch    = 0;
  Command.dPitch   = 0;
  Command.steer    = steer;
  Command.speed    = speed;
  Command.sensors  = (uint16_t) dualinput  | ctrltype << 1 | modetype << 3 | field << 5;
  Command.sensors  = Command.sensors << 8;
  Command.checksum = (uint16_t) (Command.start ^ Command.pitch ^ Command.dPitch ^ Command.steer ^ Command.speed ^ Command.sensors);   //效验和

  // 写入串行
  car1.write((uint8_t *) &Command, sizeof(Command)); 
  car2.write((uint8_t *) &Command, sizeof(Command));  //如果四驱就使用第二个串口
//------------------------------------------------------------------------------------------

  //MQTT发布
//client.publish(topic,String(carsend).c_str());
  
  //cartx.print("long"); //串口打印测试
  //terminalV9.write((uint8_t *) &Command, sizeof(Command));  //虚拟打印9通道测试
  /*
  char temp[15];
  memcpy(temp, (uint8_t *) &Command, sizeof(Command));
  client.publish_P("longcat/carsend",(const uint8_t*)temp,14,false);
  */
}

// JSON回调函数
void callback(char* topic, byte* payload, unsigned int length) {
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);
//像往常一样使用 JsonDocument...
//DeserializationError error = deserializeJson(doc,payload);

//Serial.print("消息到达 [");
//  Serial.print(topic);
//  Serial.print("] ");

/*if (error) {
  Serial.print("反序列化Json() 失败：");
  Serial.println(error.c_str());
  return;
}
Serial.print("反序列化Json");
*/


car_t1 = doc["car_t1"]; // 1511
  
car_t2 = doc["car_t2"]; // 1511
  
car_t3 = doc["car_t3"]; // 1511
  
car_t4 = doc["car_t4"]; // 1512
  
car_t5 = doc["car_t5"]; // 1511
  
car_t6 = doc["car_t6"]; // 1511 

mqtttime = doc["time"];

if (carlog == 1)      
{
  Serial.print("car_t1=");
  Serial.print(car_t1);  
  Serial.print("   car_t2=");
  Serial.print(car_t2); 
  Serial.print("   car_t3=");
  Serial.print(car_t3); 
  Serial.print("   car_t4=");
  Serial.print(car_t4);
  Serial.print("   car_t5=");
  Serial.print(car_t5);
  Serial.print("   car_t6=");
  Serial.print(car_t6);
  Serial.print("   mqtttime=");
  Serial.print(mqtttime);
  Serial.println("");
}                 
  
  
}

// ########################## 平衡车主板反馈接收 ##########################
void Receive()
{
    // 检查串行缓冲区中是否有新数据可用
    if (car2.available()) {
        incomingByte     = car2.read();                                   // 读取传入字节
        bufStartFrame = ((uint16_t)(incomingByte) << 8) | incomingBytePrev;       // 构建起始帧
    }
    else {
        return;
    }

  // 如果定义了DEBUG_RX，则打印所有传入字节
  #ifdef DEBUG_RX
        Serial.print(incomingByte);
        return;
    #endif

    // 复制收到的数据
    if (bufStartFrame == START_FRAME) {                     // 如果检测到新数据，则初始化
        p       = (byte *)&NewFeedback;
        *p++    = incomingBytePrev;
        *p++    = incomingByte;
        idx     = 2;  
    } else if (idx >= 2 && idx < sizeof(SerialFeedback)) {  // 保存新收到的数据
        *p++    = incomingByte; 
        idx++;
    } 
    
    // 检查我们是否到达包裹的末端
    if (idx == sizeof(SerialFeedback)) {
        uint16_t checksum;
        checksum = (uint16_t)(NewFeedback.start ^ NewFeedback.cmd1 ^ NewFeedback.cmd2 ^ NewFeedback.speedR_meas ^ NewFeedback.speedL_meas
                            ^ NewFeedback.batVoltage ^ NewFeedback.boardTemp ^ NewFeedback.cmdLed);

        // 检查新数据的有效性
        if (NewFeedback.start == START_FRAME && checksum == NewFeedback.checksum) {
            // 复制新数据
            memcpy(&Feedback, &NewFeedback, sizeof(SerialFeedback));
            

            if (carlog = 3)
            {
              // 将数据打印到内置串口
            Serial.print("cmd1: ");   Serial.print(Feedback.cmd1);
            Serial.print(" cmd2: ");  Serial.print(Feedback.cmd2);
            Serial.print(" 速度左: ");  Serial.print(Feedback.speedR_meas);
            Serial.print(" 速度右: ");  Serial.print(Feedback.speedL_meas);
            Serial.print(" 电压: ");  Serial.print(Feedback.batVoltage);
            Serial.print(" 温度: ");  Serial.print(Feedback.boardTemp);
            Serial.print(" LED: ");  Serial.println(Feedback.cmdLed);
            } 
            }else {
            Serial.println("跳过无效数据");
            }
          
            
            
        idx = 0;    // 重置索引（防止在下一个循环中进入此if条件）
    }

    // 更新以前的状态
    incomingBytePrev = incomingByte;
}

void setup() {
  pinMode(LED,OUTPUT);                  //配置LED口为输出口
  digitalWrite(LED,LOW);                //初始灯灭


  servoX.attach(OUT_X, 1000, 2000);    // 設定pwm的接腳             //初始化舵机


  pinMode(resetPin, INPUT_PULLUP);      //按键上拉输入模式(默认高电平输入,按下时下拉接到低电平)


  Serial.begin(baudRate);
  //波特率115200
  //数据位8,无校验,停止位1
  //RX引脚号
  //TX引脚号
  configTime(0, 0, NTP1, NTP2, NTP3);  //NTP时间客户端开始

  car1.begin(115200,SERIAL_8N1,car1rxPin,car1txPin);         //串口初始化 RX 16  ,TX 17
  car2.begin(115200,SERIAL_8N1,car2rxPin,car2txPin);         //串口初始化 RX 22  ,TX 23 如果四驱就使用第二个串口
  WiFi.hostname(HOST_NAME);             //设置设备名
  client.setCallback(callback);
  
  connectToWiFi(connectTimeOut_s);
  
}
 
void loop() {
      //长按5秒(P0)清除网络配置信息
    if(!digitalRead(resetPin)){
        delay(5000);
        if(!digitalRead(resetPin)){   
           Serial.println("\n按键已长按5秒,正在清空网络连保存接信息.");  
           restoreWiFi();    //删除保存的wifi信息 
           ESP.restart();    //重启复位esp32
           Serial.println("已重启设备.");
        }      
     }
    
    dnsServer.processNextRequest();   //检查客户端DNS请求
    server.handleClient();            //检查客户端(浏览器)http请求
    checkConnect(true);               //检测网络连接状态，参数true表示如果断开重新连接
    delay(30);
    client.loop();
    if(WiFi.status() == WL_CONNECTED){

      //----------------------------------------------
      //获取当前时间戳
        unsigned long currentMillis = millis();
      if (currentMillis - previousMillis >= interval) {
      struct tm timeInfo; //声明一个结构体
      if (!getLocalTime(&timeInfo))
        { //一定要加这个条件判断，否则内存溢出
          Serial.println("获取时间失败");
        }
      time_t now = time(nullptr);
      cartime = now;
      
      //Serial.println(&timeInfo, "%F %T %A"); 

      if (carlog == 2)
      {
        Serial.print("当前时间戳"); 
        Serial.println(cartime); 
      }
      
      
      previousMillis = currentMillis;
      //--------------------------------------------------------
                                                     }
     Receive();                        //读取串口反馈数据
      //---------------------------------------------------------                                               

        int sp;
        int st;
        int ctrltype;
        int modetype;
        int rcppm = map((car_t4), 977, 2045, 0, 2);

      //**********************************************************************************
       
      //**********************************************************************************
      if (rcppm >= 2 )
         {
           if ((cartime - mqtttime) <= 2 && car_t1 >= 900) {   //延迟大于两秒以及通道值不不正确不执行
             st = (map((car_t1), 978, 2045, -200, 200)) * (map((car_t3), 999, 2045, 2, 5)); 
             //反转正负,遥控器和app正反保持一致
             // (abs(st)>=0)?(st=-st):(st=abs(st));
             servoX.write(car_t1); //定义3508的转动速度的PWM脉宽
             Serial.println(car_t1);
             }
           else if((cartime - mqtttime) > 2){
             st = 0;
             servoX.write(1500); //定义3508的转动速度的PWM脉宽
             Serial.println(car_t1);
              }  
         }

  //***********************************************************************************        
       if (rcppm >= 2)
         {
           if ((cartime - mqtttime) <= 2 && car_t2 >= 900) {
             sp =  (map((car_t2), 978, 2032, -200, 200)) * (map((car_t3), 999, 2045, 0, 5));
             //反转正负,遥控器和app正反保持一致
             (abs(sp)>=0)?(sp=-sp):(sp=abs(sp));}
           else if((cartime - mqtttime) > 2){
             sp = 0; }         
         }
  //***********************************************************************************        
        if (rcppm >= 2)
         {
           if ((cartime - mqtttime) <= 2 && car_t5 >= 900) {
             ctrltype = map((car_t5), 978, 2045, 0, 2);}   
           else if ((cartime - mqtttime) > 2){
             ctrltype = 0; }  
         }   
  //************************************************************************************
         if (rcppm >= 2)
         {
           if ((cartime - mqtttime) <= 2 && car_t6 >= 900) {
             modetype = map((car_t6), 978, 2045, 0, 2);}
           else if((cartime - mqtttime) > 2){
             modetype = 1; }     //等于1电压模式会刹车  
         }
  //**************************************************************************************        
//**************************************************************************************  
  int steer;   
  int speed;   
 
  steer = map((st), -1000, 1000, -1000, 1000); // 用于转向的 PPM 通道 1
  speed = map((sp), -1000, 1000, -1000, 1000); // 速度的 PPM 通道 2
  
  int dualinput = 0;
  
  
  int field = 0;
  // int sensors  = dualinput  0 | ctrltype << 1 | modetype << 3 | field << 5;
      Send(steer, speed,dualinput, ctrltype, modetype, field);
      delay(30);

      //联网后运行MQTT
          MQTT();
      
      }
}

