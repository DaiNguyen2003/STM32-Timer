# 📖 HƯỚNG DẪN CHI TIẾT: RS485 PROTOCOL & UART DRIVER
## Dành cho STM32 Firmware

---

# 📁 CẤU TRÚC FILE

```
Core/
├── Inc/
│   ├── rs485_protocol.h    ← Định nghĩa Protocol
│   └── rs485_uart.h        ← Định nghĩa UART Driver
└── Src/
    ├── rs485_protocol.c    ← Triển khai Protocol
    └── rs485_uart.c        ← Triển khai UART Driver
```

---

# 📋 PHẦN 1: rs485_protocol.h

## 1.1 Mục đích
Định nghĩa tất cả **constants**, **commands**, **data structures** cho protocol RS485.

---

## 1.2 CONSTANTS (Hằng số)

```c
#define RS485_SOF           0xAA    // Start of Frame
#define RS485_BROADCAST_ID  0xFF    // Broadcast to all nodes
#define RS485_MAX_DATA_LEN  255     // Max data length
#define RS485_MIN_FRAME_LEN 5       // SOF + ID + CMD + LEN + CRC
```

| Tên | Giá trị | Ý nghĩa |
|-----|---------|---------|
| `RS485_SOF` | 0xAA | Byte đầu tiên của mọi frame |
| `RS485_BROADCAST_ID` | 0xFF | Gửi cho TẤT CẢ nodes |
| `RS485_MAX_DATA_LEN` | 255 | Data tối đa 255 bytes |
| `RS485_MIN_FRAME_LEN` | 5 | Frame ngắn nhất (không có data) |

---

## 1.3 COMMANDS (Enum lệnh)

```c
typedef enum {
    // === Basic Control ===
    CMD_ON          = 0x01,     // Bật thiết bị
    CMD_OFF         = 0x02,     // Tắt thiết bị
    CMD_TOGGLE      = 0x03,     // Toggle trạng thái
    
    // === Servo Control ===
    CMD_SERVO_ANGLE = 0x10,     // Set góc servo
    CMD_SERVO_PULSE = 0x11,     // Set pulse trực tiếp (500-2500)
    
    // === Status ===
    CMD_GET_STATUS  = 0x20,     // Đọc trạng thái
    CMD_GET_ID      = 0x21,     // Đọc ID node
    
    // === System ===
    CMD_PING        = 0x30,     // Ping kiểm tra kết nối
    CMD_RESET       = 0x31,     // Reset node
    CMD_SET_ID      = 0x32,     // Đặt ID mới
    
    // === Responses ===
    CMD_ACK         = 0xFE,     // OK - Xác nhận thành công
    CMD_NACK        = 0xFF,     // ERROR - Xác nhận lỗi
    
} RS485_CMD_t;
```

**Phân loại:**
- `0x01-0x0F`: Basic Control (ON, OFF, TOGGLE)
- `0x10-0x1F`: Servo Control
- `0x20-0x2F`: Status queries
- `0x30-0x4F`: System commands
- `0xFE-0xFF`: Responses

---

## 1.4 RS485_Frame_t (Cấu trúc Frame)

```c
typedef struct {
    uint8_t id;                         // Node ID
    uint8_t cmd;                        // Command
    uint8_t len;                        // Data length
    uint8_t data[RS485_MAX_DATA_LEN];   // Data buffer
    uint8_t crc;                        // CRC8
    uint8_t valid;                      // Frame valid flag
} RS485_Frame_t;
```

| Field | Type | Size | Mô tả |
|-------|------|------|-------|
| `id` | uint8_t | 1 byte | ID của node đích |
| `cmd` | uint8_t | 1 byte | Lệnh cần thực hiện |
| `len` | uint8_t | 1 byte | Số bytes trong data |
| `data` | uint8_t[] | 0-255 bytes | Dữ liệu kèm theo |
| `crc` | uint8_t | 1 byte | Checksum CRC8 |
| `valid` | uint8_t | 1 byte | 1 = hợp lệ, 0 = lỗi |

---

## 1.5 RS485_ParserState_t (Trạng thái Parser)

