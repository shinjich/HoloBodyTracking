---
page_type: sample
name: HoloBodyTracking sample
description: Sample Holographic UWP application that demonstrates full-body tracking with a remote server.
languages:
- cpp
products:
- windows-mixed-reality
- hololens
- azure kinect dk
---

# HoloBodyTracking sample

![License](https://img.shields.io/badge/license-MIT-green.svg)

This sample is a Holographic UWP application that demonstrates full-body tracking with a remote server. The application is based on the sensor visualization sample.

## Contents

| File/folder | Description |
|-------------|-------------|
| `CommonBT` | C++ common files for full-body tracking. |
| `HoloBodyTracking` | C++ client application files and assets for hololens 2. |
| `PCBodyTracking` | C++ server application files for server PC. |
| `PCBodyTrackingSender` | C++ client application files for onsite PC. |
| `HoloBodyTracking.sln` | Visual Studio solution file. |
| `README.md` | This README file. |

## Prerequisites

* Install the [latest Mixed Reality tools](https://docs.microsoft.com/windows/mixed-reality/develop/install-the-tools)

## Setup

1. After cloning and opening the **HoloBodyTracking.sln** solution in Visual Studio 2019, build (ARM64), and deploy for hololens 2. And build (x64), and deploy for server.
2. [Enable Device Portal and Research Mode](https://docs.microsoft.com/windows/mixed-reality/research-mode)

## Running app for client (Camera shooting side)

1. Modify strings for server IP address.
2. Build and deploy to a device (HoloLens 2 or onsite PC).
* HoloLens 2: Launch HoloBodyTracking from the start menu (Accept access to the camera for the first time).
* Onsite PC: Launch PCBodyTrackingSender.

Note:
* App for onsite PC need azure kinect runtime libraries (e.g., k4a.dll etc.)
* The firewall settings must be configured before or at run time.

## Running app for server (Body estimation processing side)

1. Build and deploy to a server.
2. Launch PCBodyTracking with client IP address as an argument.

Note:
* App for server need runtime libraries for body tracking (e.g., k4abt.dll etc.)
* The firewall settings must be configured before or at run time.
* For stable frame rates, we recommend to run with high-performance GPUs (e.g., the N-series of Azure VMs).

## Key concepts

Refer to the **Docs/ResearchMode-ApiDoc.pdf** documentation, and pay special attention to the sections on: 
* **Consent Prompts**
* **Main Sensor Reading Loop**
* **Camera Sensors**
* **Sensors > Sensor Frames**

## See also

* [Research Mode for HoloLens](https://docs.microsoft.com/windows/mixed-reality/develop/platform-capabilities-and-apis/research-mode)
* [Azure Kinect documentation](https://docs.microsoft.com/en-us/azure/kinect-dk/)
