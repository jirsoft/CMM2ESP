# CMM2ESP
Arduino code for ESP8266 connected to CMM2. It comunicates with CMM2 over serial port with default speed of 691200 baud.
It knows this commands:

**@datetime**
* gets date and time in format **yyyy-mm-dd hh:mm:ss**

**@ver**
* gets version of this firmware

**@mac**
* gets MAC address of WiFi
	
**@netinfo**
* gets info about connection to AP in format **SSID, BSSID, RSI, gateway IP, network mask, own IP**
* needs to be connected
	
**@scan**
* scans WiFi networks and returns:
  * number of networks found (n)
  * for every network **number (0...n-1), SSID, BSSID, RSI**
  * doesn't need to be connected
	
**@disconnect**
* disconnects from current AP
* returns info **Disconnected**
  
**@connect(SSID, password)**
* connect to AP
* returns info **Connected to SSID, BSSID, RSI**
  
**@httpget(url|split)**
* send HTTP GET request to server
* returns **HTTPGET url|number of batches (n)** and then **n** times **split** characters with answer
  
  SEND **@httpget(scooterlabs.com/echo?ip)**
  * **HTTPGET http://scooterlabs.com/echo?ip|1**	
  follows just 1 packet (default split is 250 characters)
  * **1.2.3.4**
		
  SEND **@httpget(scooterlabs.com/echo?ip|3)**
    * **HTTPGET http://scooterlabs.com/echo?ip|3**
    follow 3 packets (split was set to 3 characters per batch)
    * **1.2**
    * **.3.**
    * **4**

Then is here few commands for Napoleon Commander and test
**@speedtest**
* Just for speed measurement. Prepares random data and then send **data length, packet length, number of packets, rest in last packet**, you need to accept this data, on the end it sends 1 line with speed info and CRC8.
* Next expects **number of bytes to receive** followed with this exact amount,  againg on the end returns spped into
  
**Napoleon Commander command**
* **@NC_?, @NC_W, @NC_R, @NC_C, @NC_D, @NC_T, @NC_M, @NC_N and @NC_K**. These commands are used for communication with **NCudpServer.py**


#### v0.38
	first public version in own repository
