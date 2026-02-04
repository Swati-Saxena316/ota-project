# ESP32 OTA Firmware Update System

> **Language:** C (ESP-IDF)  
> **Target:** ESP32 / ESP32-S3  
> **Scope:** Production-grade OTA firmware update system  
> **Status:** ✅ Complete

---

## 📌 Project Overview

This repository contains a **complete, end-to-end, production-ready OTA (Over-The-Air) firmware update system** for ESP32 devices, implemented entirely in **C using ESP-IDF**.

The system was developed incrementally in **10 well-defined steps**, grouped into **4 logical phases**, ensuring correctness, safety, reliability, and maintainability.

The final implementation guarantees:

* Safe OTA updates without bricking
* Automatic rollback on failure
* Secure firmware integrity verification
* User-friendly Wi-Fi provisioning
* Persistent OTA diagnostics
* Clear LCD-based user feedback

---

## 🧭 Phase & Step Mapping

| Phase       | Description                 | Steps        |
| ----------- | --------------------------- | ------------ |
| **Phase 1** | Architecture & Control Flow | Steps 1–3    |
| **Phase 2** | Connectivity & Provisioning | Steps 4–6, 8 |
| **Phase 3** | OTA Download & Integrity    | Steps 7, 9   |
| **Phase 4** | Production Hardening        | Step 10      |

---

## 🧩 Phase 1 – Architecture & Control Flow

### **Step 1 – System Architecture & OTA Flow Design**

**Objective:** Define the complete OTA lifecycle and system boundaries.
<img width="1536" height="1024" alt="architecture diagram" src="https://github.com/user-attachments/assets/2cc3e63c-4d33-4743-bb2f-b8ba4a1ea5e5" />

**Key Outcomes:**

* Defined OTA states and transitions
* Identified failure and recovery paths
* Modularized responsibilities

**Artifacts:**

* OTA flow diagram
* State definitions

---

### **Step 2 – OTA Trigger Mechanism**

**Objective:** Ensure deterministic entry into OTA mode.

**Implementation:**

* OTA mode triggered via hardware button combination at boot
* Prevents accidental OTA entry

**Testing:**

* Verified intentional vs normal boot paths

---

### **Step 3 – OTA State Machine Implementation**

**Objective:** Implement deterministic OTA execution logic.

**Key States:**

* IDLE
* CHECKING_WIFI
* PROVISIONING
* CONNECTING
* FETCHING_MANIFEST
* DOWNLOADING
* SUCCESS
* FAILED

**Guarantees:**

* No race conditions
* Clear success/failure paths

---

## 🌐 Phase 2 – Connectivity & Provisioning

### **Step 4 – LCD UI Integration**

**Objective:** Provide real-time user feedback.

**Features:**

* LCD messages for each OTA state
* No dependency on serial console

---

### **Step 5 – Wi-Fi Auto-Connect**

**Objective:** Automatically connect to known Wi-Fi networks.

**Implementation:**

* STA mode using stored credentials
* Retry logic with failure detection

---

### **Step 6 – Basic Wi-Fi Provisioning**

**Objective:** Recover connectivity when credentials are missing.

**Features:**

* SoftAP provisioning mode
* Web UI for SSID/password entry

---

### **Step 8 – Advanced Wi-Fi Provisioning (Captive Portal)**

**Objective:** Provide production-grade provisioning UX.

**Enhancements:**

* DNS wildcard redirect
* HTTP wildcard redirect
* Provisioning timeout
* Network test endpoint
* Credentials stored in NVS

---

## 🔐 Phase 3 – OTA Download & Integrity

### **Step 7 – OTA Fundamentals**

**Objective:** Enable basic OTA update capability.

**Features:**

* HTTPS OTA foundation
* Dual-partition awareness

---

### **Step 9 – Streaming OTA Engine (Core OTA)**

**Objective:** Implement secure, verifiable OTA pipeline.

**Key Features:**

* Firmware downloaded in **4KB chunks**
* Real-time **SHA256 calculation** during download
* Firmware **size validation** vs manifest
* Anti-downgrade version checks
* Progress tracking
* Atomic boot partition switching

**Safety Guarantees:**

* Old firmware preserved until verification passes
* Power/network loss safe

---

## 🛠 Phase 4 – Production Hardening & Diagnostics

### **Step 10 – Production Hardening**

**Objective:** Make OTA system field-deployable and supportable.

**Features Implemented:**

* Persistent OTA diagnostics in NVS
* Last status / error / version tracking
* Boot counter
* Rollback detection and logging
* Boot-time pending-verify handling
* LCD progress bar
* Human-readable error messages

---

## 🧪 Testing Strategy

Each step was validated independently and cumulatively.

### Testing Coverage:

* Step-wise functional testing (Steps 1–10)
* Integration and regression testing
* Fault injection:

  * Power loss during OTA
  * Network drop during download
  * Invalid manifest
  * SHA256 mismatch

### Outcome:

* Device always remains bootable
* OTA failures are diagnosable without serial console

---

## 🔒 Safety & Reliability Guarantees

* Dual-partition OTA (A/B slots)
* Atomic firmware activation
* Automatic rollback
* Integrity verification before boot
* Provisioning recovery path

---

## 📊 OTA Security Model

* HTTPS transport
* SHA256 integrity validation
* Firmware size validation
* Anti-downgrade protection

---

## 🚫 Optional / Advanced Features (Not Implemented)

The following features were intentionally **not implemented**, as they were optional or beyond the problem statement scope:

### Security (Optional)

* Secure Boot
* Flash Encryption
* mTLS per-device authentication

### OTA Enhancements (Optional)

* Delta OTA updates
* Compressed firmware download
* Resume interrupted OTA

### Provisioning Enhancements (Optional)

* BLE provisioning
* Mobile app provisioning

### DevOps / Cloud (Optional)

* CI/CD pipeline
* Automatic manifest generation
* Fleet-wide OTA management

---

## 📁 Repository Structure (High-Level)

```
src/
├── ota/                # OTA state machine & control logic
├── ota_update/         # Streaming OTA engine
├── provisioning/       # Wi-Fi provisioning (captive portal)
├── network/            # Wi-Fi manager
├── storage/            # NVS persistence & OTA diagnostics
├── ui/                 # LCD UI
├── security/           # SHA256 utilities
├── manifest/           # OTA manifest client
└── main.c
```

---

## ✅ Final Status

✔ All problem statement requirements satisfied  
✔ Production-grade architecture  
✔ Fully tested & validated  
✔ Safe for real-world deployment

---

## 📌 Conclusion

This project demonstrates a **complete, professional OTA firmware update system** built incrementally through well-defined steps and phases.

The final solution is:

* Safe
* Secure
* Maintainable
* Field-debuggable

It can be confidently deployed on ESP32-based products.

---

**End of README**