```c
typedef enum {
    PARSER_IDLE,        // Đợi SOF
    PARSER_GOT_SOF,     // Nhận được SOF, đợi ID
    PARSER_GOT_ID,      // Nhận được ID, đợi CMD
    PARSER_GOT_CMD,     // Nhận được CMD, đợi LEN
    PARSER_GOT_LEN,     // Nhận được LEN, đợi DATA
    PARSER_GOT_DATA,    // Nhận đủ DATA, đợi CRC
    PARSER_COMPLETE,    // Frame hoàn chỉnh
    PARSER_ERROR,       // Lỗi
} RS485_ParserState_t;
```

**Sơ đồ chuyển trạng thái:**
```
IDLE ──(0xAA)──► SOF ──(byte)──► ID ──(byte)──► CMD
                                                  │
        COMPLETE ◄──(CRC)── DATA ◄──(len bytes)── LEN
            │        ▲              ▲
            │        └──(len=0)─────┘
            └────────(reset)────► IDLE
```

---

## 1.6 RS485_Parser_t (Struct Parser)

```c
typedef struct {
    RS485_ParserState_t state;    // Trạng thái hiện tại
    RS485_Frame_t frame;          // Frame đang build
    uint8_t data_index;           // Index trong data buffer
} RS485_Parser_t;
```

---

## 1.7 Function Prototypes

```c
// Tính CRC8
uint8_t RS485_CRC8(uint8_t *data, uint8_t len);

// Khởi tạo parser
void RS485_ParserInit(RS485_Parser_t *parser);

// Feed byte vào parser, return 1 nếu frame complete
uint8_t RS485_ParserFeed(RS485_Parser_t *parser, uint8_t byte);

// Build frame để gửi, return tổng length
uint8_t RS485_BuildFrame(uint8_t *buffer, uint8_t id, uint8_t cmd, 
                         uint8_t *data, uint8_t len);
```

---

# 📋 PHẦN 2: rs485_protocol.c

## 2.1 RS485_CRC8 - Tính Checksum

```c
uint8_t RS485_CRC8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;
    
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];             // XOR với byte hiện tại
        for (uint8_t j = 0; j < 8; j++)
        {
            if (crc & 0x80)         // Bit cao nhất = 1?
                crc = (crc << 1) ^ 0x07;  // Shift + XOR polynomial
            else
                crc <<= 1;          // Chỉ shift
        }
    }
     
    return crc;
}
```

**Giải thích:**
- Polynomial: 0x07 (CRC-8)
- Dùng bit-wise XOR và shift
- Kết quả: 1 byte (0-255)

**Ví dụ:**
```c
uint8_t data[] = {0x01, 0x01, 0x00};  // ID=1, CMD=ON, LEN=0
uint8_t crc = RS485_CRC8(data, 3);
// crc = 0x7E
```

---

## 2.2 RS485_ParserInit - Khởi tạo Parser

```c
void RS485_ParserInit(RS485_Parser_t *parser)
{
    parser->state = PARSER_IDLE;      // Về trạng thái đợi
    parser->data_index = 0;           // Reset index
    memset(&parser->frame, 0, sizeof(RS485_Frame_t));  // Clear frame
}
```

**Khi nào gọi:**
- Khi bắt đầu chương trình
- Sau khi xử lý xong frame
- Khi có lỗi cần reset

---

## 2.3 RS485_ParserFeed - Xử lý từng byte

