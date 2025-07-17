# SHT-30-31-ESP-IDF
**This code is shared publicly with no copyrights; anyone is free to use, modify, or redistribute it.**

This driver was developed by me, Bruno Bavaresco Zaffari, as part of my undergraduate final thesis project in Computer Engineering. It is one of the modules included in the TCC directory of the main repository, serving as a key component for managing low-level hardware communication in the final system. Everyone is free to use, modify, and adapt this code as they wish, with no need for copyrights.

# Summary 


1. - [Introduction: What is the SHT30?](#introduction-what-is-the-sht30)
2. - [Introduction: What is the SHT30?](#introduction-what-is-the-sht30)
3. - [Key Features](#key-features)
4. - [SHT30 I2C Commands (16 bit hex)](#sht30-i2c-commands-16-bit-hex)
5. - [Quick Guide: When to Use](#quick-guide-when-to-use)
6. - [Understanding I2C Basics](#quick-review-what-is-i2c)
7. - [What is Clock Stretching?](#what-is-clock-stretching)
8. - [Understanding I2C in ESP-IDF](#understanding-i2c-in-esp-idf)
    - [7.1 Old Model v4.x or below](#old-model-v4x-or-below)
    - [7.2 New Model v5.x or above](#new-model-v5x-or-above)
    - [7.3 Side-by-Side Comparison](#side-by-side-comparison)
    - [7.4 Practical Advantages](#practical-advantages-of-the-new-model)

# Introduction: What is the SHT30?

The **SHT30** is a high-precision digital sensor manufactured by Sensirion, designed to measure **relative humidity (RH)** and **temperature** in various environments. It integrates the sensing elements and signal processing into a compact package, making it ideal for embedded systems. \
[***^***](#summary)

---
# Documentation

For detailed electrical characteristics, communication protocols, and performance specifications, refer to the official [SHT3x Datasheet](https://sensirion.com/media/documents/213E6A3B/63A5A569/Datasheet_SHT3x_DIS.pdf).\
[***^***](#summary)

---

## Key Features

* **Measurement ranges:**

  * Humidity: 0–100% RH
  * Temperature: –40°C to +125°C

* **Accuracy:**

  * ±2% RH (typical)
  * ±0.3°C (typical)

* **Digital interface:**

  * I2C communication (7-bit address, typically 0x44 or 0x45)

* **Low power consumption:**

  * Suitable for battery-powered devices

* **Integrated heater:**

  * Prevents condensation in high-humidity conditions

* **Factory-calibrated:**

  * Provides ready-to-use digital readings without user calibration
    
[***^***](#summary)

---


###  SHT30 I2C Commands (16-bit hex)

- `0x2400` → Starts measurement (high repeatability, no clock stretching).  
  → Use when you want precise temperature and humidity readings.

- `0x30A2` → Soft reset.  
  → Resets the sensor to default without cutting power.

- `0xF32D` → Read status register (16-bit).  
  → Reads internal flags: heater state, reset alerts, errors.

- `0x306D` → Heater ON.  
  → Activates internal heater to prevent condensation.

- `0x3066` → Heater OFF.  
  → Deactivates internal heater.
  
[***^***](#summary)

---


### Quick Guide: When to Use Which Command

**To read data:**
- `0x2400` → start measurement  
- then read the 6-byte result (via I2C read)

**To check status:**
- `0xF32D` → read status register

**To control heater:**
- `0x306D` → turn heater ON  
- `0x3066` → turn heater OFF

**To reset sensor:**
- `0x30A2` → soft reset
---
[***^***](#summary)


---
---
### What is Clock Stretching?

Clock stretching is a mechanism in I2C where the **slave device temporarily holds the SCL line low** to pause communication. This tells the master:
> “Wait, I need more time to process before you continue.”

Specifically:
- The master releases SCL (lets it go high),
- The slave **pulls SCL low** to “stretch” the clock,
- Once ready, the slave releases SCL, allowing the master to resume.

###  Why does it matter for SHT30?

The SHT30 supports two modes:
- **With clock stretching** → the master can immediately try to read after a command, and the sensor holds SCL low until the data is ready.
- **Without clock stretching** → the master must wait a **fixed time delay** (typically ~15–30 ms) before reading, or it risks getting incomplete data.

In your driver (`0x2400` command), you are using:
> **High repeatability, no clock stretching**,  
so the ESP32 needs to wait the full conversion time (`vTaskDelay()`) before attempting to read.


---

### When to care about clock stretching?

*When using sensors or devices that perform slow internal operations.  
When you want to avoid fixed delays and prefer real-time readiness signals.  
*When dealing with multitasking systems where wasting CPU cycles on waiting is inefficient.

---

In summary, clock stretching is an elegant I2C feature that lets slow devices control the pacing — but if unused, the master must handle timing carefully.


---
[***^***](#summary)

---
---

# Understanding I2C in ESP-IDF
[**<**](#understanding-i2c-in-esp-idf)
- [7.1 Old Model v4.x or below](#old-model-v4x-or-below)
- [7.2 New Model v5.x or above](#new-model-v5x-or-above)
- [7.3 Side-by-Side Comparison](#side-by-side-comparison)
- [7.4 Practical Advantages](#practical-advantages-of-the-new-model)

## Introduction to I2C

I2C (**Inter-Integrated Circuit**) is a widely used two-wire serial communication protocol designed to connect microcontrollers to peripherals such as sensors, EEPROMs, displays, among others. It was designed for simplicity and low cost, but using it efficiently requires careful attention to bus management, addressing, and concurrency in multitasking systems.

ESP-IDF, the official framework for ESP32, has always offered I2C support. However, until version 4.x, it used a simple, direct model with significant limitations. Starting from version 5.x, I2C was **reimplemented with a more robust and modular architecture**, following the evolution of the framework and preparing projects for more complex applications.

This document explains how I2C works in ESP-IDF, highlighting the differences between the old and new models.

## Quick review: what is I2C?

* **Two-wire half-duplex serial protocol:** SDA (data) and SCL (clock).
* **Master-slave architecture:** the master controls the clock and initiates communication.
* **Addressing:** usually 7 bits (up to 127 devices on the bus).
* **Open-drain signals:** require external pull-up resistors.
* **Synchronous communication:** all devices share the same clock.

Limitations:

* Sensitive to electrical noise.
* Requires careful timing and concurrency management.
* Needs attention to hold times, setup times, and clock stretching.
[**<**](#understanding-i2c-in-esp-idf)

--
## Old Model v4.x or below

In the old model, the I2C driver was based on:

* **Fixed bus identifiers:**

  * `I2C_NUM_0`
  * `I2C_NUM_1`

* **Global functions:**

  ```c
  i2c_driver_install()
  i2c_param_config()
  i2c_master_write()
  i2c_master_read()
  i2c_master_write_read_device()
  ```
Characteristics:

* All code shared the same bus context.
* There was no explicit separation between bus and devices.
* Manual management was required for:

  * Device addresses.
  * Mutexes to prevent collisions in multitasking.
  * Timing to avoid data corruption.

**Consequences:**

* Difficult to scale to larger projects.
* High risk of bugs, race conditions, and deadlocks.
* Less modular, harder-to-maintain code.
[**<**](#understanding-i2c-in-esp-idf)

--
## New Model v5.x or above

With the reimplementation, the I2C driver introduced two fundamental concepts:

### 1. Bus handle (`i2c_master_bus_handle_t`)

Represents **the configured physical bus**, including:

* SDA and SCL pins.
* Clock frequency.
* Internal pull-ups.
* Hardware filters.
* Error recovery strategies.

It is created with:

```c
i2c_new_master_bus(&config)
```

**Responsibilities:**

* Manage the hardware.
* Ensure thread safety.
* Coordinate access among multiple devices.

### 2. Device handle (`i2c_master_dev_handle_t`)

Represents **a device connected to the bus**, with:

* I2C address.
* Specific configurations (such as maximum supported clock frequency).
* Own context for locks and callbacks.

It is registered with:

```c
i2c_master_bus_add_device(bus_handle, &device_config, &device_handle)
```

**Responsibilities:**

* Abstract the device address.
* Manage device-specific locks.
* Allow multiple tasks to access the same peripheral safely.

### Basic flow in the new model

1. Configure the bus:

   ```c
   i2c_new_master_bus(&bus_config, &bus_handle);
   ```

2. Register each device:

   ```c
   i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
   ```

3. Execute operations using only the `dev_handle`:

   ```c
   i2c_master_transmit(dev_handle, data, data_len, timeout);
   i2c_master_receive(dev_handle, data, data_len, timeout);
   ```

**The driver handles the rest:**

* Bus selection.
* Concurrency control.
* Timing management.

## Side-by-side comparison

| Aspect                 | Old Model (≤ v4.x)                      | New Model (≥ v5.x)                            |
| ---------------------- | --------------------------------------- | --------------------------------------------- |
| Bus                    | Fixed number (`I2C_NUM_0`, `I2C_NUM_1`) | `i2c_master_bus_handle_t` (configured handle) |
| Device                 | Address passed manually in functions    | `i2c_master_dev_handle_t` (abstracted handle) |
| Concurrency management | Manual (mutexes, delays)                | Automatic (internal locks)                    |
| Modularity             | Low                                     | High, context-oriented                        |
| Multitasking safety    | Not guaranteed                          | Guaranteed by the driver                      |
| Code structure         | Coupled, error-prone                    | Clean, modular, scalable                      |
[**<**](#understanding-i2c-in-esp-idf)
--
## Practical advantages of the new model

* Safely control multiple sensors on the same bus.
* Reduce bugs related to global state.
* Cleaner, easier-to-maintain code.
* Prepares applications for asynchronous operations and future extensions.
[**<**](#understanding-i2c-in-esp-idf)
--
[***^***](#summary)

--
--
