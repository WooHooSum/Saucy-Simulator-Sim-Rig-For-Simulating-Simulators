# EMCFFBV2 – USB HID Force Feedback Firmware

**EMCFFBV2** is an advanced firmware project that transforms supported microcontroller platforms into a **USB HID (Human Interface Device)** with **Force Feedback (FFB)** capabilities.

It is designed for hobbyists, makers, and simulation enthusiasts who want to build custom **force feedback devices** such as steering wheels, joysticks, pedals, and other interactive controllers.

EMCFFBV2 currently supports both **ESP32** and **STM32** platforms, offering flexibility across different hardware ecosystems.

> **Project Status**  
> EMCFFBV2 is currently in **BETA** and under active development.  
> Some features may be incomplete or subject to change. Feedback and contributions are welcome.

---

## ⚠️ IMPORTANT LICENSE NOTICE

**THIS IS A BINARY-ONLY DISTRIBUTION**  
This repository contains **COMPILED FIRMWARE (.hex/.bin files)** and **UTILITY SOFTWARE** only.  
**SOURCE CODE IS NOT INCLUDED OR PROVIDED.**

### 📜 Usage Terms:
- **Personal/DIY Use:** ✅ Allowed (non-commercial, personal projects only)
- **Commercial Use:** ❌ **STRICTLY PROHIBITED** without commercial license
- **Redistribution:** ❌ **NOT ALLOWED** in any form
- **Reverse Engineering:** ❌ **EXPRESSLY PROHIBITED**

### 🔒 Force Feedback Licensing:
- FFB features are **LOCKED** in the free version
- Activation requires **PAID LICENSE** per device
- Attempting to bypass/crack FFB protection is **ILLEGAL**

### 🏢 Commercial Licensing:
For commercial use (OEM, product integration, resale):
- **Volume pricing available**
- **Custom firmware options**
- **Technical support included**
- **Contact:** ebolz.magy@gmail.com

**By downloading or using these files, you agree to these terms.**

---

## Supported Platforms

- **ESP32-S2**
- **ESP32-S3**
- **STM32F4 series** (ODRIVE MINI – STM32F405)

> ⚠️ Installation procedures differ between ESP32 and STM32 platforms.  
> Please follow the appropriate guide in the Wiki.

---

## Key Features

- **USB HID Compliant**
  - Works with DirectInput-based applications and games
- **Force Feedback Support** (requires license)
  - Constant Force  
  - Periodic effects (Sine, Square, Triangle)  
  - Spring, Damper, Friction, Inertia
- **Multi-Platform Support**
  - ESP32-S2 / ESP32-S3 (native USB)
  - STM32F4 via ODRIVE MINI
- **Highly Configurable**
  - Axis mapping
  - Sensor selection
  - Motor and driver configuration

---

## 📋 License Activation Process

> 🔒 **Force Feedback (FFB) functionality requires license activation.**

1. **Free Version:**
   - USB HID functionality ✅
   - Basic axes/buttons ✅
   - **Force Feedback** ❌ (locked)

2. **Licensed Version:**
   - Complete FFB functionality ✅
   - Requires **paid activation**
   - Device-based licensing

**For activation details, see:**  
👉 [License Activation Guide](https://github.com/ebolzMagy/EMCFFBV2/wiki/licenseActivation)

---

## Typical Use Cases

- 🎮 **Force Feedback** (requires license)
- 🕹️ **Joysticks and Controllers**
- 🏎️ **Driving and Flight Simulators**

---

## Getting Started

1. **Download the latest release**
   - Visit the [Releases page](https://github.com/ebolzMagy/EMCFFBV2/releases)
   - Download the **free version** for your platform

2. **Test basic functionality**
   - USB HID should work immediately
   - FFB features will be disabled (locked)

3. **Purchase license for FFB** (if needed)
   - Contact for pricing
   - Receive activation instructions

4. **Follow the installation guide**
   - 📘 [Firmware Installation](https://github.com/ebolzMagy/EMCFFBV2/wiki/Firmware-Installation)

5. **Configure your hardware**
   - GPIO setup
   - Axis setup
   - Sensor selection
   - Motor & driver configuration

---

## 🚫 Prohibited Activities

The following are **STRICTLY PROHIBITED**:
- Selling or distributing modified versions
- Integrating into commercial products without license
- Reverse engineering the firmware/software
- Sharing license keys or activation tools
- Using in commercial services/offerings

Violators will be subject to:
- DMCA takedown notices
- Legal action for copyright infringement
- Blacklisting from future updates

---

## Documentation & Community

- 📚 **Wiki**  
  https://github.com/ebolzMagy/EMCFFBV2/wiki

- 💬 **Discussions**  
  https://github.com/ebolzMagy/EMCFFBV2/discussions
  (Commercial inquiries via email only)

- 🐞 **Bug Reports & Issues**  
  https://github.com/ebolzMagy/EMCFFBV2/issues

---

## 📞 Contact & Support

**For Personal Use Support:**
- GitHub Discussions
- Issue tracker

**For Commercial Licensing:**
- **Email:** ebolz.magy@gmail.com
- **Response Time:** 24-48 hours

**For Activation Issues:**
- Include purchase receipt
- Provide device ID

---

## Disclaimer

This firmware is provided **"AS IS"** without warranty of any kind.  
The developer is not responsible for:
- Damage to hardware
- Data loss
- Personal injury
- Commercial losses

Use at your own risk. Professional/commercial users should obtain appropriate licenses.

---

### 🚀 Happy building and enjoy Force Feedback!
*(with proper licensing)*
