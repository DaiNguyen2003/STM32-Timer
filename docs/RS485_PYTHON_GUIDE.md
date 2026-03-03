# 🐍 HƯỚNG DẪN RS485 - PYTHON APP
## Dành cho người mới bắt đầu

---

# 📖 MỤC LỤC

1. [Tổng quan](#1-tổng-quan)
2. [Cấu trúc thư mục](#2-cấu-trúc-thư-mục)
3. [Frame là gì?](#3-frame-là-gì)
4. [Các lệnh (Commands)](#4-các-lệnh-commands)
5. [Tính CRC8](#5-tính-crc8)
6. [Tạo Frame (build_frame)](#6-tạo-frame-build_frame)
7. [Giải mã Frame (parse_frame)](#7-giải-mã-frame-parse_frame)
8. [Gửi/Nhận qua Serial](#8-gửinhận-qua-serial)
9. [Tích hợp vào App](#9-tích-hợp-vào-app)
10. [Ví dụ hoàn chỉnh](#10-ví-dụ-hoàn-chỉnh)

---

# 1. TỔNG QUAN

## 1.1 RS485 là gì?

RS485 là một **chuẩn giao tiếp** cho phép:
- Truyền dữ liệu **xa** (đến 1200m)
- Kết nối **nhiều thiết bị** trên cùng 1 đường dây
- Chống **nhiễu** tốt

```
┌─────────────┐                          ┌─────────────┐
│   PC App    │                          │   STM32     │
│  (Python)   │◄─────── RS485 Bus ──────►│ (Firmware)  │
└─────────────┘                          └─────────────┘
```

## 1.2 Tại sao cần Protocol?

Khi gửi dữ liệu qua RS485, ta cần "đóng gói" dữ liệu theo một **quy tắc chung** để:
- Thiết bị nhận **hiểu được** dữ liệu
- **Phát hiện lỗi** nếu dữ liệu bị hỏng
- **Phân biệt** các thiết bị khác nhau

---

# 2. CẤU TRÚC THƯ MỤC

```
3.AppStm32/
├── core/
│   ├── rs485_protocol.py   ← Định nghĩa protocol
│   └── rs485_handler.py    ← Gửi/nhận serial
├── runners/
│   └── app_designer.py     ← Main app + UI
└── run_app.py              ← Điểm khởi động
```

| File | Tác dụng |
|------|----------|
| `rs485_protocol.py` | Tạo và giải mã frame |
| `rs485_handler.py` | Kết nối serial, gửi/nhận dữ liệu |
| `app_designer.py` | Xử lý giao diện và logic ứng dụng |

---

# 3. FRAME LÀ GÌ?

## 3.1 Định nghĩa

**Frame** = Một "gói tin" hoàn chỉnh chứa tất cả thông tin cần thiết.

Giống như khi gửi thư bưu điện:
- **Phong bì** = Frame
- **Địa chỉ người nhận** = ID
- **Nội dung thư** = DATA
- **Tem bưu điện** = CRC (kiểm tra)

## 3.2 Cấu trúc Frame

```
┌─────┬──────┬─────┬─────┬──────────┬─────┐
│ SOF │  ID  │ CMD │ LEN │   DATA   │ CRC │
├─────┼──────┼─────┼─────┼──────────┼─────┤
│ 1B  │ 1B   │ 1B  │ 1B  │ 0-255B   │ 1B  │
└─────┴──────┴─────┴─────┴──────────┴─────┘
```

## 3.3 Giải thích từng phần0.





### SOF (Start of Frame) = 0xAA
```
Luôn là 0xAA (số 170)
Đánh dấu: "Bắt đầu một frame mới!"
```

### ID (Địa chỉ)
```
1-254 = Địa chỉ cụ thể
255 (0xFF) = Gửi cho TẤT CẢ
```
**Ví dụ:** ID = 1 → Gửi cho thiết bị số 1

### CMD (Command - Lệnh)
```
Lệnh muốn thực hiện (xem mục 4)
```
**Ví dụ:** CMD = 0x01 → Bật thiết bị

### LEN (Length - Độ dài)
```
Số byte trong phần DATA
0 = Không có data
```

### DATA (Dữ liệu)
```
Dữ liệu kèm theo (tùy lệnh)
```

### CRC (Checksum)
```
Mã kiểm tra lỗi
Nếu CRC sai = Frame bị hỏng
```

## 3.4 Ví dụ minh họa

**Lệnh: Bật LED thiết bị số 1**

```
AA  01  01  00  7E
│   │   │   │   │
│   │   │   │   └── CRC = 0x7E (tính toán)
│   │   │   └────── LEN = 0 (không có data)
│   │   └────────── CMD = 0x01 (lệnh ON)
│   └────────────── ID = 1 (thiết bị số 1)
└────────────────── SOF = 0xAA (bắt đầu)
```

**Lệnh: Servo pulse 1500 cho thiết bị 1**

```
AA  01  11  02  05  DC  XX
│   │   │   │   │   │   │
│   │   │   │   │   │   └── CRC
│   │   │   │   │   └────── DATA[1] = 0xDC (byte thấp)
│   │   │   │   └────────── DATA[0] = 0x05 (byte cao)
│   │   │   └────────────── LEN = 2 (2 bytes data)
│   │   └────────────────── CMD = 0x11 (SERVO_PULSE)
│   └────────────────────── ID = 1
└────────────────────────── SOF = 0xAA

Pulse = 0x05DC = 1500 (trong thập phân)
```

---

# 4. CÁC LỆNH (COMMANDS)

## 4.1 Bảng lệnh

```python
class CMD:
    # Điều khiển cơ bản
    ON          = 0x01    # Bật (LED, relay,...)
    OFF         = 0x02    # Tắt
    TOGGLE      = 0x03    # Đảo trạng thái
    
    # Điều khiển Servo
    SERVO_PULSE = 0x11    # Đặt pulse (500-2500)
    
    # Hệ thống
    PING        = 0x30    # Kiểm tra kết nối
    GET_STATUS  = 0x20    # Đọc trạng thái
    
    # Phản hồi
    ACK         = 0xFE    # OK
    NACK        = 0xFF    # Lỗi
```

## 4.2 Sử dụng trong code

```python
from core.rs485_protocol import CMD

# Lấy giá trị lệnh ON
led_on_command = CMD.ON  # = 0x01 = 1

# Kiểm tra phản hồi
if response_cmd == CMD.ACK:
    print("Thiết bị đã xác nhận!")
```

---

# 5. TÍNH CRC8

## 5.1 CRC là gì?

CRC = **Cyclic Redundancy Check**
= Mã kiểm tra xem dữ liệu có bị lỗi không

Giống như khi bạn nói "123" và người nghe đọc lại "123" để xác nhận.

## 5.2 Thuật toán CRC8

```python
def crc8(data: bytes) -> int:
    """
    Tính CRC8 cho dữ liệu
    
    Args:
        data: Bytes cần tính CRC
        
    Returns:
        Giá trị CRC (0-255)
    """
    crc = 0x00
    
    for byte in data:
        crc ^= byte  # XOR với byte hiện tại
        
        for _ in range(8):  # Lặp 8 lần (8 bit)
            if crc & 0x80:  # Nếu bit cao nhất = 1
                crc = (crc << 1) ^ 0x07  # Dịch trái và XOR
            else:
                crc <<= 1  # Chỉ dịch trái
            crc &= 0xFF  # Giữ 8 bit
    
    return crc
```

## 5.3 Ví dụ tính CRC

```python
# Lệnh ON cho node 1: ID=1, CMD=1, LEN=0
data = bytes([0x01, 0x01, 0x00])  # Không bao gồm SOF!
result = crc8(data)
print(f"CRC = 0x{result:02X}")  # CRC = 0x7E
```

**Lưu ý quan trọng:** CRC tính từ **ID đến hết DATA**, KHÔNG bao gồm SOF!

---

# 6. TẠO FRAME (build_frame)

## 6.1 Hàm build_frame

```python
def build_frame(node_id: int, cmd: int, data: bytes = b'') -> bytes:
    """
    Tạo frame RS485 hoàn chỉnh
    
    Args:
        node_id: Địa chỉ thiết bị (1-254)
        cmd: Lệnh cần gửi
        data: Dữ liệu kèm theo (mặc định rỗng)
        
    Returns:
        Frame hoàn chỉnh dạng bytes
    """
    # Bước 1: Tạo phần đầu
    header = bytes([
        0xAA,           # SOF
        node_id,        # ID
        cmd,            # CMD
        len(data)       # LEN
    ])
    
    # Bước 2: Ghép header + data
    frame_without_crc = header + data
    
    # Bước 3: Tính CRC (từ ID đến hết data)
    crc = crc8(frame_without_crc[1:])  # Bỏ SOF
    
    # Bước 4: Ghép frame + CRC
    complete_frame = frame_without_crc + bytes([crc])
    
    return complete_frame
```

## 6.2 Ví dụ sử dụng

### Ví dụ 1: Bật LED
```python
from core.rs485_protocol import build_frame, CMD

frame = build_frame(node_id=1, cmd=CMD.ON)
print(frame)  # b'\xaa\x01\x01\x00~'
# Hex: AA 01 01 00 7E
```

### Ví dụ 2: Servo pulse 1500
```python
pulse = 1500
# Chuyển thành 2 bytes: [byte_cao, byte_thấp]
data = bytes([pulse >> 8, pulse & 0xFF])  # [0x05, 0xDC]

frame = build_frame(1, CMD.SERVO_PULSE, data)
# Hex: AA 01 11 02 05 DC XX
```

### Ví dụ 3: Ping
```python
frame = build_frame(1, CMD.PING)
# Hex: AA 01 30 00 92
```

---

# 7. GIẢI MÃ FRAME (parse_frame)

## 7.1 Hàm parse_frame

```python
def parse_frame(data: bytes) -> dict | None:
    """
    Giải mã frame nhận được
    
    Args:
        data: Bytes nhận được
        
    Returns:
        Dictionary chứa thông tin frame, hoặc None nếu lỗi
    """
    # Kiểm tra độ dài tối thiểu
    if len(data) < 5:
        return None
    
    # Kiểm tra SOF
    if data[0] != 0xAA:
        return None
    
    # Trích xuất các phần
    node_id = data[1]
    cmd = data[2]
    data_len = data[3]
    payload = data[4:4+data_len]
    received_crc = data[4+data_len]
    
    # Tính CRC để so sánh
    calculated_crc = crc8(data[1:4+data_len])
    
    return {
        'id': node_id,
        'cmd': cmd,
        'data': payload,
        'crc_ok': received_crc == calculated_crc
    }
```

## 7.2 Ví dụ sử dụng

```python
# Nhận được: AA 01 FE 00 A9
received = bytes([0xAA, 0x01, 0xFE, 0x00, 0xA9])

result = parse_frame(received)

if result:
    print(f"ID: {result['id']}")       # ID: 1
    print(f"CMD: {result['cmd']}")     # CMD: 254 (0xFE = ACK)
    print(f"CRC OK: {result['crc_ok']}")  # CRC OK: True
    
    if result['cmd'] == CMD.ACK:
        print("Thiết bị xác nhận thành công!")
```

---

# 8. GỬI/NHẬN QUA SERIAL

## 8.1 Kết nối Serial

```python
import serial

# Mở kết nối
port = serial.Serial(
    port='COM3',      # Tên cổng (thay đổi theo máy)
    baudrate=115200,  # Tốc độ
    timeout=1         # Timeout 1 giây
)

print("Đã kết nối!")
```

## 8.2 Gửi Frame

```python
def send_frame(port, node_id, cmd, data=b''):
    """Gửi frame qua serial"""
    frame = build_frame(node_id, cmd, data)
    port.write(frame)
    print(f"Đã gửi: {frame.hex(' ').upper()}")
```

**Ví dụ:**
```python
send_frame(port, 1, CMD.ON)
# Đã gửi: AA 01 01 00 7E
```

## 8.3 Nhận Frame

```python
def receive_frame(port, timeout=1.0):
    """Nhận và giải mã frame"""
    import time
    
    buffer = bytearray()
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if port.in_waiting > 0:
            buffer.extend(port.read(port.in_waiting))
            
            # Tìm SOF
            while len(buffer) > 0 and buffer[0] != 0xAA:
                buffer.pop(0)
            
            # Kiểm tra đủ header
            if len(buffer) >= 4:
                data_len = buffer[3]
                expected_len = 4 + data_len + 1
                
                if len(buffer) >= expected_len:
                    frame = bytes(buffer[:expected_len])
                    return parse_frame(frame)
        
        time.sleep(0.01)
    
    return None
```

---

# 9. TÍCH HỢP VÀO APP

## 9.1 Trong app_designer.py

```python
from core.rs485_protocol import build_frame, CMD

def _send_command(self):
    """Xử lý khi user gõ lệnh"""
    
    cmd_text = self.QLine_CMD.text().strip().lower()
    
    # Parse lệnh text thành frame
    if cmd_text == 'led on':
        frame = build_frame(1, CMD.ON)
        
    elif cmd_text.startswith('servo '):
        pulse = int(cmd_text.split()[1])
        data = bytes([pulse >> 8, pulse & 0xFF])
        frame = build_frame(1, CMD.SERVO_PULSE, data)
    
    # Gửi qua serial
    self.serial.send_binary(frame)
```

## 9.2 Các lệnh Terminal hỗ trợ

| Gõ | Tác dụng |
|----|----------|
| `ping` | Kiểm tra kết nối |
| `on` hoặc `led on` | Bật LED |
| `off` hoặc `led off` | Tắt LED |
| `toggle` | Đảo LED |
| `servo 1500` | Servo pulse 1500 |
| `status` | Đọc trạng thái |

---

# 10. VÍ DỤ HOÀN CHỈNH

## 10.1 Script đơn giản

```python
"""
script_test.py - Test RS485 đơn giản
"""
import serial
import time
from core.rs485_protocol import build_frame, parse_frame, CMD

# 1. Kết nối
port = serial.Serial('COM3', 115200, timeout=1)
print("✓ Đã kết nối COM3")

# 2. Gửi PING
frame = build_frame(1, CMD.PING)
port.write(frame)
print(f"→ Gửi PING: {frame.hex(' ').upper()}")

# 3. Chờ phản hồi
time.sleep(0.5)
if port.in_waiting > 0:
    response = port.read(port.in_waiting)
    result = parse_frame(response)
    
    if result and result['crc_ok']:
        if result['cmd'] == CMD.ACK:
            print("← Nhận ACK - Thiết bị hoạt động!")
        else:
            print(f"← Nhận: {hex(result['cmd'])}")
else:
    print("✗ Không có phản hồi")

# 4. Bật LED
frame = build_frame(1, CMD.ON)
port.write(frame)
print(f"→ Gửi LED ON: {frame.hex(' ').upper()}")
time.sleep(0.5)

# 5. Đọc phản hồi
if port.in_waiting > 0:
    response = port.read(port.in_waiting)
    print(f"← Nhận: {response.hex(' ').upper()}")

# 6. Đóng kết nối
port.close()
print("✓ Đã đóng kết nối")
```

## 10.2 Kết quả mong đợi

```
✓ Đã kết nối COM3
→ Gửi PING: AA 01 30 00 92
← Nhận ACK - Thiết bị hoạt động!
→ Gửi LED ON: AA 01 01 00 7E
← Nhận: AA 01 FE 00 A9
✓ Đã đóng kết nối
```

---

# 📌 TÓM TẮT

1. **Frame** = Gói tin có cấu trúc: `SOF + ID + CMD + LEN + DATA + CRC`
2. **SOF** = 0xAA luôn ở đầu
3. **CRC** = Mã kiểm tra lỗi, tính từ ID đến DATA
4. **build_frame()** = Tạo frame từ thông tin
5. **parse_frame()** = Giải mã frame nhận được
6. **send_binary()** = Gửi bytes qua serial

---

*Tài liệu này là một phần của dự án SuperApp RS485*
