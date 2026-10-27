#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPS++.h>
#include <ESP32Servo.h>

// =====================================================
// PROJECT: Intelligent Agricultural Robot
// MAIN ESP32 CODE
// Features:
// - WiFi AP Dashboard
// - BTS7960 Motor Control
// - GPS Live Status
// - Servo Sensor Deploy/Retract, delay = 1800 ms
// - NPK 7-in-1 through MAX485 Modbus RTU
// - CSV Export
// - ESP32-CAM stream viewer
// =====================================================

// =====================================================
// WIFI ACCESS POINT
// =====================================================
const char* ssid = "ESP32_Robot";
const char* password = "12345678";

WebServer server(80);

// =====================================================
// ESP32-CAM SETTINGS
// Keep ESP32-CAM code using static IP: 192.168.4.200
// Stream URL is displayed only inside the dashboard.
// =====================================================
const char* CAMERA_IP = "192.168.4.200";
const char* CAMERA_PAGE_URL = "http://192.168.4.200";
const char* CAMERA_STREAM_URL = "http://192.168.4.200:81/stream";

// =====================================================
// GPS MODULE
// =====================================================
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

#define GPS_RX_PIN 34   // GPS TX -> ESP32 GPIO34, GPS RX not used

String latitude = "Not Taken";
String longitude = "Not Taken";
String gpsStatus = "No GPS Data";
String gpsStatusColor = "#ef4444";
String gpsSatellites = "--";
String gpsHDOP = "--";
String gpsAge = "--";

unsigned long lastGPSStatusUpdate = 0;
const unsigned long GPS_STATUS_INTERVAL = 1000;

// =====================================================
// MAX485 / NPK SENSOR SETTINGS
// Datasheet:
// - Modbus RTU
// - Default ID: 0x01
// - Baud: 9600
// - Data: 8N1
// - Function: 0x03
// Register map:
// pH          0x0006 unit 0.01 pH
// Moisture    0x0012 unit 0.1%RH
// Temperature 0x0013 unit 0.1C
// EC          0x0015 unit 1 us/cm
// N           0x001E unit mg/kg
// P           0x001F unit mg/kg
// K           0x0020 unit mg/kg
// =====================================================
HardwareSerial npkSerial(1);

#define RS485_RX_PIN 16       // MAX485 RO -> ESP32 GPIO16
#define RS485_TX_PIN 17       // ESP32 GPIO17 -> MAX485 DI
#define RS485_DE_RE_PIN 4     // MAX485 RE + DE together -> ESP32 GPIO4

byte NPK_SLAVE_ID = 0x01;

String nitrogen = "--";
String phosphorus = "--";
String potassium = "--";
String soilPH = "--";
String moisture = "--";
String temperature = "--";
String ecValue = "--";
String npkStatus = "Not Read";

bool npkValid = false;
float valMoisture = 0;
float valTemp = 0;
float valPH = 0;
int valEC = 0;
int valN = 0;
int valP = 0;
int valK = 0;

// =====================================================
// BTS7960 MOTOR DRIVER PINS
// =====================================================
#define M1_RPWM 26   // Left motor RPWM
#define M1_LPWM 27   // Left motor LPWM
#define M2_RPWM 13   // Right motor RPWM
#define M2_LPWM 12   // Right motor LPWM

// =====================================================
// MOTOR SAFETY WATCHDOG
// =====================================================
bool motorMoving = false;
unsigned long lastMotorCommandTime = 0;
const unsigned long MOTOR_TIMEOUT = 900;

// =====================================================
// SERVO SENSOR DEPLOY MECHANISM
// =====================================================
Servo sensorServo;

#define SERVO_PIN 25

const int SERVO_STOP_VALUE = 90;
const int SERVO_DOWN_VALUE = 180;
const int SERVO_UP_VALUE = 0;

// Requested delay for deploy/retract motor
const int SERVO_MOVE_TIME_MS = 1000;

bool sensorIsDown = false;
String sensorStatus = "Sensor UP";

// =====================================================
// SAMPLE DATA
// =====================================================
String sampleTable = "";
String csvData = "";
int sampleCounter = 0;
String systemMessage = "System Ready";

// =====================================================
// FUNCTION DECLARATIONS
// =====================================================
void handleRoot();
void updateGPSStatus();
bool readNPKSensor();

// =====================================================
// HELPER FUNCTIONS
// =====================================================
String jsonEscape(String input) {
  input.replace("\\", "\\\\");
  input.replace("\"", "\\\"");
  input.replace("\n", "\\n");
  input.replace("\r", "\\r");
  return input;
}

