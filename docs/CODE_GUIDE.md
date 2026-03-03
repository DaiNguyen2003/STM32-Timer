# 📚 HƯỚNG DẪN CODE RS485 - Chi Tiết Từng Bước

---

## 📁 1. CẤU TRÚC THƯ MỤC

```
SuperApp/
│
├── 3.AppStm32/                 ← 🐍 PYTHON APP (PC)
│   ├── core/                   ← Logic xử lý
│   │   ├── rs485_protocol.py   ← Định nghĩa protocol
│   │   └── rs485_handler.py    ← Gửi/nhận serial
│   ├── runners/
│   │   └── app_designer.py     ← Main app + UI logic
│   ├── ui/designer/
│   │   └── App.ui              ← Giao diện Qt
│   └── run_app.py              ← Entry point
│
└── Digital_Servo/              ← 🔧 STM32 FIRMWARE
    ├── Core/Inc/               ← Header files
    │   ├── rs485_protocol.h    ← Định nghĩa protocol
    │   └── rs485_uart.h        ← UART driver
    ├── Core/Src/               ← Source files
    │   ├── main.c              ← Main logic
    │   ├── rs485_protocol.c    ← Frame parser
    │   └── rs485_uart.c        ← UART driver
    └── docs/
        └── RS485_HARDWARE_SETUP.md
```

---

## 🔄 2. LUỒNG HOẠT ĐỘNG

```
┌─────────────────────────────────────────────────────────────┐
│                        PC (Python App)                       │
│  1. User gõ "led on"                                        │
│  2. App tạo frame: AA 01 01 00 7E                           │
│  3. Gửi qua USB-TTL                                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      USB-TTL Converter                       │
│  Chuyển USB → UART                                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        STM32 (Firmware)                      │
│  1. UART interrupt nhận từng byte                           │
│  2. Parser ghép thành frame                                 │
│  3. Xử lý command: Bật LED                                  │
│  4. Gửi ACK về: AA 01 FE 00 A9                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 📦 3. CẤU TRÚC FRAME RS485

### 3.1 Format

```
┌─────┬──────┬─────┬─────┬──────────┬─────┐
│ SOF │  ID  │ CMD │ LEN │   DATA   │ CRC │
├─────┼──────┼─────┼─────┼──────────┼─────┤
│0xAA │ 1B   │ 1B  │ 1B  │ 0-255B   │ 1B  │
└─────┴──────┴─────┴─────┴──────────┴─────┘
```

| Byte | Tên | Giải thích |
|------|-----|------------|
| SOF | Start of Frame | Luôn = `0xAA`, đánh dấu bắt đầu |
| ID | Node ID | Địa chỉ thiết bị (1-254) |
| CMD | Command | Lệnh cần thực hiện |
| LEN | Data Length | Số byte data (0-255) |
| DATA | Payload | Dữ liệu kèm theo |
| CRC | Checksum | CRC8 để kiểm tra lỗi |

### 3.2 Ví dụ cụ thể

**LED ON cho Node 1:**
```
AA  01  01  00  7E
│   │   │   │   └── CRC8
│   │   │   └────── LEN = 0 (không có data)
│   │   └────────── CMD = 0x01 (ON)
│   └────────────── ID = 1
└────────────────── SOF = 0xAA
```

**Servo pulse 1500 cho Node 1:**
```
AA  01  11  02  05  DC  XX
│   │   │   │   │   │   └── CRC8
│   │   │   │   │   └────── Low byte: 0xDC (220)
│   │   │   │   └────────── High byte: 0x05 (5)
│   │   │   └────────────── LEN = 2 bytes
│   │   └────────────────── CMD = 0x11 (SERVO_PULSE)
│   └────────────────────── ID = 1
└────────────────────────── SOF

Pulse = 0x05DC = 1500
```

---

## 🐍 4. CODE PYTHON - Giải thích

### 4.1 rs485_protocol.py - Tạo/Parse Frame

```python
# === CONSTANTS ===
SOF = 0xAA              # Byte đầu tiên của mọi frame

# === COMMANDS ===
class CMD:
    ON          = 0x01    # Bật
    OFF         = 0x02    # Tắt
    SERVO_PULSE = 0x11    # Điều khiển servo
    PING        = 0x30    # Kiểm tra kết nối
    ACK         = 0xFE    # Phản hồi OK
    NACK        = 0xFF    # Phản hồi lỗi
```

**Ví dụ tạo frame:**
```python
from core.rs485_protocol import build_frame, CMD

# LED ON cho node 1
frame = build_frame(node_id=1, cmd=CMD.ON)
# Kết quả: bytes([0xAA, 0x01, 0x01, 0x00, 0x7E])

# Servo pulse 1500 cho node 1
pulse = 1500
data = bytes([pulse >> 8, pulse & 0xFF])  # [0x05, 0xDC]
frame = build_frame(1, CMD.SERVO_PULSE, data)
```

### 4.2 rs485_handler.py - Gửi/Nhận Serial

```python
from core.rs485_handler import RS485Handler

