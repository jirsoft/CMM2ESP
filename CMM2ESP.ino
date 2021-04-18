#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Time.h>
#include <TimeLib.h>
#include <ESP8266Ping.h>


const String ESPversion = "0.66";

const long serTimeout = 5000; //5s serial input timeout
const long udpTimeout = 5000; //5s UDP input timeout
char serByte;
char serMode = 0; // 0 = command mode, 1 = file mode
String serCommand = "";

const long utcOffsetInSeconds = 3600;
const int udpPacketLength = 8002;

time_t CURRENT_TIME = now();
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds, 60000);

unsigned int udpPort = 34701;
unsigned int tcpPort = 34702;
unsigned int dbgPort = 34703;
IPAddress udpIP;
boolean NCwifiServerFound = false;

WiFiUDP udp;
char udpRecBuffer[udpPacketLength + 1];
char udpSendBuffer[udpPacketLength + 1];
int udpSize;

WiFiServer server(tcpPort);


void setup() {
  Serial.begin(691200);
  serOut("\nBooting");
  serOut("ESP ready");
  
  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("CMM2ESP");
  
  // No authentication by default
  // ArduinoOTA.setPassword("ESP");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    serOut("OTA started (" + type + ")");
  });
  ArduinoOTA.onEnd([]() {
    serOut("\nOTA finished");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  Serial.setTimeout(serTimeout);
  WiFi.disconnect();
}

String getTime()
{
  String h = String(hour(CURRENT_TIME));
  if (h.length() < 2)
    h = "0" + h;
  String m = String(minute(CURRENT_TIME));
  if (m.length() < 2)
    m = "0" + m;
  String s = String(second(CURRENT_TIME));
  if (s.length() < 2)
    s = "0" + s;

  return(h + ":" + m + ":" + s);
}

String getDate()
{
  String m = String(month(CURRENT_TIME));
  if (m.length() < 2)
    m = "0" + m;
  String d = String(day(CURRENT_TIME));
  if (d.length() < 2)
    d = "0" + d;
  String y = String(year(CURRENT_TIME));
  
  return(y + "-" + m + "-" + d);
}

void serOut(String s)
{
  Serial.print(s + '\n');
}   

void speedTest()
{
  long dataLen = random(50000, 150000);
  long daLe = dataLen;
  long crb, crc = 0;
  long partLen = 250;
  int partNum = dataLen / partLen, partRem = dataLen % partLen;
  byte buf[partLen];
  for (int i = 0; i < partLen; i++)
  {
    buf[i] = random(0, 256);
    crc = (crc + buf[i]) & 0xFF;
  }
  crb = crc;
  crc = 0;
  
  unsigned long tim = millis(); 
  serOut(String(dataLen) + "," + String(partLen) + "," + String(partNum) + "," + String(partRem));
  int part = 0;
  while (dataLen >= partLen)
  {
    Serial.write(buf, partLen);
    dataLen -= partLen;
    crc = (crc + crb) & 0xFF;
    if(!Serial.readStringUntil('\n').equalsIgnoreCase("OK"))
      return;
  }
  if (dataLen > 0)
  {
    Serial.write(buf, dataLen);
    for (int i = 0; i < dataLen; i++)
      crc = (crc + buf[i]) & 0xFF;
    if(!Serial.readStringUntil('\n').equalsIgnoreCase("OK"))
      return;
  }
  tim = millis() - tim;
  double spd = 1000 * daLe / 1024 / tim;
  serOut("Write test OK, " + String(daLe) + " bytes written, speed = " + String(spd, 2) + "kB/s, CRC8 = " + String(crc));
  tim = millis();
  daLe = Serial.readStringUntil('\n').toInt();
  dataLen = daLe;
  partLen = 1250;
  byte buf2[partLen];
  while (dataLen > 0)
  {
    dataLen -= Serial.readBytes(buf2, min(partLen, dataLen));
  }
  tim = millis() - tim;
  spd = 1000 * daLe / 1024 / tim;
  serOut("Read test OK, " + String(daLe) + " bytes read, speed = " + String(spd, 2) + "kB/s");  
}        

void udpOut(String s)
{
  if (NCwifiServerFound)
  {
    s.toCharArray(udpSendBuffer, s.length());
    udpSendBuffer[s.length() + 1] = 0;
    udp.beginPacket(udpIP, udpPort);
    udp.write(udpSendBuffer, s.length());
    udp.endPacket();
  }
}

void udpDebug(String s)
{
  s.toCharArray(udpSendBuffer, s.length());
  udpSendBuffer[s.length() + 1] = 0;
  udp.beginPacket(udpIP, dbgPort);
  udp.write(udpSendBuffer, s.length());
  udp.endPacket();
}

String getUdpString()
{
  udpSize = 0;
  unsigned long timeOut = millis();
  String received = "";
  
  while (!udpSize and (millis() - timeOut) < udpTimeout)
    udpSize = udp.parsePacket();
  if (udpSize)
  {
    int n = udp.read(udpRecBuffer, udpPacketLength);
    
    received = String(udpRecBuffer);
    received.remove(n);
  }
  return(received);  
}

void testUdpPacket()
{
  udpSize = udp.parsePacket();
  if (udpSize)
  {
    //serOut("Received packet of size " + String(udpSize) + " from " + udp.remoteIP().toString() + ":" + String(udp.remotePort()));

    int n = udp.read(udpRecBuffer, udpPacketLength);
    String received = (char *)udpRecBuffer;
    received.remove(n);
    if (received.equalsIgnoreCase("NCudpServer\n"))
    {
      NCwifiServerFound = true;
      udpIP = udp.remoteIP();
      udpOut("NConCMM2\n");
      if (!NCwifiServerFound or udpIP != udp.remoteIP())
        serOut("NCudpServer found on " + udpIP.toString() + ":" + String(udpPort));
    }
    else
      serOut(received);
  }  
}

void ncD(String ww)
{
  String fileName = ww.substring(0, ww.indexOf('|'));
  int partLen = ww.substring(ww.indexOf('|') + 1).toInt();
  int udpSize, counter;
  unsigned long timeOut;
  String received;
  udpOut("D" + ww + "\n");
  
  String rdy = getUdpString();
  serOut(rdy);
  int fileLen = rdy.substring(rdy.indexOf('|') + 1).toInt();
  int partNum = fileLen / partLen;
  int partRem = fileLen % partLen;
  int partCount = partNum;
  if (partRem > 0)
    partCount++;
  int rep = 0;
  bool ok = true;


  //serOut("#" + String(fileLen) + "," + String(partNum) + "," + String(partRem) + "," + String(partCount));
  if (Serial.readStringUntil('\n') == "#START")
  {
    udpOut("#START\n");
    int i = 0;
    while (i < partCount)
    {
      if (ok)
        udpOut("#NEXT\n");
      else
      {
        if (rep < 3)
          udpOut("#REPEAT"+ String(i) + "\n");
        else
        {
          udpOut("#CANCEL\n");
          return;
        }
      }
        
      udpSize = 0;
      timeOut = millis();
      while (!udpSize and (millis() - timeOut) < udpTimeout)
        udpSize = udp.parsePacket();
      if (udpSize)
      {
        if (i < partNum)
          udpSize = udp.read(udpRecBuffer, udpPacketLength);
        else
          udpSize = udp.read(udpRecBuffer, udpPacketLength);
        counter = udpRecBuffer[0] + 256 * udpRecBuffer[1];
        if (counter == i)
        {
          ok = true;
          rep = 0;
          if (Serial.readStringUntil('\n') == "#NEXT")
          {
            if (i < partNum)
              Serial.write(udpRecBuffer + 2, partLen);
            else
              Serial.write(udpRecBuffer + 2, partRem);
            i++;
          }
        }
        else
        {
          ok = false;
          rep++;
        }
      }
    }
    
    udpOut(Serial.readStringUntil('\n') + "\n");    
  }
}

void ncW(String ww)
{
  const int packetSize = 8000;
  String hlp;
  int fileLen = ww.substring(ww.indexOf('|') + 1).toInt();
  int packNum = fileLen / packetSize;
  int packRem = fileLen % packetSize;
  int actSize, expect;
  int actPack = 0, maxRep = 3, numRep = 0;
  int rep;
  
  udpOut("W" + ww + "\n");
  hlp = getUdpString();
  serOut(hlp);
  
  for (int i = 0; i < packNum; i++)
  {
    actSize = 2;
    while (actSize < (packetSize + 2))
    {
      expect = min(125, packetSize - actSize + 2);
      serOut("NEXT");
      while(Serial.available() < expect)
        ;
      actSize += Serial.readBytes(udpSendBuffer + actSize, expect);
    }

    udpSendBuffer[0] = char(i % 256);
    udpSendBuffer[1] = char(i / 256);
    udpSendBuffer[actSize] = 0;

    rep = 0;
    while (rep < 3)
    {        
      udp.beginPacket(udpIP, udpPort);
      udp.write(udpSendBuffer, actSize);
      udp.endPacket(); 
      
      hlp = getUdpString();
      if (hlp.toInt() == i)
        rep = 10;   
      else
        rep++;   
      serOut(hlp);
    }
  }

  if (packRem > 0)
  {
    actSize = 2;
    while (actSize < (packRem + 2))
    {
      expect = min(125, packRem - actSize + 2);
      serOut("NEXT");
      while(Serial.available() < expect)
        ;
      actSize += Serial.readBytes(udpSendBuffer + actSize, expect);
    }
    
    udpSendBuffer[0] = char(packNum % 256);
    udpSendBuffer[1] = char(packNum / 256);
    udpSendBuffer[actSize] = 0;

    rep = 0;
    while (rep < 3)
    {        
      udp.beginPacket(udpIP, udpPort);
      udp.write(udpSendBuffer, actSize);
      udp.endPacket();
      
      hlp = getUdpString();
      if (hlp.toInt() == packNum)
        rep = 10;   
      else
        rep++;   
      serOut(hlp);
    }
  }
  hlp = getUdpString();
  serOut(hlp);
}

void ncR(String ww)
{
  String fileName = ww.substring(0, ww.indexOf('|'));
  int partLen = ww.substring(ww.indexOf('|') + 1).toInt();
  int udpSize, counter;
  unsigned long timeOut;
  String received;
  udpOut("R" + ww + "\n");
  
  String rdy = getUdpString();
  int fileLen = rdy.substring(rdy.indexOf('|') + 1).toInt();
  int partNum = fileLen / partLen;
  int partRem = fileLen % partLen;
  int partCount = partNum;
  int rep = 0;
  bool ok = true;

  if (partRem > 0)
    partCount++;
    
  serOut(rdy);
  if (Serial.readStringUntil('\n') == "#START")
  {
    udpOut("#START\n");
    int i = 0;
    while (i < partCount)
    {
      if (ok)
        udpOut("#NEXT\n");
      else
      {
        if (rep < 3)
          udpOut("#REPEAT"+ String(i) + "\n");
        else
        {
          udpOut("#CANCEL\n");
          return;
        }
      }
        
      udpSize = 0;
      timeOut = millis();
      while (!udpSize and (millis() - timeOut) < udpTimeout)
        udpSize = udp.parsePacket();
      if (udpSize)
      {
        if (i < partNum)
          udpSize = udp.read(udpRecBuffer, udpPacketLength);
        else
          udpSize = udp.read(udpRecBuffer, udpPacketLength);
        counter = udpRecBuffer[0] + 256 * udpRecBuffer[1];
        if (counter == i)
        {
          ok = true;
          rep = 0;
          if (Serial.readStringUntil('\n') == "#NEXT")
          {
            if (i < partNum)
              Serial.write(udpRecBuffer + 2, partLen);
            else
              Serial.write(udpRecBuffer + 2, partRem);
            i++;
          }
        }
        else
        {
          ok = false;
          rep++;
        }
      }
    }
    
    udpOut(Serial.readStringUntil('\n') + "\n");    
  }
}

void ncT(String ww)
{
  String fileName = ww.substring(0, ww.indexOf('|'));
  int partLen = ww.substring(ww.indexOf('|') + 1).toInt();
  int udpSize, counter;
  unsigned long timeOut;
  String received;
  udpOut("T" + ww + "\n");
  
  String rdy = getUdpString();
  serOut(rdy);
  int fileLen = rdy.substring(rdy.indexOf('|') + 1).toInt();
  int partNum = fileLen / partLen;
  int partRem = fileLen % partLen;
  int partCount = partNum;
  if (partRem > 0)
    partCount++;
  int rep = 0;
  bool ok = true;


  //serOut("#" + String(fileLen) + "," + String(partNum) + "," + String(partRem) + "," + String(partCount));
  if (Serial.readStringUntil('\n') == "#START")
  {
    udpOut("#START\n");
    int i = 0;
    while (i < partCount)
    {
      if (ok)
        udpOut("#NEXT\n");
      else
      {
        if (rep < 3)
          udpOut("#REPEAT"+ String(i) + "\n");
        else
        {
          udpOut("#CANCEL\n");
          return;
        }
      }
        
      udpSize = 0;
      timeOut = millis();
      while (!udpSize and (millis() - timeOut) < udpTimeout)
        udpSize = udp.parsePacket();
      if (udpSize)
      {
        if (i < partNum)
          udpSize = udp.read(udpRecBuffer, udpPacketLength);
        else
          udpSize = udp.read(udpRecBuffer, udpPacketLength);
        counter = udpRecBuffer[0] + 256 * udpRecBuffer[1];
        if (counter == i)
        {
          ok = true;
          rep = 0;
          if (Serial.readStringUntil('\n') == "#NEXT")
          {
            if (i < partNum)
              Serial.write(udpRecBuffer + 2, partLen);
            else
              Serial.write(udpRecBuffer + 2, partRem);
            i++;
          }
        }
        else
        {
          ok = false;
          rep++;
        }
      }
    }
    
    udpOut(Serial.readStringUntil('\n') + "\n");    
  }
}

void httpGet(String url)
{
  int split = 250;
  WiFiClient client;
  HTTPClient http;
  int pos = url.indexOf('|');
  if (pos >= 0)
  {
    split = url.substring(pos + 1).toInt();
    url.remove(pos);
  }
  if (!url.startsWith("http"))
    url = "http://" + url;

  http.begin(url.c_str());
  int httpResponseCode = http.GET();
  if (httpResponseCode > 0) 
  {
    String payload = http.getString();
    int lines = payload.length() / split;
    if (payload.length() % split)
      lines++;
    serOut("HTTPGET " + url + '|' + String(lines));
    for(int i = 0; i <  payload.length(); i += split)
    {
      if ((i + split) < payload.length())
        serOut(payload.substring(i, i + split));
      else
        serOut(payload.substring(i));
    }
  }
  else
    serOut("Error code: " + String(httpResponseCode));
}

void tcpConnect(String server)
{
  int port = 80, plus = 0;
  WiFiClient client;
  int pos = server.indexOf(':');
  if (pos >= 0)
  {
    port = server.substring(pos + 1).toInt();
    server.remove(pos);
  }
  
  if (client.connect(server, port))
  {
    serOut("TCPCONNECTed to " + server + ':' + String(port));
    char ch;
    while (client.connected())
    {
      if (client.available())
      {
        ch = client.read();
        Serial.print(ch);
      }
      while (Serial.available() > 0 and plus < 3)
      {
        ch = Serial.read();
        Serial.print(ch);
        client.write(ch);
        if (ch == '+')
          plus++;
        else
          plus = 0;
      }
      if (plus >= 3)
      {
        client.stop();
        serOut("TCPCONNECT canceled with +++");
        return;
      }
    }
    client.stop();
    serOut("TCPCONNECT disconnected");
  }
  else
    serOut("TCPCONNECT error");
}

void pinger(String remote_host)
{
  if(Ping.ping(remote_host.c_str(), 1))
    serOut("ping " + remote_host + " ... success");
  else
     serOut("ping " + remote_host + " ... error");
}

void outHelp()
{
  serOut("HELP,10");
  serOut("@connect(SSID,password)");
  serOut("@datetime");
  serOut("@disconnect");
  serOut("@help");
  serOut("@http(url|split)");
  serOut("@mac");
  serOut("@netinfo");
  serOut("@ping(IP)");
  serOut("@reboot");
  serOut("@scan");
  serOut("@speedtest");
  serOut("@tcp(server:port)");
  serOut("@ver");
  serOut("");
  serOut("@NC_commands for Napoleon Commander UDP server");
}

void wifiConnect(String ssid)
{
  int pos = ssid.indexOf(",");
  if (pos >= 0)
  {
    String pass = ssid.substring(pos + 1);
    ssid.remove(pos);

    if (WiFi.status() == WL_CONNECTED)
      WiFi.disconnect();
    
    WiFi.begin(ssid, pass);
    if (WiFi.waitForConnectResult() == WL_CONNECTED)
    {
      ArduinoOTA.begin();
      timeClient.begin();
      timeClient.update();
      setTime(timeClient.getEpochTime());
      serOut("Connected to " + WiFi.SSID() + "," + WiFi.BSSIDstr() + "," + String(WiFi.RSSI()));
      udp.begin(udpPort);
      server.begin();
    }
    else
      serOut("Not connected to '" + ssid + "'/'" + pass + "'");        
  }
}

void loop() {
  ArduinoOTA.handle();
  timeClient.update();
  CURRENT_TIME = now();

  if (Serial.available() > 0)
  {
    // read the incoming byte:
    if (serMode == 0)
    {
      serByte = Serial.read();
      if (serByte == '@')
      {
        serCommand = Serial.readStringUntil('\n');
        serByte = '\n';
      }
    }

    else  
    {     
      Serial.print("I received: ");
      Serial.println(serByte, HEX);
    }
  }

  if (serCommand.length() > 0)
  {
    if (serCommand.equalsIgnoreCase("datetime"))
      serOut(getDate() + " " + getTime());
      
    else if (serCommand.equalsIgnoreCase("speedtest"))
      speedTest();
          
    else if (serCommand.equalsIgnoreCase("ver"))
      serOut("ESP v" + ESPversion);

    else if (serCommand.equalsIgnoreCase("mac"))
      serOut(WiFi.macAddress());

    else if (serCommand.equalsIgnoreCase("netinfo"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        String netInfo = WiFi.SSID() + "," + WiFi.BSSIDstr() + "," + String(WiFi.RSSI()) + "," + WiFi.gatewayIP().toString() + "," + WiFi.subnetMask().toString() + "," + WiFi.localIP().toString();
        serOut(netInfo);
      }
      else
        serOut("");
    }

    else if (serCommand.equalsIgnoreCase("scan"))
    {
      int countNet = WiFi.scanNetworks();
      serOut(String(countNet) + " networks found");
      if (countNet > 0)
        for (int i = 0; i < countNet; ++i)
          serOut(String(i) + "," + WiFi.SSID(i) + "," + WiFi.BSSIDstr(i) + "," + String(WiFi.RSSI(i)));
    }

    else if (serCommand.equalsIgnoreCase("disconnect"))
    {
      WiFi.disconnect();
      serOut("Disconnected");
    }

    else if (serCommand.equalsIgnoreCase("NC_?"))
    {
      if (WiFi.status() == WL_CONNECTED)
        udpOut("?\n");
    }
    
    else if (serCommand.startsWith("NC_W"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        ncW(serCommand.substring(4));
      }
    }

    else if (serCommand.startsWith("NC_R"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        ncR(serCommand.substring(4));
      }
    }

    else if (serCommand.startsWith("NC_C"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        udpOut("C" + serCommand.substring(4) + "\n");
        serOut(getUdpString());
      }
    }

    else if (serCommand.startsWith("NC_D"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        //udpOut("D" + serCommand.substring(4) + "\n");
        ncD(serCommand.substring(4));
      }
    }

    else if (serCommand.startsWith("NC_T"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        //udpOut("T" + serCommand.substring(4) + "\n");
        ncT(serCommand.substring(4));
      }
    }

    else if (serCommand.startsWith("NC_M"))
    {
      if (WiFi.status() == WL_CONNECTED)
      {
        udpOut("M" + serCommand.substring(4) + "\n");
        serOut(getUdpString());
      }
    }

    else if (serCommand.startsWith("NC_N"))
    {
      if (WiFi.status() == WL_CONNECTED)
        udpOut("N" + serCommand.substring(4) + "\n");
    }

    else if (serCommand.startsWith("NC_K"))
    {
      if (WiFi.status() == WL_CONNECTED)
        udpOut("K" + serCommand.substring(4) + "\n");
    }

    else if (serCommand.substring(0, 8).equalsIgnoreCase("connect(") && serCommand.endsWith(")"))
    {
      wifiConnect(serCommand.substring(8, serCommand.length() - 1));
    }

    else if (serCommand.substring(0, 5).equalsIgnoreCase("ping(") && serCommand.endsWith(")"))
    {
      if (WiFi.status() == WL_CONNECTED)
        pinger(serCommand.substring(5, serCommand.length() - 1));
    }
    
    else if ((WiFi.status() == WL_CONNECTED) && serCommand.substring(0, 5).equalsIgnoreCase("http(") && serCommand.endsWith(")"))
    {
      if (WiFi.status() == WL_CONNECTED)
        httpGet(serCommand.substring(5, serCommand.length() - 1));
    }

    else if ((WiFi.status() == WL_CONNECTED) && serCommand.substring(0, 4).equalsIgnoreCase("tcp(") && serCommand.endsWith(")"))
    {
      if (WiFi.status() == WL_CONNECTED)
        tcpConnect(serCommand.substring(4, serCommand.length() - 1));
    }

    else if (serCommand.equalsIgnoreCase("reboot"))
    {
      serOut("Rebooting...");
      ESP.restart();
    }

    else if (serCommand.equalsIgnoreCase("help"))
    {
      outHelp();
    }
    
    else
      serOut("Unknown command '" + serCommand + "'");

    serCommand = "";
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    testUdpPacket();
  }
  
  yield();
}
