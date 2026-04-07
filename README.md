# ⚡ IoT-Integrated Smart Wireless Charging Infrastructure for Electric Vehicles with Dynamic On-Road Power Transfer

## 📌 Overview
This project presents a **fully automated IoT-based Smart EV Charging Station** integrated with **Wireless Power Transfer (WPT)** using inductive coupling. The system enables **contactless EV charging**, along with **real-time monitoring, automated billing, cloud logging, and intelligent control**.

The system is built using **ESP8266**, integrates **2N2222A transistor-based wireless energy transfer**, and provides a complete smart infrastructure for EV charging stations.

---

## 🚀 Key Features

### ⚡ Wireless EV Charging
- Contactless power transfer using inductive coupling  
- 2N2222A transistor-based transmitter circuit  
- Transmitter coils on charging pad, receiver coils under EV  

### 📡 IoT Monitoring System
- ESP8266 web server dashboard  
- Real-time system monitoring  
- Live status updates  

### 💰 Smart Billing System
- Automatic billing based on charging duration  
- Per-minute cost calculation  
- Stores last session data  

### 🚧 Smart Gate Automation
- Servo-controlled gate system  
- IR sensor-based vehicle detection  
- Automatic entry based on availability  

### ☁️ Cloud Integration
- Google Sheets logging using HTTP API  
- Stores events, billing, and system data  

### 📧 Email Notification System
- SMTP-based email alerts  
- Triggered on events (occupied, free, full)  

### 🔔 Notification System
- Real-time notification queue  
- Dashboard pop-up alerts  
- Event tracking system  

### 📟 LCD Display System
- Live status display  
- Temporary messages for user interaction  

---

## 🧠 System Architecture

```
                ⚡ Power Supply
                       │
                       ▼
        ┌───────────────────────────┐
        │   Transmitter Coil (Tx)   │
        └───────────────────────────┘
                       │
                       │  ~ ~ ~ Magnetic Field ~ ~ ~
                       ▼
        ┌───────────────────────────┐
        │   Receiver Coil (EV Rx)   │
        └───────────────────────────┘
                       │
                       ▼
                🚗 EV Charging
                       │
                       ▼
        ┌───────────────────────────┐
        │     ESP8266 Controller    │
        └───────────────────────────┘
                       │
        ┌──────────────┼──────────────┬──────────────┐
        ▼              ▼              ▼              ▼
  📡 Web Dashboard   ☁️ Google Sheets   📧 Email Alerts   🔔 Notifications

                       │
                       ▼
     Sensors → Billing → Gate Control → User Interaction
```

---

## ⚡ Wireless Power Transfer (Core Concept)

The system works based on **Electromagnetic Induction**.

### 📖 Working Principle

- The **2N2222A transistor** acts as a switching oscillator  
- Generates high-frequency current in transmitter coil  
- Creates alternating magnetic field  
- Receiver coil captures magnetic flux  
- Induces voltage (Faraday’s Law)  
- Converted to DC and used for EV charging  

---

## 🔌 Circuit Explanation

### 🧩 Transmitter Side (Charging Pad)

```cpp
- 2N2222A Transistor
- Copper Coil
- Base Resistor
- Power Supply
```

**Working:**
- Transistor rapidly switches ON/OFF  
- Creates alternating magnetic field  
- Enables wireless power transfer  

---

### 🚗 Receiver Side (EV)

```cpp
- Copper Coil
- Rectifier Circuit (Diodes)
- Capacitor Filter
- Battery / Load
```

**Working:**
- Receives magnetic field  
- Generates AC voltage  
- Converts to DC  
- Charges battery  

---

## 🛠️ Hardware Components

- ESP8266 (NodeMCU)  
- 2N2222A Transistor  
- Copper Coils (Tx & Rx)  
- IR Sensors (Gate + 3 Pads)  
- Servo Motor (Gate Control)  
- 16x2 LCD (I2C)  
- Resistors, Capacitors, Diodes  
- Power Supply  

---

## 💻 Software & Libraries

- Arduino IDE  

Libraries:
- ESP8266WiFi  
- ESP8266WebServer  
- ESP8266HTTPClient  
- WiFiClientSecureBearSSL  
- LiquidCrystal_I2C  
- Servo  

---

## 🔌 Pin Configuration

| Component        | Pin |
|-----------------|-----|
| Gate IR Sensor  | D3  |
| Pad 1 IR        | D0  |
| Pad 2 IR        | D2  |
| Pad 3 IR        | D1  |
| Servo Motor     | D4  |

---

## ⚙️ Configuration

```cpp
const char* ssid = "YOUR_WIFI";
const char* password = "YOUR_PASSWORD";

const char* senderEmail = "your_email@gmail.com";
const char* senderPass  = "app_password";
const char* receiverEmail = "receiver@gmail.com";

const char* sheetsURL = "YOUR_SCRIPT_URL";
```

---

## 🔄 System Working Flow

### 🚗 Entry Process
1. Vehicle detected by gate IR sensor  
2. System checks available pads  
3. Gate opens automatically  
4. LCD displays assigned pad  

### ⚡ Charging Process
5. Vehicle detected on pad  
6. Wireless charging starts  
7. Timer begins for billing  

### 💰 Exit Process
8. Vehicle leaves pad  
9. Charging stops  
10. Bill calculated automatically  
11. Data logged to Google Sheets  
12. Email notification sent  

---

## 🧠 Internal System Logic

### 🔹 Task Queue System
- Handles email and cloud logging tasks  
- Prevents blocking operations  

### 🔹 Debouncing Logic
- Eliminates false IR sensor triggers  
- Ensures stable readings  

### 🔹 State Management
- Tracks pad occupancy  
- Maintains session data  

### 🔹 Notification Queue
- Stores and displays alerts  
- Supports real-time updates  

---

## 🌐 Web Dashboard Features

- Real-time pad status  
- Charging progress  
- Billing information  
- Revenue analytics  
- Notifications display  
- System status monitoring  

---

## 📧 Email Alerts

Triggered when:
- Pad occupied  
- Charging completed  
- Station full  
- System start  

---

## 📈 Google Sheets Logging

Stores:
- Event type  
- Pad number  
- Status  
- Billing  
- IP address  

---

## ⚠️ Limitations

- Efficiency depends on coil alignment  
- Limited charging distance (~3–5 cm)  
- Energy loss in air gap  

---

## 🔮 Future Enhancements

- Resonant wireless charging  
- AI-based predictive analytics  
- Mobile app integration  
- Renewable energy integration (VAWT)  
- Dynamic road charging  

---

## 🎯 Applications

- Smart EV Charging Stations  
- Wireless Charging Infrastructure  
- Smart Cities  
- Sustainable Transportation  

---

## 👨‍💻 Team Members

- **Vijay M** (Guide)
- **Nagella Nagavenkat**
- **C. Charitha**  
- **G. Sai Lakshmi**   
- **Chaitra Reddy D**  
- **K. Asritha**  

Kalasalingam Academy of Research and Education  

---

## 📄 License

For academic and research purposes only.
