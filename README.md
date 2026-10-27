Intelligent Agricultural Tracked Robot for In-Situ Soil Analysis & Decision Support
### 📹 Field Demonstration & In-Situ Soil Sampling Video
[▶️ Click Here to Watch Field Traversal & Robot Soil Sampling Demo (MP4)](./20261027_155631.mp4)
Please download the file.
---
University of Aleppo | Faculty of Electrical & Electronic Engineering | Mechatronics Department (2025–2026)

Graduation Capstone Project | Grade Awarded: 97/100 (Top Distinction)

Supervised by: Dr. Yaman Jannat | Exhibited at Damascus International Fair 2026

1. Project Overview & Motivation

Traditional agricultural soil diagnostics rely heavily on manual core sampling followed by delayed, centralized chemical laboratory testing. This project implements a fully integrated mechatronic mobile platform: an autonomous-ready crawler UGV engineered to traverse uneven field terrain, deploy a multi-depth probe to acquire real-time soil fertility metrics, pair readings with GPS satellite coordinates, stream live visual feedback, and deliver rule-based AI fertilizer recommendations via an interactive decision dashboard.

2. Key Engineering Subsystems & Architecture

graph TD
    subgraph Agricultural Field Sensing & Positioning
        NPK[7-in-1 NPK Soil Probe<br/>N, P, K, pH, Moisture, Temp, EC]
        GPS[NEO-6M GPS Module<br/>NMEA Sentences via UART]
        CAM[ESP32-CAM Module<br/>Live Video Stream at 192.168.4.200:81]
    end

    subgraph Central Control & Telemetry Node
        ESP[ESP32 Dual-Core Microcontroller<br/>Local Wi-Fi AP: ESP32_Robot<br/>Embedded HTTP WebServer & Dashboard]
        MAX[MAX485 Transceiver<br/>Direction Control via GPIO4]
    end

    subgraph Actuation & High-Power Drive
        BTS1[BTS7960 Driver - Left Motor]
        BTS2[BTS7960 Driver - Right Motor]
        SRV[MG995 Servo Actuator<br/>Rack & Pinion Probe Deployment]
        TRK[High-Traction Crawler Tracks<br/>8x Independent Spring Suspensions]
    end

    subgraph External Decision Support & AI
        CSV[CSV Dataset Logging<br/>Sample ID, Timestamp, Lat/Long, NPK]
        AI[Python & Streamlit Expert System<br/>Automated Nutrient Deficit & Fertilizer Dosage]
    end

    NPK -- RS485 Differential --> MAX
    MAX -- TTL UART --> ESP
    GPS -- Serial UART --> ESP
    ESP -- PWM / Direction --> BTS1
    ESP -- PWM / Direction --> BTS2
    ESP -- PWM Signal --> SRV
    BTS1 --> TRK
    BTS2 --> TRK
    ESP -- Local Wi-Fi Access Point --> CSV
    CSV --> AI


⚙️ Hardware Specifications:

Crawler Locomotion & Chassis: Custom tracked mobility base (408 mm length × 289 mm width × 132.5 mm height) featuring 8 independent spring-loaded suspension arms, 10 road-wheel assemblies with precision steel bearings, and dual DC motors driven by heavy-duty BTS7960 H-bridges.

Probe Actuation Mechanism: Vertical rack-and-pinion deployment stage actuated by an MG995 metal-gear servo ensuring controlled perpendicular soil penetration and retraction to protect sensors during motion.

Multi-Parameter Sensor Rig: Industrial 7-in-1 Soil Sensor reading Nitrogen, Phosphorus, Potassium (mg/kg), pH, Moisture (%), Temperature (°C), and Electrical Conductivity (EC in µS/cm) communicating over RS-485 Modbus RTU with CRC error verification.

Micro-Positioning: NEO-6M GPS providing real-time latitude, longitude, and satellite fix data for spatial fertility mapping.

Embedded Web Dashboard: Self-hosted local Wi-Fi Access Point (ESP32_Robot) running on the ESP32, allowing full teleoperation, real-time sensor visualization, live camera streaming from ESP32-CAM, and one-click CSV data export.

3. Decision-Support & Rule-Based AI Engine

The offline analytical module is built in Python (Streamlit) to convert raw agricultural field readings into actionable agronomic insights:

Data Validation Pipeline: Cleanses incoming CSV streams, filters sensor communication faults, and parses Modbus register anomalies.

Agronomic Knowledge Base: Evaluates macronutrient levels (N-P-K), soil reaction (pH), moisture status, and salinity hazards (EC) against targeted crop requirement thresholds.

Prescription Generator: Generates automated fertilizer recommendations (Nitrogenous, Phosphatic, Potassic) and irrigation advisories accompanied by interactive spatial correlation charts.

4. Experimental Results & Capstone Distinction

Field Validated: Successfully demonstrated across uneven agricultural terrains in Aleppo with zero track derailing and robust vibration isolation.

Evaluation Score: Awarded 97/100 (Outstanding / First-Class Honors) by the academic examination board at the University of Aleppo.

National Recognition: Selected to officially represent the University of Aleppo at the Damascus International Fair 2026.