String getRunTime() {
  if (gps.date.isValid() && gps.time.isValid()) {
    char buffer[35];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d UTC",
            gps.date.year(), gps.date.month(), gps.date.day(),
            gps.time.hour(), gps.time.minute(), gps.time.second());
    return String(buffer);
  }

  unsigned long totalSeconds = millis() / 1000;
  int hours = totalSeconds / 3600;
  int minutes = (totalSeconds % 3600) / 60;
  int seconds = totalSeconds % 60;

  char buffer[20];
  sprintf(buffer, "UP %02d:%02d:%02d", hours, minutes, seconds);
  return String(buffer);
}

// =====================================================
// GPS STATUS UPDATE
// =====================================================
void updateGPSStatus() {
  int sats = -1;

  if (gps.satellites.isValid()) {
    sats = gps.satellites.value();
    gpsSatellites = String(sats);
  } else {
    gpsSatellites = "--";
  }

  if (gps.hdop.isValid()) {
    gpsHDOP = String(gps.hdop.hdop(), 2);
  } else {
    gpsHDOP = "--";
  }

  if (gps.location.isValid()) {
    gpsAge = String(gps.location.age() / 1000.0, 1) + " s";
  } else {
    gpsAge = "--";
  }

  if (gps.location.isValid() && gps.location.age() < 5000) {
    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    gpsStatus = "GPS FIX OK";
    gpsStatusColor = "#22c55e";
  } else if (sats > 0) {
    gpsStatus = "Searching Satellites";
    gpsStatusColor = "#f97316";
  } else {
    gpsStatus = "No GPS Signal";
    gpsStatusColor = "#ef4444";
  }
}

