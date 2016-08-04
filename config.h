const char* ssid = "YourSSID";
const char* password = "YourPassword";
IPAddress broadcastIP(192, 168, 1, 255);
unsigned int txPort = 1145; // port to transmit data on

//SERVER strings and interfers for OpenEVSE Energy Monotoring
// Credits: https://github.com/TrystanLea/EmonESP
const char* host = "emoncms.org";
const int httpsPort = 443;
const char* e_url = "/input/post.json?json=";
const char* fingerprint = "B6:44:19:FF:B8:F2:62:46:60:39:9D:21:C6:FB:F5:6A:F3:D9:1A:79";

const String apikey = "your-api-key";