```c
uint8_t RS485_ParserFeed(RS485_Parser_t *parser, uint8_t byte)
{
    switch (parser->state)
    {
        case PARSER_IDLE:
            if (byte == RS485_SOF)      // Tìm thấy 0xAA?
            {
                RS485_ParserInit(parser);
                parser->state = PARSER_GOT_SOF;
            }
            break;
            
        case PARSER_GOT_SOF:
            parser->frame.id = byte;    // Lưu ID
            parser->state = PARSER_GOT_ID;
            break;
            
        case PARSER_GOT_ID:
            parser->frame.cmd = byte;   // Lưu CMD
            parser->state = PARSER_GOT_CMD;
            break;
            
        case PARSER_GOT_CMD:
            parser->frame.len = byte;   // Lưu LEN
            parser->data_index = 0;
            if (byte == 0)
                parser->state = PARSER_GOT_DATA;   // Không có data
            else
                parser->state = PARSER_GOT_LEN;    // Chờ data
            break;
            
        case PARSER_GOT_LEN:
            parser->frame.data[parser->data_index++] = byte;
            if (parser->data_index >= parser->frame.len)
                parser->state = PARSER_GOT_DATA;   // Đủ data
            break;
            
        case PARSER_GOT_DATA:
            // Tính CRC và so sánh
            uint8_t crc_data[258];
            crc_data[0] = parser->frame.id;
            crc_data[1] = parser->frame.cmd;
            crc_data[2] = parser->frame.len;
            memcpy(&crc_data[3], parser->frame.data, parser->frame.len);
            
            uint8_t expected = RS485_CRC8(crc_data, 3 + parser->frame.len);
            parser->frame.crc = byte;
            parser->frame.valid = (byte == expected);
            parser->state = PARSER_COMPLETE;
            
            return parser->frame.valid;  // ← 1 nếu CRC đúng
            
        case PARSER_COMPLETE:
            RS485_ParserInit(parser);
            if (byte == RS485_SOF)
                parser->state = PARSER_GOT_SOF;
            break;
    }
    
    return 0;  // Frame chưa hoàn chỉnh
}
```

**Return value:**
- `0`: Frame chưa complete, tiếp tục nhận
- `1`: Frame complete và CRC đúng → Xử lý!
- Nếu CRC sai: return 1 nhưng `frame.valid = 0`

---

## 2.4 RS485_BuildFrame - Tạo Frame

```c
uint8_t RS485_BuildFrame(uint8_t *buffer, uint8_t id, uint8_t cmd, 
                         uint8_t *data, uint8_t len)
{
    uint8_t index = 0;
    
    // Header
    buffer[index++] = RS485_SOF;    // 0xAA
    buffer[index++] = id;           // Node ID
    buffer[index++] = cmd;          // Command
    buffer[index++] = len;          // Data length
    
    // Data
    if (data != NULL && len > 0)
    {
        memcpy(&buffer[index], data, len);
        index += len;
    }
    
    // CRC (từ ID đến hết DATA)
    buffer[index] = RS485_CRC8(&buffer[1], 3 + len);
    index++;
    
    return index;  // Tổng length của frame
}
```

**Ví dụ:**
```c
uint8_t tx_buffer[64];
uint8_t len = RS485_BuildFrame(tx_buffer, 1, CMD_ACK, NULL, 0);
// tx_buffer = {0xAA, 0x01, 0xFE, 0x00, 0xA9}
// len = 5
```

---

# 📋 PHẦN 3: rs485_uart.h

## 3.1 Configuration

```c
#define RS485_NODE_ID       1       // ID của node này
#define RS485_RX_BUFFER_SIZE 64     // Buffer size
```

**Lưu ý:** Thay đổi `RS485_NODE_ID` cho mỗi board khác nhau!

---

## 3.2 Function Prototypes

```c
// Khởi tạo
void RS485_UART_Init(UART_HandleTypeDef *huart);

// Gửi frame
void RS485_UART_SendFrame(uint8_t id, uint8_t cmd, uint8_t *data, uint8_t len);

// Gửi response
void RS485_UART_SendAck(void);
void RS485_UART_SendNack(void);

// Xử lý byte nhận (gọi từ interrupt)
void RS485_UART_RxCallback(uint8_t byte);

// Kiểm tra và lấy frame
uint8_t RS485_UART_HasNewFrame(void);
RS485_Frame_t* RS485_UART_GetFrame(void);
void RS485_UART_ClearFrameFlag(void);
```

---

# 📋 PHẦN 4: rs485_uart.c

## 4.1 Private Variables

```c
static UART_HandleTypeDef *_huart = NULL;  // Handle UART
static RS485_Parser_t _parser;              // Parser instance
static volatile uint8_t _new_frame_flag = 0;// Flag có frame mới
static uint8_t _tx_buffer[RS485_RX_BUFFER_SIZE]; // Buffer gửi
```

**Giải thích từ khóa:**
- `static`: Chỉ visible trong file này
- `volatile`: Biến có thể thay đổi từ interrupt

