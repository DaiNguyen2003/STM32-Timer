# 🔧 Hướng Dẫn Setup Phần Cứng RS485 + STM32

## 📌 Tổng Quan

Tài liệu này hướng dẫn cách kết nối phần cứng và cấu hình STM32CubeMX cho giao tiếp RS485.

---

## 🛠️ Phần 1: Linh Kiện Cần Có

| STT | Linh kiện | Số lượng | Ghi chú |
|-----|-----------|----------|---------|
| 1 | STM32F103C8T6 (Blue Pill) | 2+ | Node trong mạng |
| 2 | Module MAX485 | 2+ | Mỗi node 1 module |
| 3 | USB-RS485 Converter | 1 | Kết nối PC |
| 4 | Dây cáp xoắn đôi | 1m+ | Bus A/B |
| 5 | Điện trở 120Ω | 2 | Termination resistor |
| 6 | ST-Link V2 | 1 | Nạp code |

---

## 🔌 Phần 2: Sơ Đồ Kết Nối

### 2.1 STM32 ↔ MAX485

```
    STM32 Blue Pill              MAX485 Module
    ────────────────             ─────────────
         3.3V  ──────────────────  VCC
         GND   ──────────────────  GND
         PA9 (TX)  ──────────────  DI (Data In)
         PA10 (RX) ──────────────  RO (Receiver Out)
         PA8 (DE/RE) ────┬───────  DE (Driver Enable)
                         └───────  RE (Receiver Enable)
```

> **Lưu ý**: DE và RE nối chung vào PA8
> - PA8 = HIGH → Transmit mode
> - PA8 = LOW  → Receive mode

### 2.2 MAX485 ↔ Bus RS485

```
    MAX485 #1        Bus         MAX485 #2
    ─────────        ───         ─────────
       A ────────────┬──────────── A
                     │
       B ────────────┼──────────── B
                     │
                   [120Ω]  ← Điện trở kết cuối (đầu bus)
```

### 2.3 Sơ Đồ Tổng Thể

```
┌─────────────┐     USB      ┌───────────────┐
│     PC      │─────────────▶│ USB-RS485     │
│  (Python)   │              │ Converter     │
└─────────────┘              └───────┬───────┘
                                     │ A/B
         ┌───────────────────────────┴────────────────────────┐
         │                      Bus RS485                      │
         │                     (2 dây A/B)                     │
         └───┬─────────────────────┬────────────────────┬─────┘
             │                     │                    │
      ┌──────▼──────┐       ┌──────▼──────┐     ┌──────▼──────┐
      │   MAX485    │       │   MAX485    │     │   MAX485    │
      └──────┬──────┘       └──────┬──────┘     └──────┬──────┘
             │                     │                    │
      ┌──────▼──────┐       ┌──────▼──────┐     ┌──────▼──────┐
      │  STM32 #1   │       │  STM32 #2   │     │  STM32 #3   │
      │   ID = 1    │       │   ID = 2    │     │   ID = 3    │
      │   (Servo)   │       │   (Relay)   │     │  (Sensor)   │
      └─────────────┘       └─────────────┘     └─────────────┘
```

---

## ⚙️ Phần 3: Cấu Hình STM32CubeMX (.ioc)

### 3.1 Mở Project trong CubeMX

1. Mở file `Digital_Servo.ioc` trong STM32CubeIDE
2. Hoặc: Click đúp vào file `.ioc` → Open with CubeMX

### 3.2 Cấu Hình USART1

1. **Pinout & Configuration** → **Connectivity** → **USART1**
2. Chọn **Mode**: `Asynchronous`
3. **Parameter Settings**:
   | Parameter | Value |
   |-----------|-------|
   | Baud Rate | 115200 |
   | Word Length | 8 Bits |
   | Parity | None |
   | Stop Bits | 1 |
   
4. Pins sẽ tự động được assign:
   - PA9 → USART1_TX
   - PA10 → USART1_RX

### 3.3 Cấu Hình GPIO cho DE/RE

1. Click vào pin **PA8** trên sơ đồ chip
2. Chọn **GPIO_Output**
3. **System Core** → **GPIO** → Chọn PA8:
   | Parameter | Value |
   |-----------|-------|
   | GPIO Output Level | Low |
   | GPIO Mode | Output Push Pull |
   | GPIO Pull-up/Pull-down | No pull |
   | User Label | RS485_DE |

### 3.4 Enable USART1 Interrupt

1. **System Core** → **NVIC**
2. Tìm `USART1 global interrupt`
3. ✅ Check **Enabled**
4. Set Priority: `1` (thấp hơn SysTick)

### 3.5 Generate Code

1. Click **Project** → **Generate Code**
2. Hoặc: Nhấn `Ctrl + S` để save và auto-generate

---

## 📋 Phần 4: Checklist Sau Cấu Hình

### CubeMX:
- [ ] USART1 enabled với Asynchronous mode
- [ ] Baud rate = 115200
- [ ] PA8 configured as GPIO Output
- [ ] PA8 User Label = RS485_DE
- [ ] USART1 interrupt enabled
- [ ] Code generated thành công

### Phần cứng:
- [ ] MAX485 VCC nối 3.3V hoặc 5V
- [ ] MAX485 GND nối GND
- [ ] PA9 → DI
- [ ] PA10 → RO  
- [ ] PA8 → DE + RE (chung)
- [ ] Bus A nối với A các module khác
- [ ] Bus B nối với B các module khác
- [ ] Điện trở 120Ω ở 2 đầu bus

---

## ⚠️ Lưu Ý Quan Trọng

1. **Nguồn chung**: Tất cả module phải dùng chung GND
2. **Điện trở kết cuối**: Chỉ đặt ở 2 đầu bus (không phải mỗi node)
3. **Chiều dài bus**: Tối đa 1200m với RS485
4. **Half-duplex**: Chỉ 1 thiết bị truyền tại 1 thời điểm
5. **DE/RE logic**:
   - Trước khi truyền: Set DE = HIGH
   - Sau khi truyền: Set DE = LOW (delay ~1ms)

---

## 🔗 Pinout Summary

| Chức năng | Pin STM32 | Kết nối |
|-----------|-----------|---------|
| UART TX | PA9 | MAX485 DI |
| UART RX | PA10 | MAX485 RO |
| DE/RE Control | PA8 | MAX485 DE + RE |
| LED Debug | PC13 | LED onboard |
| Servo PWM | PA0 | Servo signal |

---

## 📚 Tài Liệu Tham Khảo

- [MAX485 Datasheet](https://www.maximintegrated.com/en/products/interface/transceivers/MAX485.html)
- [STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008.pdf)
- [RS485 Application Note](https://www.ti.com/lit/an/snla049b/snla049b.pdf)
