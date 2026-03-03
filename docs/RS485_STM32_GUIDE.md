# 🔧 HƯỚNG DẪN RS485 - STM32 FIRMWARE
## Dành cho người mới bắt đầu

---

# 📖 MỤC LỤC

1. [Tổng quan](#1-tổng-quan)
2. [Cấu trúc thư mục](#2-cấu-trúc-thư-mục)
3. [Luồng hoạt động](#3-luồng-hoạt-động)
4. [Định nghĩa Protocol (rs485_protocol.h)](#4-định-nghĩa-protocol)
5. [Parser State Machine (rs485_protocol.c)](#5-parser-state-machine)
6. [UART Driver (rs485_uart.c)](#6-uart-driver)
7. [Xử lý Command (main.c)](#7-xử-lý-command)
8. [Cấu hình CubeMX](#8-cấu-hình-cubemx)
9. [Debug và Test](#9-debug-và-test)
10. [Ví dụ hoàn chỉnh](#10-ví-dụ-hoàn-chỉnh)

---

# 1. TỔNG QUAN

## 1.1 STM32 làm gì?

STM32 đóng vai trò **thiết bị slave**, chờ nhận lệnh từ PC:

```
    PC gửi lệnh          STM32 nhận và xử lý
         │                        │
         ▼                        ▼
┌─────────────┐           ┌─────────────┐
│  LED ON     │  ──────►  │  Bật LED    │
│  AA 01 01   │           │  GPIO LOW   │
└─────────────┘           └─────────────┘
         │                        │
         │                        ▼
         │                ┌─────────────┐
         ◄────────────────│  Gửi ACK    │
                          │  AA 01 FE   │
                          └─────────────┘
```

## 1.2 Các bước xử lý

```
1. UART nhận byte    → Interrupt callback
2. Parser ghép frame → State machine
3. Xử lý command     → Switch case
4. Gửi phản hồi      → UART transmit
```

---

# 2. CẤU TRÚC THƯ MỤC

```
Digital_Servo/
├── Core/
│   ├── Inc/                    ← Header files
│   │   ├── main.h
│   │   ├── rs485_protocol.h    ← Định nghĩa protocol
│   │   └── rs485_uart.h        ← UART driver
│   ├── Src/                    ← Source files
│   │   ├── main.c              ← Xử lý command
│   │   ├── rs485_protocol.c    ← Parser
│   │   └── rs485_uart.c        ← UART driver
│   └── Startup/
│       └── startup_stm32f103c8tx.s
├── Drivers/                    ← HAL library
└── Digital_Servo.ioc           ← CubeMX config
```

| File | Tác dụng |
|------|----------|
| `rs485_protocol.h` | Định nghĩa commands, frame structure |
| `rs485_protocol.c` | Parser state machine + CRC8 |
| `rs485_uart.h/c` | UART driver wrapper |
| `main.c` | Command handler |

---

# 3. LUỒNG HOẠT ĐỘNG

## 3.1 Sơ đồ chi tiết

```
┌──────────────────────────────────────────────────────────┐
│                     UART HARDWARE                         │
│  PA10 (RX) nhận từng byte qua Interrupt                  │
└────────────────────────┬─────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│              HAL_UART_RxCpltCallback()                   │
│  Mỗi byte nhận được → Gọi RS485_UART_RxCallback()        │
└────────────────────────┬─────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│              RS485_ParserFeed(byte)                      │
│  Ghép các byte thành frame hoàn chỉnh                    │
│  IDLE → SOF → ID → CMD → LEN → DATA → CRC → COMPLETE    │
└────────────────────────┬─────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│              RS485_UART_HasNewFrame()                    │
│  Kiểm tra có frame mới không (gọi trong main loop)       │
└────────────────────────┬─────────────────────────────────┘
                         │
                         ▼
┌──────────────────────────────────────────────────────────┐
│              switch(frame->cmd)                          │
│  Thực hiện lệnh tương ứng                                │
│  CMD_ON → Bật LED                                        │
│  CMD_SERVO_PULSE → Set PWM                               │
└──────────────────────────────────────────────────────────┘
```

## 3.2 Timeline

```
Thời gian →

    ┌─────┐ ┌────┐ ┌─────┐ ┌─────┐ ┌─────┐
RX: │0xAA │ │0x01│ │0x01 │ │0x00 │ │0x7E │
    └──┬──┘ └──┬─┘ └──┬──┘ └──┬──┘ └──┬──┘
       │       │      │       │       │
       ▼       ▼      ▼       ▼       ▼
State: IDLE → SOF → ID → CMD → LEN → CRC
                                      │
                                      ▼
                              COMPLETE! → Xử lý
```

---

# 4. ĐỊNH NGHĨA PROTOCOL

## 4.1 rs485_protocol.h - Giải thích

### Constants (Hằng số)

```c
#define RS485_SOF           0xAA    // Byte bắt đầu frame
#define RS485_BROADCAST_ID  0xFF    // ID gửi cho tất cả
#define RS485_MAX_DATA_LEN  255     // Data tối đa 255 bytes
#define RS485_MIN_FRAME_LEN 5       // Frame tối thiểu
```

### Commands (Các lệnh)

```c
typedef enum {
    // Điều khiển cơ bản
    CMD_ON          = 0x01,     // Bật thiết bị
    CMD_OFF         = 0x02,     // Tắt thiết bị
    CMD_TOGGLE      = 0x03,     // Đảo trạng thái
    
    // Servo
    CMD_SERVO_PULSE = 0x11,     // Set pulse PWM
    
    // Hệ thống
    CMD_PING        = 0x30,     // Ping
    CMD_GET_STATUS  = 0x20,     // Đọc trạng thái
    
    // Phản hồi
    CMD_ACK         = 0xFE,     // OK
    CMD_NACK        = 0xFF,     // Lỗi
} RS485_CMD_t;
```

### Frame Structure (Cấu trúc Frame)

```c
typedef struct {
    uint8_t id;       // ID thiết bị
    uint8_t cmd;      // Lệnh
    uint8_t len;      // Độ dài data
    uint8_t data[255];// Dữ liệu
    uint8_t crc;      // Checksum
    uint8_t valid;    // Frame hợp lệ?
} RS485_Frame_t;
```

---

# 5. PARSER STATE MACHINE

## 5.1 Parser là gì?

**Parser** = Bộ phân tích, ghép các byte đơn lẻ thành frame hoàn chỉnh.

Giống như khi bạn nhận từng chữ cái qua điện thoại và ghép thành câu.

## 5.2 Các trạng thái

```c
typedef enum {
    PARSER_IDLE,      // Chờ byte SOF (0xAA)
    PARSER_GOT_SOF,   // Đã nhận SOF, chờ ID
    PARSER_GOT_ID,    // Đã nhận ID, chờ CMD
    PARSER_GOT_CMD,   // Đã nhận CMD, chờ LEN
    PARSER_GOT_LEN,   // Đã nhận LEN, chờ DATA
    PARSER_GOT_DATA,  // Đã nhận DATA, chờ CRC
    PARSER_COMPLETE,  // Frame hoàn chỉnh!
} RS485_ParserState_t;
```

## 5.3 Sơ đồ state machine

```
         0xAA
┌──────┐ ───► ┌─────────┐
│ IDLE │      │ GOT_SOF │
└──────┘ ◄─── └────┬────┘
    ▲        byte   │ byte
    │  khác         ▼
    │        ┌──────────┐
    │        │ GOT_ID   │
    │        └────┬─────┘
    │             │ byte
    │             ▼
    │        ┌──────────┐
    │        │ GOT_CMD  │
    │        └────┬─────┘
    │             │ byte
    │             ▼
    │        ┌──────────┐   len=0
    │        │ GOT_LEN  │─────────┐
    │        └────┬─────┘         │
    │             │ len>0         │
    │             ▼               ▼
    │        ┌──────────┐   ┌──────────┐
    │        │ RECEIVING│   │ GOT_DATA │
    │        │ DATA     │──►│          │
    │        └──────────┘   └────┬─────┘
    │           đủ data          │ CRC
    │                            ▼
    │                      ┌──────────┐
    └──────────────────────│ COMPLETE │
           reset           └──────────┘
```

## 5.4 Code RS485_ParserFeed()

```c
uint8_t RS485_ParserFeed(RS485_Parser_t *parser, uint8_t byte)
{
    switch (parser->state)
    {
        // ═══════════════════════════════════════════
        // Trạng thái 1: Đang chờ SOF
        // ═══════════════════════════════════════════
        case PARSER_IDLE:
            if (byte == RS485_SOF)  // Nhận được 0xAA?
            {
                RS485_ParserInit(parser);  // Reset parser
                parser->state = PARSER_GOT_SOF;  // Chuyển state
            }
            // Nếu không phải 0xAA → bỏ qua
            break;
            
        // ═══════════════════════════════════════════
        // Trạng thái 2: Đã có SOF, chờ ID
        // ═══════════════════════════════════════════
        case PARSER_GOT_SOF:
            parser->frame.id = byte;  // Lưu ID
            parser->state = PARSER_GOT_ID;
            break;
            
        // ═══════════════════════════════════════════
        // Trạng thái 3: Đã có ID, chờ CMD
        // ═══════════════════════════════════════════
        case PARSER_GOT_ID:
            parser->frame.cmd = byte;  // Lưu CMD
            parser->state = PARSER_GOT_CMD;
            break;
            
        // ═══════════════════════════════════════════
        // Trạng thái 4: Đã có CMD, chờ LEN
        // ═══════════════════════════════════════════
        case PARSER_GOT_CMD:
            parser->frame.len = byte;  // Lưu LEN
            parser->data_index = 0;    // Reset index
            
            if (byte == 0)
                parser->state = PARSER_GOT_DATA;  // Không có data
            else
                parser->state = PARSER_GOT_LEN;   // Chờ nhận data
            break;
            
        // ═══════════════════════════════════════════
        // Trạng thái 5: Đang nhận DATA
        // ═══════════════════════════════════════════
        case PARSER_GOT_LEN:
            parser->frame.data[parser->data_index++] = byte;
            
            if (parser->data_index >= parser->frame.len)
                parser->state = PARSER_GOT_DATA;  // Đủ data
            break;
            
        // ═══════════════════════════════════════════
        // Trạng thái 6: Đã có DATA, chờ CRC
        // ═══════════════════════════════════════════
        case PARSER_GOT_DATA:
            {
                // Tính CRC mong đợi
                uint8_t crc_data[3 + 255];
                crc_data[0] = parser->frame.id;
                crc_data[1] = parser->frame.cmd;
                crc_data[2] = parser->frame.len;
                memcpy(&crc_data[3], parser->frame.data, 
                       parser->frame.len);
                
                uint8_t expected_crc = RS485_CRC8(
                    crc_data, 
                    3 + parser->frame.len
                );
                
                // So sánh CRC
                parser->frame.crc = byte;
                parser->frame.valid = (byte == expected_crc);
                parser->state = PARSER_COMPLETE;
                
                return parser->frame.valid;  // 1 = OK
            }
        
        // ═══════════════════════════════════════════
        // Trạng thái 7: Frame complete, reset
        // ═══════════════════════════════════════════
        case PARSER_COMPLETE:
            RS485_ParserInit(parser);
            if (byte == RS485_SOF)
                parser->state = PARSER_GOT_SOF;
            break;
    }
    
    return 0;  // Chưa hoàn thành
}
```

---

# 6. UART DRIVER

## 6.1 rs485_uart.h - API

```c
// Khởi tạo
void RS485_UART_Init(UART_HandleTypeDef *huart);

// Gửi frame
void RS485_UART_SendFrame(uint8_t id, uint8_t cmd, 
                          uint8_t *data, uint8_t len);

// Gửi ACK/NACK
void RS485_UART_SendAck(void);
void RS485_UART_SendNack(void);

// Kiểm tra frame mới
uint8_t RS485_UART_HasNewFrame(void);

// Lấy frame
RS485_Frame_t* RS485_UART_GetFrame(void);

// Xóa flag
void RS485_UART_ClearFrameFlag(void);
```

## 6.2 Callback Interrupt

```c
// Trong main.c - Gọi mỗi khi nhận 1 byte
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        // Đưa byte vào parser
        RS485_UART_RxCallback(rx_byte);
        
        // Tiếp tục nhận byte tiếp theo
        HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    }
}
```

## 6.3 Giải thích

```
                    ┌─────────────────┐
 UART Hardware ───► │ HAL_UART_Receive│
                    │    _IT()        │
                    └────────┬────────┘
                             │ Interrupt khi có byte
                             ▼
                    ┌─────────────────┐
                    │ HAL_UART_RxCplt │
                    │   Callback()    │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ RS485_UART_     │
                    │  RxCallback()   │
                    └────────┬────────┘
                             │
                             ▼
                    ┌─────────────────┐
                    │ RS485_Parser    │
                    │   Feed()        │
                    └─────────────────┘
```

---

# 7. XỬ LÝ COMMAND

## 7.1 Main Loop

```c
int main(void)
{
    // Khởi tạo
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_USART1_UART_Init();
    
    // Khởi tạo RS485
    RS485_UART_Init(&huart1);
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
    
    // Main loop
    while (1)
    {
        // Kiểm tra có frame mới?
        if (RS485_UART_HasNewFrame())
        {
            // Xử lý frame
            ProcessCommand();
        }
        
        HAL_Delay(10);
    }
}
```

## 7.2 Command Handler

```c
void ProcessCommand(void)
{
    RS485_Frame_t *frame = RS485_UART_GetFrame();
    
    switch (frame->cmd)
    {
        // ═══════════════════════════════════════
        // Lệnh: BẬT LED
        // ═══════════════════════════════════════
        case CMD_ON:
            // PC13 active LOW → RESET = ON
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            led_state = 1;
            RS485_UART_SendAck();  // Phản hồi OK
            break;
            
        // ═══════════════════════════════════════
        // Lệnh: TẮT LED
        // ═══════════════════════════════════════
        case CMD_OFF:
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            led_state = 0;
            RS485_UART_SendAck();
            break;
            
        // ═══════════════════════════════════════
        // Lệnh: SERVO PULSE
        // ═══════════════════════════════════════
        case CMD_SERVO_PULSE:
            if (frame->len >= 2)
            {
                // Ghép 2 bytes thành số 16-bit
                uint16_t pulse = (frame->data[0] << 8) 
                                | frame->data[1];
                
                // Kiểm tra phạm vi
                if (pulse >= 500 && pulse <= 2500)
                {
                    // Set PWM
                    __HAL_TIM_SET_COMPARE(&htim2, 
                                          TIM_CHANNEL_1, 
                                          pulse);
                    RS485_UART_SendAck();
                }
                else
                {
                    RS485_UART_SendNack();  // Ngoài phạm vi
                }
            }
            else
            {
                RS485_UART_SendNack();  // Thiếu data
            }
            break;
            
        // ═══════════════════════════════════════
        // Lệnh: PING
        // ═══════════════════════════════════════
        case CMD_PING:
            RS485_UART_SendAck();  // Đơn giản trả lời ACK
            break;
            
        // ═══════════════════════════════════════
        // Lệnh không hỗ trợ
        // ═══════════════════════════════════════
        default:
            RS485_UART_SendNack();
            break;
    }
    
    // Xóa flag để sẵn sàng nhận frame tiếp
    RS485_UART_ClearFrameFlag();
}
```

---

# 8. CẤU HÌNH CUBEMX

## 8.1 Cấu hình USART1

```
Connectivity → USART1 → Mode: Asynchronous

Parameter Settings:
├── Baud Rate: 115200
├── Word Length: 8 Bits
├── Parity: None
└── Stop Bits: 1

Pins:
├── PA9  → USART1_TX
└── PA10 → USART1_RX
```

## 8.2 Cấu hình NVIC (Interrupt)

```
System Core → NVIC

✓ USART1 global interrupt → Enabled
  Priority: 1 (thấp hơn SysTick)
```

## 8.3 Cấu hình TIM2 (cho Servo)

```
Timers → TIM2 → PWM Generation CH1

Parameter Settings:
├── Prescaler: 71 (72MHz/72 = 1MHz)
├── Period: 19999 (50Hz = 20ms)
└── Pulse: 1500 (neutral position)

Pin:
└── PA0 → TIM2_CH1
```

---

# 9. DEBUG VÀ TEST

## 9.1 LED Debug

Thêm LED blink khi nhận frame:

```c
case CMD_PING:
    // Blink LED 2 lần để debug
    for (int i = 0; i < 4; i++)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        HAL_Delay(50);
    }
    RS485_UART_SendAck();
    break;
```

## 9.2 Sử dụng printf

```c
// Redirect printf to UART
int _write(int file, char *data, int len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 100);
    return len;
}

// Trong ProcessCommand:
case CMD_PING:
    printf("Received PING!\r\n");
    RS485_UART_SendAck();
    break;
```

---

# 10. VÍ DỤ HOÀN CHỈNH

## 10.1 Nhận lệnh LED ON

```
Timeline:
─────────────────────────────────────────────────────────

1. PC gửi: AA 01 01 00 7E
              │  │  │  │  └── CRC
              │  │  │  └───── LEN=0
              │  │  └──────── CMD=ON
              │  └─────────── ID=1
              └────────────── SOF

2. UART Interrupt nhận từng byte:
   → 0xAA: State = GOT_SOF
   → 0x01: State = GOT_ID,  frame.id = 1
   → 0x01: State = GOT_CMD, frame.cmd = 1
   → 0x00: State = GOT_DATA (len=0, không có data)
   → 0x7E: State = COMPLETE, CRC OK!

3. Main loop:
   → RS485_UART_HasNewFrame() = TRUE
   → ProcessCommand()
   → case CMD_ON: Bật LED

4. STM32 gửi ACK: AA 01 FE 00 A9
```

## 10.2 Nhận lệnh Servo 1500

```
Timeline:
─────────────────────────────────────────────────────────

1. PC gửi: AA 01 11 02 05 DC XX
              │  │  │  │  │  │  └── CRC
              │  │  │  │  │  └───── DATA[1]=0xDC
              │  │  │  │  └──────── DATA[0]=0x05
              │  │  │  └─────────── LEN=2
              │  │  └────────────── CMD=SERVO_PULSE
              │  └───────────────── ID=1
              └──────────────────── SOF

   Pulse = 0x05DC = 1500

2. UART Interrupt:
   → 0xAA: State = GOT_SOF
   → 0x01: frame.id = 1
   → 0x11: frame.cmd = 0x11
   → 0x02: frame.len = 2
   → 0x05: frame.data[0] = 0x05
   → 0xDC: frame.data[1] = 0xDC
   → 0xXX: CRC check → COMPLETE!

3. ProcessCommand:
   → case CMD_SERVO_PULSE:
   → pulse = (0x05 << 8) | 0xDC = 1500
   → 500 <= 1500 <= 2500 → OK!
   → __HAL_TIM_SET_COMPARE(&htim2, CH1, 1500)
   → Servo quay đến vị trí giữa!

4. STM32 gửi ACK
```

---

# 📌 TÓM TẮT

| Thành phần | Tác dụng |
|------------|----------|
| `rs485_protocol.h` | Định nghĩa commands, structs |
| `rs485_protocol.c` | Parser state machine |
| `rs485_uart.c` | UART wrapper, send/receive |
| `main.c` | Command handler |

**Luồng chính:**
```
UART RX Interrupt 
    → ParserFeed() 
    → HasNewFrame() 
    → ProcessCommand() 
    → SendAck/Nack()
```

---

*Tài liệu này là một phần của dự án SuperApp RS485*
