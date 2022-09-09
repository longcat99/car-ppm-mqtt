//-------------------------------------------------
// Created by 龙猫 on 2022/9/9.
//---------------------------------------------
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <ESPmDNS.h>      //用于设备域名 MDNS.begin("esp32")
#include <esp_wifi.h>     //用于esp_wifi_restore() 删除保存的wifi信息
#include <PubSubClient.h>        //另一个MQTT库
//-----------------------------------------------------------
//MQTT 代理
const char *mqtt_broker = "m.lijuan.wang";   //MQTT服务器域名
const char *topic = "/2458968/send";            //主题
const char *mqtt_username = "**********";       //用户名
const char *mqtt_password = "*********"; //密码
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


//---------------------------------------------------------------------- 
const int baudRate = 115200;               //设置波特率
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
      //---------------------------------------------------------                                               


      //**********************************************************************************
       
      //*****************************只写了一个舵机,其他同理自己写*****************************************************
     
           if ((cartime - mqtttime) <= 2 && car_t1 >= 900) {   //延迟大于两秒以及通道值不不正确不执行
            


             //-----------------------------------
             servoX.write(car_t1); //定义3508的转动速度的PWM脉宽
             }
           else if((cartime - mqtttime) > 2){
             servoX.write(1500); //定义3508的转动速度的PWM脉宽

             //-------------------------------------
              }  
         

  //***********************************************************************************        

      //联网后运行MQTT
          MQTT();
      
      }
}

