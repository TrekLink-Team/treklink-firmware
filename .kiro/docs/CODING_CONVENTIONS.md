# TrekLink Coding Conventions

**Project:** TrekLink MVP - ESP32 LoRa Mesh Device  
**Purpose:** Establish low-power, responsive, and maintainable embedded C++ practices  
**Version:** 1.0  
**Last Updated:** February 4, 2026

---

## Table of Contents

1. [Core Principles](#core-principles)
2. [Timing & Non-Blocking Code](#timing--non-blocking-code)
3. [Power Management](#power-management)
4. [Memory Management](#memory-management)
5. [Error Handling](#error-handling)
6. [FreeRTOS Usage](#freertos-usage)
7. [State Machines](#state-machines)
8. [Interrupt Safety](#interrupt-safety)
9. [Serial Debug System](#serial-debug-system)
10. [Flash & NVS Management](#flash--nvs-management)
11. [Code Organization](#code-organization)

---

## Core Principles

### The Three Commandments

1. **NEVER use `delay()`** - All timing must use `millis()` or FreeRTOS primitives
2. **Loop responsiveness** - `loop()` must complete in <10ms (optimal: 1ms)
3. **Freeze the heap** - No `malloc()`/`new` after `setup()` completes

---

## Timing & Non-Blocking Code

### ❌ BANNED: Blocking Functions

```cpp
// NEVER DO THIS
delay(1000);           // Halts entire chip
while(!ready);         // Polling loop without timeout
```

### ✅ REQUIRED: Non-Blocking Patterns

```cpp
// Use millis() for timing
unsigned long lastCheck = 0;
const unsigned long CHECK_INTERVAL = 1000;  // 1 second

void loop() {
    unsigned long now = millis();
    
    if (now - lastCheck >= CHECK_INTERVAL) {
        lastCheck = now;
        doPeriodicTask();
    }
}
```

### Loop Performance Targets

| Metric | Target | Maximum |
|--------|--------|---------|
| Loop execution time | 1ms | 10ms |
| Blocking operation | 0ms | 100ms |
| ISR duration | <10μs | 50μs |

---

## Power Management

### Wake Sources

**Allowed wake triggers:**
- Button press (GPIO interrupt)
- Incoming LoRa packet (AUX pin)
- Timer (periodic GPS check)

### Sleep Strategy

**Use Light Sleep** (not Deep Sleep) to preserve:
- ✅ RAM contents (mesh routing table)
- ✅ FreeRTOS task states
- ✅ Network connections

```cpp
// Light sleep example
esp_sleep_enable_gpio_wakeup();
esp_sleep_enable_timer_wakeup(60 * 1000000);  // 60 seconds
esp_light_sleep_start();
```

### GPIO Hold During Sleep

```cpp
// Keep critical pins HIGH during sleep
gpio_hold_en(LORA_EN_PIN);
esp_light_sleep_start();
gpio_hold_dis(LORA_EN_PIN);
```

### Peripheral Power Gating

- ✅ GPS: Power off during sleep, V_BCKP always connected
- ✅ OLED: Power off in Silent Mode
- ⚠️ LoRa: Use low-power mode, DO NOT power gate (wake latency)

---

## Memory Management

### String Handling

**❌ BANNED: Arduino `String` class**

```cpp
// NEVER DO THIS - heap fragmentation
String msg = "Hello " + deviceID + "!";
```

**✅ REQUIRED: C-style strings with `snprintf`**

```cpp
char msg[64];
snprintf(msg, sizeof(msg), "Hello %s!", deviceID);
```

### Radio Payload Buffers

**Use `uint8_t[]` for raw bytes**

```cpp
uint8_t txBuffer[256];  // Fixed-size, stack or global
uint8_t rxBuffer[256];

// NOT: char payload[256];  // Confuses binary vs text
```

### Heap Allocation Rules

**✅ Acceptable in `setup()` only:**

```cpp
void setup() {
    // One-time allocation - OK
    displayBuffer = (uint8_t*)malloc(1024);
    packetQueue = new PacketQueue(32);
    
    // After this, heap is FROZEN
}
```

**❌ BANNED in `loop()` or tasks:**

```cpp
void loop() {
    char* temp = new char[128];  // ❌ HEAP FRAGMENTATION
    // ...
    delete[] temp;
}
```

### Global vs Local Variables

**✅ Use globals for singletons:**

```cpp
// Physical resources that exist once
LoRaDriver radio;
Adafruit_SSD1306 display;
GPSService gps;
MeshConfig config;
```

**✅ Pass buffers by reference:**

```cpp
// Efficient - no copy
void processPacket(uint8_t* buffer, size_t len);
void updateDisplay(MeshNode& node);
```

---

## Error Handling

### Return Type Pattern

**✅ Use `enum class` for results:**

```cpp
enum class Result {
    OK,
    ERROR_TIMEOUT,
    ERROR_I2C_FAIL,
    ERROR_INVALID_PARAM,
    ERROR_BUFFER_FULL
};

Result sendPacket(uint8_t* data, size_t len) {
    if (len == 0) return Result::ERROR_INVALID_PARAM;
    if (len > 256) return Result::ERROR_BUFFER_FULL;
    
    // ... transmission logic ...
    
    return Result::OK;
}
```

### Logging Philosophy

**Functions return errors silently. Logging happens at the top level.**

```cpp
// ❌ BAD: Low-level driver prints
Result LoRa::transmit(uint8_t* data, size_t len) {
    Serial.println("Transmitting...");  // ❌ Pollutes driver
    // ...
}

// ✅ GOOD: Caller decides what to log
void loop() {
    Result res = radio.transmit(packet, len);
    
    if (res != Result::OK) {
        DEBUG_LOG("TX failed: %d", static_cast<int>(res));
    }
}
```

---

## FreeRTOS Usage

### Task Architecture

**Migrate from Arduino loop to FreeRTOS tasks**

```cpp
void setup() {
    // Create tasks pinned to specific cores
    xTaskCreatePinnedToCore(
        loraRxTask,      // Task function
        "LoRaRX",        // Name
        4096,            // Stack size
        NULL,            // Parameters
        2,               // Priority (0-24, higher = more priority)
        NULL,            // Task handle
        0                // Core 0 (PRO_CPU)
    );
    
    xTaskCreatePinnedToCore(
        uiTask,
        "UI",
        2048,
        NULL,
        1,               // Lower priority
        NULL,
        1                // Core 1 (APP_CPU)
    );
}
```

### Core Assignment

| Core | Responsibilities | Rationale |
|------|------------------|-----------|
| **Core 0** (PRO_CPU) | LoRa RX/TX, Mesh routing, Crypto | Time-critical networking |
| **Core 1** (APP_CPU) | UI, GPS parsing, Button handling | User-facing, less latency-sensitive |

### Task Synchronization

```cpp
// Use FreeRTOS primitives
SemaphoreHandle_t radioMutex;
QueueHandle_t packetQueue;

void loraRxTask(void* param) {
    while (1) {
        if (xSemaphoreTake(radioMutex, pdMS_TO_TICKS(100))) {
            // Critical section
            radio.receive();
            xSemaphoreGive(radioMutex);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // Yield to other tasks
    }
}
```

---

## State Machines

### ✅ Explicit State Machine Pattern

```cpp
enum class NodeState {
    INITIALIZING,
    IDLE,
    GPS_ACQUIRING,
    TRANSMITTING,
    RECEIVING,
    SLEEPING
};

NodeState currentState = NodeState::INITIALIZING;

void stateMachine() {
    switch (currentState) {
        case NodeState::INITIALIZING:
            if (initComplete) {
                currentState = NodeState::IDLE;
            }
            break;
            
        case NodeState::IDLE:
            if (needGPS) {
                currentState = NodeState::GPS_ACQUIRING;
            } else if (packetPending) {
                currentState = NodeState::TRANSMITTING;
            }
            break;
            
        case NodeState::GPS_ACQUIRING:
            if (gps.hasFix()) {
                currentState = NodeState::IDLE;
            } else if (timeout()) {
                currentState = NodeState::IDLE;  // Fallback
            }
            break;
            
        // ... more states
    }
}
```

### ❌ Avoid Flag-Based Logic

```cpp
// BAD: Can create illegal states
bool isTx = false;
bool isRx = false;
bool isSleeping = false;

// What if isTx && isRx both true? Undefined behavior!
```

---

## Interrupt Safety

### ISR Rules

**✅ Minimal ISRs with `IRAM_ATTR`:**

```cpp
volatile bool packetReceived = false;

void IRAM_ATTR loraISR() {
    packetReceived = true;  // Set flag only
    // NO Serial.print, NO delay, NO malloc
}

void setup() {
    attachInterrupt(LORA_DIO0_PIN, loraISR, RISING);
}

void loop() {
    if (packetReceived) {
        packetReceived = false;
        handlePacket();  // Process outside ISR
    }
}
```

### FreeRTOS ISR Notifications

```cpp
TaskHandle_t rxTaskHandle;

void IRAM_ATTR loraISR() {
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    vTaskNotifyGiveFromISR(rxTaskHandle, &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void loraRxTask(void* param) {
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // Block until ISR
        radio.processPacket();
    }
}
```

---

## Serial Debug System

### Toggle-able Debug Output

**Press `\` to enable debug mode, `i` to show/hide instructions**

```cpp
bool debugEnabled = false;
bool showInstructions = false;

void setup() {
    Serial.begin(115200);
}

void loop() {
    if (Serial.available()) {
        char cmd = Serial.read();
        
        if (cmd == '\\') {
            debugEnabled = !debugEnabled;
            Serial.printf("\n[DEBUG] Mode: %s\n", debugEnabled ? "ON" : "OFF");
            if (debugEnabled) printInstructions();
        }
        else if (cmd == 'i') {
            showInstructions = !showInstructions;
            if (showInstructions) printInstructions();
        }
    }
    
    // Persistent instructions at top
    if (showInstructions && debugEnabled) {
        // Print instructions every N lines
    }
}

void printInstructions() {
    Serial.println("\n=== TrekLink Debug Console ===");
    Serial.println("Commands:");
    Serial.println("  \\ - Toggle debug output");
    Serial.println("  i - Show/hide instructions");
    Serial.println("  s - System status");
    Serial.println("  m - Mesh routing table");
    Serial.println("  g - GPS data");
    Serial.println("==============================\n");
}
```

### Debug Macro System

```cpp
#define DEBUG_LOG(fmt, ...) \
    do { if (debugEnabled) Serial.printf("[DEBUG] " fmt "\n", ##__VA_ARGS__); } while(0)

#define DEBUG_WARN(fmt, ...) \
    do { if (debugEnabled) Serial.printf("[WARN] " fmt "\n", ##__VA_ARGS__); } while(0)

#define DEBUG_ERROR(fmt, ...) \
    Serial.printf("[ERROR] " fmt "\n", ##__VA_ARGS__)  // Always print errors

// Usage
DEBUG_LOG("Packet RX: RSSI=%d, SNR=%d", rssi, snr);
DEBUG_ERROR("I2C timeout on address 0x%02X", addr);
```

---

## Flash & NVS Management

### Minimize NVS Writes

**Use "Dirty Bit" pattern:**

```cpp
struct Config {
    uint8_t channelID;
    uint16_t deviceID;
    bool locationBroadcast;
    // ...
};

Config config;
bool configDirty = false;

void setChannelID(uint8_t newID) {
    if (config.channelID != newID) {
        config.channelID = newID;
        configDirty = true;  // Mark for save
    }
}

void loop() {
    static unsigned long lastSave = 0;
    
    // Save only if dirty and 30 seconds elapsed
    if (configDirty && (millis() - lastSave > 30000)) {
        nvs.write("config", &config, sizeof(config));
        configDirty = false;
        lastSave = millis();
    }
}
```

### Write Frequency Limits

| Data Type | Max Write Frequency | Rationale |
|-----------|---------------------|-----------|
| Settings | Once per menu exit | User-triggered |
| Telemetry | Once per 30 minutes | Minimize wear |
| Logs | On critical events only | SOS, panic |

---

## Watchdog Timer

### Hardware Watchdog Configuration

```cpp
void setup() {
    // Enable HW watchdog (5 seconds)
    esp_task_wdt_init(5, true);
    esp_task_wdt_add(NULL);  // Add current task
}

void loop() {
    // Pet watchdog every iteration
    esp_task_wdt_reset();
    
    // ... main loop logic ...
}
```

### Task Watchdog for Critical Tasks

```cpp
void loraRxTask(void* param) {
    esp_task_wdt_add(NULL);  // Subscribe task to WDT
    
    while (1) {
        radio.receive();
        esp_task_wdt_reset();  // Pet watchdog
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

## Code Organization

### File Structure

```
treklink/
├── src/
│   ├── main.cpp                  // FreeRTOS task creation
│   ├── hal/                      // Hardware drivers
│   │   ├── power_gate.cpp
│   │   ├── button_handler.cpp
│   │   └── lora_driver.cpp
│   ├── services/                 // Business logic
│   │   ├── mesh_router.cpp
│   │   ├── gps_service.cpp
│   │   └── crypto_service.cpp
│   └── ui/                       // User interface
│       ├── display.cpp
│       └── menu.cpp
├── include/
│   ├── hardware_config.h         // GPIO pin definitions
│   ├── hal/
│   ├── services/
│   └── ui/
└── CODING_CONVENTIONS.md         // This file
```

### Naming Conventions

| Element | Convention | Example |
|---------|-----------|---------|
| Classes | PascalCase | `LoRaDriver`, `MeshRouter` |
| Functions | camelCase | `sendPacket()`, `updateDisplay()` |
| Variables | camelCase | `packetBuffer`, `currentState` |
| Constants | UPPER_SNAKE_CASE | `MAX_PACKET_SIZE`, `GPS_TIMEOUT_MS` |
| Enums | PascalCase enum + UPPER_CASE values | `enum class State { IDLE, SLEEPING }` |

---

## Quick Reference Checklist

Before committing code, verify:

- [ ] No `delay()` calls anywhere
- [ ] `loop()` completes in <10ms
- [ ] No heap allocations in `loop()` or tasks
- [ ] All strings use `snprintf()`, not `String`
- [ ] Radio buffers use `uint8_t[]`
- [ ] Error returns use `enum class Result`
- [ ] ISRs have `IRAM_ATTR` and are minimal
- [ ] State machine uses `enum class` + `switch`
- [ ] Debug output guarded by `debugEnabled` flag
- [ ] NVS writes use dirty-bit pattern
- [ ] Watchdog is petted in all tasks

---

## Enforcement

Code reviews MUST reject:
- ❌ Any use of `delay()`
- ❌ Arduino `String` class
- ❌ Heap allocation in `loop()`
- ❌ Serial.print in low-level drivers
- ❌ Missing `IRAM_ATTR` on ISRs

---

**Last Reviewed:** 2026-02-04  
**Maintainer:** TrekLink Development Team