// =====================================================
// MODBUS CRC
// =====================================================
uint16_t modbusCRC(byte *buf, int len) {
  uint16_t crc = 0xFFFF;

  for (int pos = 0; pos < len; pos++) {
    crc ^= (uint16_t)buf[pos];

    for (int i = 0; i < 8; i++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

void rs485TransmitMode() {
  digitalWrite(RS485_DE_RE_PIN, HIGH);  // DE=1, RE=1 -> transmit
  delayMicroseconds(300);
}

void rs485ReceiveMode() {
  digitalWrite(RS485_DE_RE_PIN, LOW);   // DE=0, RE=0 -> receive
  delayMicroseconds(300);
}

bool readRegisters(byte slaveID, uint16_t startReg, uint16_t quantity, uint16_t *values) {
  while (npkSerial.available()) {
    npkSerial.read();
  }

  byte request[8];
  request[0] = slaveID;
  request[1] = 0x03;
  request[2] = highByte(startReg);
  request[3] = lowByte(startReg);
  request[4] = highByte(quantity);
  request[5] = lowByte(quantity);

  uint16_t crc = modbusCRC(request, 6);
  request[6] = crc & 0xFF;
  request[7] = (crc >> 8) & 0xFF;

  rs485TransmitMode();
  npkSerial.write(request, 8);
  npkSerial.flush();
  delayMicroseconds(900);
  rs485ReceiveMode();

  byte response[80];
  int index = 0;
  int expectedBytes = 5 + (quantity * 2);
  unsigned long startTime = millis();

  while (millis() - startTime < 1200) {
    if (npkSerial.available()) {
      response[index++] = npkSerial.read();
      if (index >= expectedBytes) break;
    }
  }

  if (index < expectedBytes) return false;
  if (response[0] != slaveID) return false;
  if (response[1] != 0x03) return false;
  if (response[2] != quantity * 2) return false;

  uint16_t receivedCRC = response[index - 2] | (response[index - 1] << 8);
  uint16_t calculatedCRC = modbusCRC(response, index - 2);
  if (receivedCRC != calculatedCRC) return false;

  for (int i = 0; i < quantity; i++) {
    int dataIndex = 3 + (i * 2);
    values[i] = (response[dataIndex] << 8) | response[dataIndex + 1];
  }

  return true;
}

// =====================================================
// NPK READ FUNCTION BASED ON DATASHEET REGISTER MAP
// =====================================================
bool readNPKSensor() {
  npkStatus = "Reading...";
  systemMessage = "Reading soil sensor...";
  npkValid = false;

  uint16_t values[4];
  bool okPH = false;
  bool okMoistTemp = false;
  bool okEC = false;
  bool okNPK = false;

  okPH = readRegisters(NPK_SLAVE_ID, 0x0006, 1, values);
  if (okPH) {
    valPH = values[0] / 100.0;
  }

  okMoistTemp = readRegisters(NPK_SLAVE_ID, 0x0012, 2, values);
  if (okMoistTemp) {
    valMoisture = values[0] / 10.0;
    int16_t tempRaw = (int16_t)values[1];
    valTemp = tempRaw / 10.0;
  }

  okEC = readRegisters(NPK_SLAVE_ID, 0x0015, 1, values);
  if (okEC) {
    valEC = values[0];
  }

  okNPK = readRegisters(NPK_SLAVE_ID, 0x001E, 3, values);
  if (okNPK) {
    valN = values[0];
    valP = values[1];
    valK = values[2];
  }

  if (okPH) soilPH = String(valPH, 2);
  else soilPH = "--";

  if (okMoistTemp) {
    moisture = String(valMoisture, 1) + " %";
    temperature = String(valTemp, 1) + " C";
  } else {
    moisture = "--";
    temperature = "--";
  }

  if (okEC) ecValue = String(valEC) + " us/cm";
  else ecValue = "--";

  if (okNPK) {
    nitrogen = String(valN) + " mg/kg";
    phosphorus = String(valP) + " mg/kg";
    potassium = String(valK) + " mg/kg";
  } else {
    nitrogen = "--";
    phosphorus = "--";
    potassium = "--";
  }

  if (okPH && okMoistTemp && okEC && okNPK) {
    npkStatus = "Read OK";
    systemMessage = "Soil sensor values read successfully";
    npkValid = true;
    return true;
  }

  if (okPH || okMoistTemp || okEC || okNPK) {
    npkStatus = "Partial Read";
    systemMessage = "Soil sensor partially responded";
    npkValid = false;
    return false;
  }

  npkStatus = "No Response";
  systemMessage = "Soil sensor did not respond";
  npkValid = false;
  return false;
}

// =====================================================
// MOTOR FUNCTIONS
// =====================================================
void markMotorCommand() {
  motorMoving = true;
  lastMotorCommandTime = millis();
}

void stopMotors() {
  digitalWrite(M1_RPWM, LOW);
  digitalWrite(M1_LPWM, LOW);
  digitalWrite(M2_RPWM, LOW);
  digitalWrite(M2_LPWM, LOW);
  motorMoving = false;
}

void forward() {
  markMotorCommand();
  digitalWrite(M1_RPWM, HIGH);
  digitalWrite(M1_LPWM, LOW);
  digitalWrite(M2_RPWM, HIGH);
  digitalWrite(M2_LPWM, LOW);
}

void backward() {
  markMotorCommand();
  digitalWrite(M1_RPWM, LOW);
  digitalWrite(M1_LPWM, HIGH);
  digitalWrite(M2_RPWM, LOW);
  digitalWrite(M2_LPWM, HIGH);
}

void left() {
  markMotorCommand();
  digitalWrite(M1_RPWM, LOW);
  digitalWrite(M1_LPWM, HIGH);
  digitalWrite(M2_RPWM, HIGH);
  digitalWrite(M2_LPWM, LOW);
}

void right() {
  markMotorCommand();
  digitalWrite(M1_RPWM, HIGH);
  digitalWrite(M1_LPWM, LOW);
  digitalWrite(M2_RPWM, LOW);
  digitalWrite(M2_LPWM, HIGH);
}

void motorWatchdog() {
  if (motorMoving && (millis() - lastMotorCommandTime > MOTOR_TIMEOUT)) {
    stopMotors();
  }
}

// =====================================================
// SERVO FUNCTIONS
// =====================================================
void stopServoMotor() {
  sensorServo.write(SERVO_STOP_VALUE);
}

void moveSensorDown() {
  stopMotors();
  sensorStatus = "Deploying Sensor...";
  systemMessage = "Sensor motor moving down";

  sensorServo.write(SERVO_DOWN_VALUE);
  delay(SERVO_MOVE_TIME_MS);
  stopServoMotor();

  sensorIsDown = true;
  sensorStatus = "Sensor DOWN";
  systemMessage = "Sensor deployed successfully";
}

void moveSensorUp() {
  stopMotors();
  sensorStatus = "Retracting Sensor...";
  systemMessage = "Sensor motor moving up";

  sensorServo.write(SERVO_UP_VALUE);
  delay(SERVO_MOVE_TIME_MS);
  stopServoMotor();

  sensorIsDown = false;
  sensorStatus = "Sensor UP";
  systemMessage = "Sensor retracted successfully";
}

void toggleSensor() {
  if (!sensorIsDown) moveSensorDown();
  else moveSensorUp();
}

// =====================================================
// SAMPLE FUNCTIONS
// =====================================================
void takeSample() {
  stopMotors();
  updateGPSStatus();

  if (!sensorIsDown) {
    systemMessage = "Deploy the sensor before taking sample";
    npkStatus = "Deploy Sensor First";
    return;
  }

  String csvLatitude = "";
  String csvLongitude = "";

  if (gps.location.isValid() && gps.location.age() < 10000) {
    latitude = String(gps.location.lat(), 6);
    longitude = String(gps.location.lng(), 6);
    csvLatitude = latitude;
    csvLongitude = longitude;
  } else {
    latitude = "No Signal";
    longitude = "No Signal";
  }

  readNPKSensor();
  sampleCounter++;
  String currentTime = getRunTime();

  sampleTable += "<tr>";
  sampleTable += "<td>" + String(sampleCounter) + "</td>";
  sampleTable += "<td>" + currentTime + "</td>";
  sampleTable += "<td>" + latitude + "</td>";
  sampleTable += "<td>" + longitude + "</td>";
  sampleTable += "<td>" + nitrogen + "</td>";
  sampleTable += "<td>" + phosphorus + "</td>";
  sampleTable += "<td>" + potassium + "</td>";
  sampleTable += "<td>" + soilPH + "</td>";
  sampleTable += "<td>" + moisture + "</td>";
  sampleTable += "<td>" + temperature + "</td>";
  sampleTable += "<td>" + ecValue + "</td>";
  sampleTable += "<td>" + npkStatus + "</td>";
  sampleTable += "</tr>";

  csvData += String(sampleCounter) + ",";
  csvData += currentTime + ",";
  csvData += csvLatitude + ",";
  csvData += csvLongitude + ",";

  if (npkValid) {
    csvData += String(valN) + ",";
    csvData += String(valP) + ",";
    csvData += String(valK) + ",";
    csvData += String(valPH, 2) + ",";
    csvData += String(valMoisture, 1) + ",";
    csvData += String(valTemp, 1) + ",";
    csvData += String(valEC) + ",";
    csvData += npkStatus + "\n";
  } else {
    csvData += ",,,,,,,";
    csvData += npkStatus + "\n";
  }

  systemMessage = "Sample saved successfully";
}

void clearSamples() {
  sampleTable = "";
  sampleCounter = 0;
  csvData = "SampleID,Time,Latitude,Longitude,N_mgkg,P_mgkg,K_mgkg,pH,Moisture_percent,Temperature_C,EC_uscm,NPK_Status\n";

  latitude = "Not Taken";
  longitude = "Not Taken";
  nitrogen = "--";
  phosphorus = "--";
  potassium = "--";
  soilPH = "--";
  moisture = "--";
  temperature = "--";
  ecValue = "--";
  npkStatus = "Not Read";
  systemMessage = "Samples cleared";
}

void downloadCSV() {
  String output = "\xEF\xBB\xBF";
  output += csvData;
  server.sendHeader("Content-Disposition", "attachment; filename=soil_samples.csv");
  server.sendHeader("Connection", "close");
  server.send(200, "text/csv; charset=utf-8", output);
}

void getCSVRaw() {
  server.send(200, "text/csv; charset=utf-8", csvData);
}

// =====================================================
// WEB DASHBOARD
// =====================================================
void handleRoot() {
  updateGPSStatus();

  String sensorButtonText = sensorIsDown ? "Retract Sensor" : "Deploy Sensor";
  String sensorButtonClass = sensorIsDown ? "orange" : "green";
  String sensorStepText = sensorIsDown ? "Sensor is inside soil. You can take sample now." : "Deploy sensor before taking sample.";

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
<title>Intelligent Agricultural Robot</title>
<style>
*{box-sizing:border-box;margin:0;padding:0;font-family:Arial,Helvetica,sans-serif;-webkit-tap-highlight-color:transparent;}
body{background:#08111f;color:white;min-height:100vh;padding:14px;user-select:none;overscroll-behavior:none;}
.container{max-width:1200px;margin:auto;}
.header{background:linear-gradient(135deg,#0f172a,#172033);border:1px solid #27354f;border-radius:20px;padding:18px;margin-bottom:14px;display:flex;justify-content:space-between;align-items:center;gap:12px;box-shadow:0 12px 32px rgba(0,0,0,.3);}
.header h1{color:#38bdf8;font-size:25px;margin-bottom:5px;}
.header p{color:#94a3b8;font-size:14px;}
.badge{background:#06111f;border:1px solid #334155;border-radius:999px;padding:10px 14px;color:#e5e7eb;white-space:nowrap;}
.badge span{color:#22c55e;font-weight:900;}
.grid{display:grid;grid-template-columns:1.08fr .92fr;gap:14px;}
.card{background:#121e33;border:1px solid #27354f;border-radius:18px;padding:16px;margin-bottom:14px;box-shadow:0 8px 24px rgba(0,0,0,.25);}
.cardHeader{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;gap:10px;}
h2{font-size:18px;color:#22c55e;}
.tag{background:#08111f;color:#94a3b8;border:1px solid #27354f;border-radius:999px;padding:6px 10px;font-size:12px;}
.cameraBox{background:#06111f;border:1px solid #27354f;border-radius:16px;min-height:285px;display:flex;align-items:center;justify-content:center;overflow:hidden;position:relative;}
.cameraBox img{width:100%;max-height:390px;object-fit:contain;}
.cameraFallback{display:none;color:#94a3b8;text-align:center;line-height:1.8;padding:20px;}
.gpsPanel,.infoBox,.messageBox{background:#06111f;border:1px solid #27354f;border-radius:15px;padding:13px;line-height:1.8;color:#e5e7eb;font-size:14px;}
.gpsPanel{margin-bottom:12px;}
.gpsTop{display:flex;justify-content:space-between;gap:10px;border-bottom:1px solid #27354f;padding-bottom:8px;margin-bottom:8px;}
.gpsTop b{color:#94a3b8;}
.gpsStatus{font-weight:900;}
.gpsGrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;}
.gpsItem{background:#0f172a;border:1px solid #27354f;border-radius:12px;padding:8px;}
.gpsItem b{display:block;color:#38bdf8;font-size:12px;margin-bottom:3px;}
.gpsItem span{font-weight:800;color:#e5e7eb;font-size:13px;}
.controls{display:grid;grid-template-columns:90px 90px 90px;grid-template-rows:70px 70px 70px;justify-content:center;align-items:center;gap:10px;margin:12px 0 14px;}
.ctrlBtn{height:66px;border:none;border-radius:16px;background:#38bdf8;color:white;font-size:26px;font-weight:900;cursor:pointer;touch-action:none;box-shadow:0 8px 16px rgba(56,189,248,.22);}
.ctrlBtn:active,.actionBtn:active{transform:scale(.96);filter:brightness(.9);}
.stopBtn{background:#ef4444;font-size:17px;box-shadow:0 8px 16px rgba(239,68,68,.22);}
.workflow{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:12px;}
.step{background:#06111f;border:1px solid #27354f;border-radius:14px;padding:12px;text-align:center;color:#cbd5e1;font-size:13px;}
.step b{display:block;color:#38bdf8;margin-bottom:5px;}
.actionGrid{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-top:12px;}
.actionBtn{border:none;border-radius:14px;padding:14px 10px;color:white;font-weight:900;cursor:pointer;min-height:52px;font-size:14px;}
.green{background:#22c55e;}.red{background:#ef4444;}.orange{background:#f97316;}.purple{background:#8b5cf6;}.blue{background:#0ea5e9;}.dark{background:#334155;}
.values{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;}
.valueCard{background:#06111f;border:1px solid #27354f;border-radius:14px;padding:13px;text-align:center;min-height:86px;}
.valueCard .label{color:#94a3b8;font-size:13px;margin-bottom:9px;}
.valueCard .value{color:#22c55e;font-size:17px;font-weight:900;}
.tableWrap{width:100%;overflow-x:auto;margin-top:14px;border:1px solid #27354f;border-radius:14px;}
table{width:100%;min-width:1080px;border-collapse:collapse;background:#06111f;font-size:13px;}
th{background:#38bdf8;color:#06111f;padding:11px;white-space:nowrap;}td{padding:10px;color:#e5e7eb;border-bottom:1px solid #27354f;text-align:center;}
.note{margin-top:10px;color:#94a3b8;font-size:13px;line-height:1.7;}
.messageBox{margin-top:12px;}.messageBox b{color:#38bdf8;}
@media(max-width:900px){body{padding:8px}.header{flex-direction:column;align-items:flex-start}.header h1{font-size:20px}.grid{grid-template-columns:1fr}.controls{grid-template-columns:78px 78px 78px;grid-template-rows:64px 64px 64px}.ctrlBtn{height:60px;font-size:23px}.actionGrid{grid-template-columns:1fr}.workflow{grid-template-columns:1fr 1fr}.values{grid-template-columns:1fr 1fr}.cameraBox{min-height:210px}.gpsGrid{grid-template-columns:1fr}}
</style>
</head>
<body>
<div class="container">
  <div class="header">
    <div><h1>INTELLIGENT AGRICULTURAL ROBOT</h1><p>Soil analysis data collection system for AI fertilizer recommendation</p></div>
    <div class="badge"><span>●</span> Online | GPS | NPK | Camera | CSV</div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="cardHeader"><h2>Live Camera</h2><div class="tag">ESP32-CAM @@CAMERA_IP@@</div></div>
      <div class="cameraBox">
        <img id="cam" alt="ESP32-CAM Stream">
        <div id="camText" class="cameraFallback">Camera is not connected yet<br>Retrying stream automatically...</div>
      </div>
      <div class="note">Camera stream: @@CAMERA_STREAM_URL@@</div>
      <div class="actionGrid" style="grid-template-columns:repeat(3,1fr);margin-top:10px">
        <button class="actionBtn blue" onclick="reloadCamera()">Reload Camera</button>
        <button class="actionBtn dark" onclick="openCameraStream()">Open Stream</button>
        <button class="actionBtn dark" onclick="openCameraPage()">Open Camera</button>
      </div>
    </div>

    <div class="card">
      <div class="cardHeader"><h2>Robot Control</h2><div class="tag">W A S D</div></div>

      <div class="gpsPanel">
        <div class="gpsTop"><b>GPS Status</b><span id="gpsStatus" class="gpsStatus" style="color:@@GPS_STATUS_COLOR@@">@@GPS_STATUS@@</span></div>
        <div class="gpsGrid">
          <div class="gpsItem"><b>Satellites</b><span id="gpsSat">@@GPS_SATELLITES@@</span></div>
          <div class="gpsItem"><b>HDOP</b><span id="gpsHdop">@@GPS_HDOP@@</span></div>
          <div class="gpsItem"><b>GPS Age</b><span id="gpsAge">@@GPS_AGE@@</span></div>
          <div class="gpsItem"><b>Fix Quality</b><span id="gpsFix">@@GPS_STATUS@@</span></div>
          <div class="gpsItem"><b>Latitude</b><span id="gpsLat">@@LATITUDE_DATA@@</span></div>
          <div class="gpsItem"><b>Longitude</b><span id="gpsLng">@@LONGITUDE_DATA@@</span></div>
        </div>
      </div>

      <div class="controls">
        <div></div><button class="ctrlBtn holdBtn" data-cmd="forward">↑</button><div></div>
        <button class="ctrlBtn holdBtn" data-cmd="left">←</button><button class="ctrlBtn stopBtn" onclick="sendStop()">STOP</button><button class="ctrlBtn holdBtn" data-cmd="right">→</button>
        <div></div><button class="ctrlBtn holdBtn" data-cmd="backward">↓</button><div></div>
      </div>

      <div class="infoBox">Keyboard: W Forward | S Backward | A Left | D Right<br>Motor watchdog is enabled for safety.</div>
    </div>
  </div>

  <div class="card">
    <div class="cardHeader"><h2>Sampling Workflow</h2><div class="tag">Manual Field Operation</div></div>
    <div class="workflow">
      <div class="step"><b>1</b>Drive robot</div><div class="step"><b>2</b>Deploy sensor</div><div class="step"><b>3</b>Take sample</div><div class="step"><b>4</b>Export CSV</div>
    </div>
    <div class="messageBox"><b>System Message:</b> @@SYSTEM_MESSAGE@@<br><b>Sensor Status:</b> @@SENSOR_STATUS@@<br><b>Current Step:</b> @@SENSOR_STEP_TEXT@@</div>
    <div class="actionGrid">
      <button class="actionBtn @@SENSOR_BUTTON_CLASS@@" onclick="sendAction('sensorToggle')">@@SENSOR_BUTTON_TEXT@@</button>
      <button class="actionBtn purple" onclick="sendAction('readNPK')">Read NPK Only</button>
      <button class="actionBtn dark" onclick="sendStop()">Emergency Stop</button>
    </div>
  </div>

  <div class="card">
    <div class="cardHeader"><h2>Sampling Data</h2><div class="tag">GPS + NPK + Time</div></div>
    <div class="infoBox">Last Latitude: <span id="sampleLat">@@LATITUDE_DATA@@</span><br>Last Longitude: <span id="sampleLng">@@LONGITUDE_DATA@@</span><br>NPK Status: @@NPK_STATUS@@</div>
    <div class="actionGrid">
      <button class="actionBtn green" onclick="sendAction('sample')">Take Sample</button>
      <button class="actionBtn blue" onclick="downloadCSV()">Download CSV</button>
      <button class="actionBtn orange" onclick="sendToAI()">Send to AI Platform</button>
      <button class="actionBtn red" onclick="sendAction('clear')">Clear Table</button>
    </div>
    <div class="tableWrap"><table><tr><th>#</th><th>Time</th><th>Latitude</th><th>Longitude</th><th>N</th><th>P</th><th>K</th><th>pH</th><th>Moisture</th><th>Temp</th><th>EC</th><th>Status</th></tr>@@SAMPLE_TABLE@@</table></div>
  </div>

  <div class="card">
    <div class="cardHeader"><h2>Live Soil Sensor Values</h2><div class="tag">NPK 7 in 1</div></div>
    <div class="values">
      <div class="valueCard"><div class="label">Nitrogen</div><div class="value">@@N_VALUE@@</div></div>
      <div class="valueCard"><div class="label">Phosphorus</div><div class="value">@@P_VALUE@@</div></div>
      <div class="valueCard"><div class="label">Potassium</div><div class="value">@@K_VALUE@@</div></div>
      <div class="valueCard"><div class="label">pH</div><div class="value">@@PH_VALUE@@</div></div>
      <div class="valueCard"><div class="label">Moisture</div><div class="value">@@MOISTURE_VALUE@@</div></div>
      <div class="valueCard"><div class="label">Temperature</div><div class="value">@@TEMP_VALUE@@</div></div>
      <div class="valueCard"><div class="label">EC</div><div class="value">@@EC_VALUE@@</div></div>
      <div class="valueCard"><div class="label">Samples</div><div class="value">@@SAMPLE_COUNT@@</div></div>
    </div>
  </div>
</div>

<script>
let activeCmd=null;let repeatTimer=null;let activeKey=null;const AI_UPLOAD_URL="http://192.168.4.3:5000/upload";
const CAMERA_STREAM_URL="@@CAMERA_STREAM_URL@@";
const CAMERA_PAGE_URL="@@CAMERA_PAGE_URL@@";
function sendCmd(cmd){fetch('/'+cmd,{cache:'no-store'}).catch(()=>{});}
function sendStop(){activeCmd=null;activeKey=null;if(repeatTimer){clearInterval(repeatTimer);repeatTimer=null;}fetch('/stop',{cache:'no-store',keepalive:true}).catch(()=>{});}
function startMove(cmd){if(activeCmd===cmd)return;sendStop();activeCmd=cmd;sendCmd(cmd);repeatTimer=setInterval(function(){if(activeCmd){sendCmd(activeCmd);}},250);}
function sendAction(cmd){sendStop();fetch('/'+cmd,{cache:'no-store'}).then(r=>location.reload()).catch(()=>location.reload());}
function downloadCSV(){sendStop();window.location.href='/download';}
function sendToAI(){sendStop();fetch('/csvraw',{cache:'no-store'}).then(r=>r.text()).then(csv=>{let form=document.createElement('form');form.method='POST';form.action=AI_UPLOAD_URL;form.target='_blank';let input=document.createElement('textarea');input.name='csv';input.value=csv;input.style.display='none';form.appendChild(input);document.body.appendChild(form);form.submit();form.remove();}).catch(()=>{alert('AI platform is not available yet.');});}
function updateGPSLive(){fetch('/gpsjson',{cache:'no-store'}).then(r=>r.json()).then(data=>{let gpsStatus=document.getElementById('gpsStatus');let gpsFix=document.getElementById('gpsFix');let gpsSat=document.getElementById('gpsSat');let gpsHdop=document.getElementById('gpsHdop');let gpsAge=document.getElementById('gpsAge');let gpsLat=document.getElementById('gpsLat');let gpsLng=document.getElementById('gpsLng');let sampleLat=document.getElementById('sampleLat');let sampleLng=document.getElementById('sampleLng');if(gpsStatus){gpsStatus.innerText=data.status;gpsStatus.style.color=data.color;}if(gpsFix)gpsFix.innerText=data.status;if(gpsSat)gpsSat.innerText=data.satellites;if(gpsHdop)gpsHdop.innerText=data.hdop;if(gpsAge)gpsAge.innerText=data.age;if(gpsLat)gpsLat.innerText=data.lat;if(gpsLng)gpsLng.innerText=data.lng;if(sampleLat)sampleLat.innerText=data.lat;if(sampleLng)sampleLng.innerText=data.lng;}).catch(()=>{});}
setInterval(updateGPSLive,1000);updateGPSLive();
function loadCamera(){let cam=document.getElementById('cam');let camText=document.getElementById('camText');if(!cam)return;if(camText)camText.style.display='block';cam.onerror=function(){if(camText)camText.style.display='block';};cam.onload=function(){cam.style.display='block';if(camText)camText.style.display='none';};cam.src=CAMERA_STREAM_URL+'?t='+Date.now();}
function reloadCamera(){let cam=document.getElementById('cam');if(cam){cam.removeAttribute('src');setTimeout(loadCamera,300);}}
function openCameraStream(){window.open(CAMERA_STREAM_URL,'_blank');}
function openCameraPage(){window.open(CAMERA_PAGE_URL,'_blank');}
setTimeout(loadCamera,900);
document.querySelectorAll('.holdBtn').forEach(function(btn){btn.addEventListener('pointerdown',function(e){e.preventDefault();startMove(btn.dataset.cmd);});btn.addEventListener('pointerup',function(e){e.preventDefault();sendStop();});btn.addEventListener('pointercancel',function(e){e.preventDefault();sendStop();});btn.addEventListener('pointerleave',function(e){e.preventDefault();sendStop();});btn.addEventListener('contextmenu',function(e){e.preventDefault();});});
document.addEventListener('keydown',function(e){let key=e.key.toLowerCase();if(e.repeat)return;if(key==='w'){activeKey='w';startMove('forward');}if(key==='s'){activeKey='s';startMove('backward');}if(key==='a'){activeKey='a';startMove('left');}if(key==='d'){activeKey='d';startMove('right');}if(key===' '){sendStop();}});
document.addEventListener('keyup',function(e){let key=e.key.toLowerCase();if(key===activeKey){sendStop();}});
window.addEventListener('blur',sendStop);document.addEventListener('visibilitychange',function(){if(document.hidden){sendStop();}});
</script>
</body>
</html>
)rawliteral";

  html.replace("@@CAMERA_IP@@", String(CAMERA_IP));
  html.replace("@@CAMERA_PAGE_URL@@", String(CAMERA_PAGE_URL));
  html.replace("@@CAMERA_STREAM_URL@@", String(CAMERA_STREAM_URL));

  html.replace("@@GPS_STATUS_COLOR@@", gpsStatusColor);
  html.replace("@@GPS_STATUS@@", gpsStatus);
  html.replace("@@GPS_SATELLITES@@", gpsSatellites);
  html.replace("@@GPS_HDOP@@", gpsHDOP);
  html.replace("@@GPS_AGE@@", gpsAge);
  html.replace("@@SYSTEM_MESSAGE@@", systemMessage);
  html.replace("@@LATITUDE_DATA@@", latitude);
  html.replace("@@LONGITUDE_DATA@@", longitude);
  html.replace("@@NPK_STATUS@@", npkStatus);
  html.replace("@@SENSOR_STATUS@@", sensorStatus);
  html.replace("@@SENSOR_BUTTON_TEXT@@", sensorButtonText);
  html.replace("@@SENSOR_BUTTON_CLASS@@", sensorButtonClass);
  html.replace("@@SENSOR_STEP_TEXT@@", sensorStepText);
  html.replace("@@SAMPLE_TABLE@@", sampleTable);
  html.replace("@@SAMPLE_COUNT@@", String(sampleCounter));
  html.replace("@@N_VALUE@@", nitrogen);
  html.replace("@@P_VALUE@@", phosphorus);
  html.replace("@@K_VALUE@@", potassium);
  html.replace("@@PH_VALUE@@", soilPH);
  html.replace("@@MOISTURE_VALUE@@", moisture);
  html.replace("@@TEMP_VALUE@@", temperature);
  html.replace("@@EC_VALUE@@", ecValue);

  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "text/html; charset=utf-8", html);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);

  sampleTable.reserve(12000);
  csvData.reserve(14000);
  csvData = "SampleID,Time,Latitude,Longitude,N_mgkg,P_mgkg,K_mgkg,pH,Moisture_percent,Temperature_C,EC_uscm,NPK_Status\n";

  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, -1);

  pinMode(RS485_DE_RE_PIN, OUTPUT);
  rs485ReceiveMode();
  npkSerial.begin(9600, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

  pinMode(M1_RPWM, OUTPUT);
  pinMode(M1_LPWM, OUTPUT);
  pinMode(M2_RPWM, OUTPUT);
  pinMode(M2_LPWM, OUTPUT);
  stopMotors();

  sensorServo.setPeriodHertz(50);
  sensorServo.attach(SERVO_PIN, 500, 2400);
  sensorServo.write(SERVO_STOP_VALUE);

  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);

  Serial.println("WiFi Started");
  Serial.print("Robot Dashboard IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Camera expected IP: ");
  Serial.println(CAMERA_IP);
  Serial.print("Camera stream URL: ");
  Serial.println(CAMERA_STREAM_URL);

  server.on("/", handleRoot);
  server.on("/forward", [](){ forward(); server.send(200, "text/plain", "OK"); });
  server.on("/backward", [](){ backward(); server.send(200, "text/plain", "OK"); });
  server.on("/left", [](){ left(); server.send(200, "text/plain", "OK"); });
  server.on("/right", [](){ right(); server.send(200, "text/plain", "OK"); });
  server.on("/stop", [](){ stopMotors(); server.send(200, "text/plain", "OK"); });
  server.on("/sensorToggle", [](){ toggleSensor(); server.send(200, "text/plain", "OK"); });
  server.on("/sample", [](){ takeSample(); server.send(200, "text/plain", "OK"); });
  server.on("/readNPK", [](){ readNPKSensor(); server.send(200, "text/plain", "OK"); });
  server.on("/clear", [](){ clearSamples(); server.send(200, "text/plain", "OK"); });
  server.on("/download", [](){ downloadCSV(); });
  server.on("/csvraw", [](){ getCSVRaw(); });

  server.on("/gpsjson", [](){
    updateGPSStatus();
    String json = "{";
    json += "\"status\":\"" + jsonEscape(gpsStatus) + "\",";
    json += "\"color\":\"" + jsonEscape(gpsStatusColor) + "\",";
    json += "\"satellites\":\"" + jsonEscape(gpsSatellites) + "\",";
    json += "\"hdop\":\"" + jsonEscape(gpsHDOP) + "\",";
    json += "\"age\":\"" + jsonEscape(gpsAge) + "\",";
    json += "\"lat\":\"" + jsonEscape(latitude) + "\",";
    json += "\"lng\":\"" + jsonEscape(longitude) + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  server.begin();
  systemMessage = "Dashboard started successfully";
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  server.handleClient();

  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (millis() - lastGPSStatusUpdate >= GPS_STATUS_INTERVAL) {
    lastGPSStatusUpdate = millis();
    updateGPSStatus();
  }

  motorWatchdog();
}