# Khởi tạo
handler = RS485Handler('COM3', 115200)
handler.connect()

# Gửi lệnh
handler.send_command(node_id=1, cmd=CMD.ON)

# Nhận phản hồi
response = handler.receive_frame(timeout=1.0)
if response and response['cmd'] == CMD.ACK:
    print("STM32 đã nhận lệnh!")

handler.disconnect()
```

### 4.3 app_designer.py - Xử lý lệnh từ Terminal

```python
# User gõ "led on" trong Terminal
cmd_text = "led on"

# App convert thành frame
if cmd_text == 'led on':
    frame = build_frame(1, CMD.ON)
    
# App gửi qua serial
self.serial.send_binary(frame)
```

---

## 🔧 5. CODE STM32 - Giải thích

### 5.1 rs485_protocol.h - Định nghĩa

```c
// Commands
typedef enum {
    CMD_ON          = 0x01,
    CMD_OFF         = 0x02,
    CMD_SERVO_PULSE = 0x11,
    CMD_PING        = 0x30,
    CMD_ACK         = 0xFE,
    CMD_NACK        = 0xFF,
} RS485_CMD_t;

// Frame structure
typedef struct {
    uint8_t id;
    uint8_t cmd;
    uint8_t len;
    uint8_t data[255];
    uint8_t valid;
} RS485_Frame_t;
```

### 5.2 rs485_protocol.c - Parser State Machine

```c
// Mỗi byte nhận được → feed vào parser
uint8_t RS485_ParserFeed(RS485_Parser_t *parser, uint8_t byte)
{
    switch (parser->state)
    {
        case PARSER_IDLE:
            if (byte == 0xAA)  // Tìm thấy SOF
                parser->state = PARSER_GOT_SOF;
            break;
            
        case PARSER_GOT_SOF:
            parser->frame.id = byte;  // Lưu ID
            parser->state = PARSER_GOT_ID;
            break;
            
        // ... tiếp tục cho CMD, LEN, DATA, CRC
    }
}
```

### 5.3 main.c - Xử lý Command

```c
// Trong main loop
if (RS485_UART_HasNewFrame())
{
    RS485_Frame_t *frame = RS485_UART_GetFrame();
    
    switch (frame->cmd)
    {
        case CMD_ON:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            RS485_UART_SendAck();
            break;
            
        case CMD_SERVO_PULSE:
            // Ghép 2 bytes thành pulse
            uint16_t pulse = (frame->data[0] << 8) | frame->data[1];
            
            if (pulse >= 500 && pulse <= 2500) {
                __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
                RS485_UART_SendAck();
            } else {
                RS485_UART_SendNack();
            }
            break;
    }
    
    RS485_UART_ClearFrameFlag();
}
```

---

## 📝 6. CÁC LỆNH TERMINAL

| Lệnh | Frame gửi | Mô tả |
|------|-----------|-------|
| `ping` | `AA 01 30 00 92` | Kiểm tra kết nối |
| `on` | `AA 01 01 00 7E` | Bật LED |
| `off` | `AA 01 02 00 FF` | Tắt LED |
| `toggle` | `AA 01 03 00 ...` | Toggle LED |
| `servo 1500` | `AA 01 11 02 05 DC ...` | Servo pulse 1500 |
| `status` | `AA 01 20 00 ...` | Đọc trạng thái |

---

## 🧮 7. CRC8 - KIỂM TRA LỖI

### 7.1 Công thức

```python
def crc8(data):
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF
    return crc
```

### 7.2 Ví dụ

```python
# Frame: AA 01 01 00 ??
# CRC tính từ: 01 01 00 (bỏ SOF)
data = bytes([0x01, 0x01, 0x00])
crc = crc8(data)  # = 0x7E

# Frame hoàn chỉnh: AA 01 01 00 7E
```

---

## 🔌 8. KẾT NỐI PHẦN CỨNG

```
PC                 USB-TTL              STM32
─────              ───────              ─────
USB ─────────────► TX ─────────────► PA10 (RX)
                   RX ◄───────────── PA9 (TX)
                   GND ────────────── GND
                   VCC ────────────── 3.3V
```

---

## ❓ 9. FAQ - CÂU HỎI THƯỜNG GẶP

**Q: Tại sao dùng 0xAA làm SOF?**
> A: 0xAA = 10101010 binary, dễ nhận diện và ít trùng với data thông thường.

**Q: Tại sao pulse cần 2 bytes?**
> A: Vì pulse từ 500-2500, vượt quá 255 (1 byte max). Cần 2 bytes để biểu diễn.

**Q: STM32 không phản hồi?**
> 1. Kiểm tra baudrate (115200)
> 2. Kiểm tra TX/RX có đấu chéo không
> 3. Kiểm tra code đã flash chưa

**Q: CRC sai?**
> CRC tính từ byte ID đến hết DATA, không bao gồm SOF.