---

## 4.2 RS485_UART_Init

```c
void RS485_UART_Init(UART_HandleTypeDef *huart)
{
    _huart = huart;              // Lưu handle
    RS485_ParserInit(&_parser);  // Init parser
    _new_frame_flag = 0;         // Clear flag
}
```

**Gọi trong main.c:**
```c
RS485_UART_Init(&huart1);
```

---

## 4.3 RS485_UART_SendFrame

```c
void RS485_UART_SendFrame(uint8_t id, uint8_t cmd, uint8_t *data, uint8_t len)
{
    if (_huart == NULL) return;
    
    uint8_t frame_len = RS485_BuildFrame(_tx_buffer, id, cmd, data, len);
    HAL_UART_Transmit(_huart, _tx_buffer, frame_len, 100);
}
```

**Luồng:**
1. Build frame vào `_tx_buffer`
2. Gửi qua `HAL_UART_Transmit()` (blocking, timeout 100ms)

---

## 4.4 RS485_UART_SendAck / SendNack

```c
void RS485_UART_SendAck(void)
{
    RS485_UART_SendFrame(RS485_NODE_ID, CMD_ACK, NULL, 0);
}

void RS485_UART_SendNack(void)
{
    RS485_UART_SendFrame(RS485_NODE_ID, CMD_NACK, NULL, 0);
}
```

**Tiện ích:** Gọi nhanh để phản hồi thành công/thất bại.

---

## 4.5 RS485_UART_RxCallback (QUAN TRỌNG!)

```c
void RS485_UART_RxCallback(uint8_t byte)
{
    if (RS485_ParserFeed(&_parser, byte))  // Frame complete?
    {
        // Kiểm tra ID của frame
        if (_parser.frame.id == RS485_NODE_ID ||     // Gửi cho mình
            _parser.frame.id == RS485_BROADCAST_ID)  // Hoặc broadcast
        {
            _new_frame_flag = 1;  // Báo có frame mới!
        }
    }
}
```

**Gọi từ đâu?** Từ UART Interrupt Callback trong main.c:

```c
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        RS485_UART_RxCallback(rx_byte);
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);  // Tiếp tục nhận
    }
}
```

---

## 4.6 HasNewFrame / GetFrame / ClearFrameFlag

```c
uint8_t RS485_UART_HasNewFrame(void)
{
    return _new_frame_flag;
}

RS485_Frame_t* RS485_UART_GetFrame(void)
{
    return &_parser.frame;
}

void RS485_UART_ClearFrameFlag(void)
{
    _new_frame_flag = 0;
}
```

**Sử dụng trong main loop:**

```c
while (1)
{
    if (RS485_UART_HasNewFrame())
    {
        RS485_Frame_t *frame = RS485_UART_GetFrame();
        // Xử lý frame->cmd...
        RS485_UART_ClearFrameFlag();  // Nhớ clear!
    }
}
```

---

# 🔄 SƠ ĐỒ TỔNG QUAN

```
┌─────────────────────────────────────────────────────────────┐
│                     UART HARDWARE                            │
│  PA10 (RX) ◄──────────────────────────────────────────────   │
└──────────────────────────────┬──────────────────────────────┘
                               │ Interrupt mỗi byte
                               ▼
┌─────────────────────────────────────────────────────────────┐
│  HAL_UART_RxCpltCallback()   (main.c)                       │
│      RS485_UART_RxCallback(byte);                           │
└──────────────────────────────┬──────────────────────────────┘
                               │
                               ▼
┌─────────────────────────────────────────────────────────────┐
│  RS485_ParserFeed()   (rs485_protocol.c)                    │
│      Ghép byte → Frame                                      │
│      Kiểm tra CRC                                           │
└──────────────────────────────┬──────────────────────────────┘
                               │ Frame complete: set flag
                               ▼
┌─────────────────────────────────────────────────────────────┐
│  Main Loop   (main.c)                                       │
│      if (RS485_UART_HasNewFrame())                          │
│          GetFrame() → switch(cmd) → SendAck()               │
└─────────────────────────────────────────────────────────────┘
```

---

*Tài liệu này là một phần của dự án SuperApp RS485*
