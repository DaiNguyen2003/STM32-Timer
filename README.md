# Điều Khiển Servo MG995 với STM32

Hướng dẫn chi tiết cách thiết lập phần cứng và cấu hình STM32CubeMX để điều khiển động cơ Servo MG995.

## 📋 Thông số kỹ thuật MG995

| Thông số | Giá trị |
|----------|---------|
| Trọng lượng | 55g |
| Kích thước | 40.7 x 19.7 x 42.9 mm |
| Mô-men xoắn | 8.5 kgf·cm (4.8V), 10 kgf·cm (6V) |
| Tốc độ hoạt động | 0.2s/60° (4.8V), 0.16s/60° (6V) |
| Điện áp hoạt động | 4.8V - 7.2V |
| Dead band width | 5 µs |
| Nhiệt độ hoạt động | 0°C - 55°C |
| Góc quay | ±60° (tổng 120°) |
| Bánh răng | Kim loại |

---

## 🔌 Thiết Lập Phần Cứng (Hardware Setup)

### Sơ đồ kết nối

```
STM32F103C8                    MG995 Servo
┌──────────────┐              ┌──────────────┐
│              │              │              │
│   PA0/TIM2   ├──────────────┤ PWM (Vàng)   │
│   CH1        │   Tín hiệu   │              │
│              │              │              │
│   GND        ├──────────────┤ GND (Nâu)    │
│              │              │              │
└──────────────┘              │ VCC (Đỏ)     │
                              │    ↓         │
Nguồn ngoài 5-6V ─────────────┤ VCC          │
(2A trở lên)                  └──────────────┘
```

### Bảng kết nối chi tiết

| Dây Servo MG995 | Màu sắc  | Kết nối với |
|-----------------|--------- |-------------|
| PWM (Tín hiệu)  | Vàng/Cam | PA0 (TIM2_CH1) |
| VCC (Nguồn)     | Đỏ       | Nguồn ngoài 5-6V |
| GND             | Nâu/Đen | GND chung (STM32 + Nguồn ngoài) |

> ⚠️ **Lưu ý quan trọng:**
>
> - **KHÔNG** cấp nguồn servo từ chân 3.3V/5V của STM32 (dòng không đủ)
> - Sử dụng nguồn ngoài riêng cho servo (5-6V, ít nhất 2A)
> - **BẮT BUỘC** nối GND chung giữa STM32 và nguồn servo

> 🚨 **LỖI PHỔ BIẾN NHẤT: QUÊN NỐI GND CHUNG!**
>
> Nếu servo không quay dù code chạy đúng (LED nhấp nháy), hãy kiểm tra:
>
> ```
> STM32 GND ────┬──── Nguồn 6V (cực âm -)
>               │
>               └──── Servo dây NÂU/ĐEN
> ```
>
> **Tất cả 3 điểm GND phải được nối chung với nhau!**

---

## ⚙️ Cấu Hình STM32CubeMX (Pinout & Configuration)

### Bước 1: Cấu hình System Clock

1. Mở file `.ioc` trong STM32CubeIDE
2. Vào **Clock Configuration**
3. Thiết lập:
   - HSE hoặc HSI làm nguồn clock
   - System Clock: **72 MHz** (khuyên dùng)
   - APB1 Timer clocks: **72 MHz**

### Bước 2: Cấu hình Timer cho PWM (Chi Tiết)

Timer là trung tâm của việc tạo xung PWM để điều khiển góc quay của servo. Servo hoạt động với xung có chu kỳ là **20ms (50Hz)**. Độ rộng xung PWM sẽ quyết định góc quay. Vì vậy chúng ta sẽ cấu hình bộ Timer của STM32 để xuất ra PWM 50Hz, và có độ phân giải **1µs** (Microsecond) để dễ dàng tinh chỉnh góc quay.

1. Vào **Pinout & Configuration** → **Timers** → Chọn **TIM2**
2. Ở mục **Mode**:
   - Clock Source: Chọn `Internal Clock`
   - Channel 1: Chọn `PWM Generation CH1`
3. Ở mục **Configuration** bên dưới, tab **Parameter Settings**, thiết lập các thông số sau:

| Thông số | Giá trị | Giải thích |
|----------|---------|------------|
| Prescaler (PSC) | **71** | Bộ chia tần số. Do System Clock là 72MHz, để Timer đếm 1MHz (tức là 1µs mỗi tick), ta cấu hình `71` (vị 72MHz / (71 + 1) = 1MHz). |
| Counter Mode | **Up** | Đếm lên. |
| Counter Period (ARR) | **19999** | Chu kỳ của xung (Auto-Reload Register). Servo cần chu kỳ **20ms**, với độ phân giải 1µs (1MHz), cần 20,000 xung đếm. Ghi `19999` (vì 19999 + 1 = 20,000 µs = 20ms). |
| Internal Clock Division | **No Division** | Không chia nhỏ thêm. |
| auto-reload preload | **Enable** | Kích hoạt nạp lại tự động. |

1. Tiếp tục cuộn xuống phần **PWM Generation Channel 1**:

| Thông số | Giá trị | Giải thích |
|----------|---------|------------|
| Mode | **PWM mode 1** | Chế độ PWM tiêu chuẩn. |
| Pulse (CCR1) | **1500** | Độ rộng xung ban đầu (Compare value). Số 1500 tương ứng 1500µs (1.5ms), là góc 0 độ của Servo. |
| CH Polarity | **High** | Xung ở mức logic 1 tương ứng với độ rộng xung (Active High). |

> 💡 **Tính toán toán học nhanh:**
>
> - Tần số Timer: `72,000,000 Hz` / `(71 + 1)` = `1,000,000 Hz` (1MHz = 1µs đếm 1 nhịp)
> - Tần số PWM đầu ra: `1,000,000 Hz` / `(19999 + 1)` = `50 Hz` (Tương đương 20ms chu kỳ cho servo)
> - Điểm giữa của Servo (1500µs) tức là thanh ghi nhịp số `1500`. Do vậy `__HAL_TIM_SET_COMPARE` truyền thẳng giá trị thời gian µs rất tiện lợi.

### Bước 3: Cấu hình GPIO (tự động)

Sau khi cấu hình TIM2_CH1, chân **PA0** sẽ tự động được cấu hình:

- Mode: `Alternate Function Push Pull`
- Speed: `High`

### Bước 4: Generate Code

1. **Project Manager** → Đặt tên project
2. **Code Generator** → Tick `Generate peripheral initialization...`
3. Nhấn **Generate Code**

---

## 💻 Code Demo

### Thêm vào `main.c`

```c
/* USER CODE BEGIN Includes */
// Không cần thêm include
/* USER CODE END Includes */

/* USER CODE BEGIN PD */
// Định nghĩa giá trị pulse cho các góc quay
#define SERVO_MIN_PULSE   500   // -60 độ (500µs)
#define SERVO_MID_PULSE   1500  // 0 độ (1500µs)  
#define SERVO_MAX_PULSE   2500  // +60 độ (2500µs)
/* USER CODE END PD */

/* USER CODE BEGIN 0 */
/**
 * @brief  Đặt góc cho servo MG995
 * @param  angle: Góc từ -60 đến +60 độ
 * @retval None
 */
void Servo_SetAngle(int8_t angle)
{
    // Giới hạn góc trong phạm vi cho phép
    if (angle < -60) angle = -60;
    if (angle > 60) angle = 60;
    
    // Chuyển đổi góc sang giá trị pulse
    // -60° → 500µs, 0° → 1500µs, +60° → 2500µs
    uint16_t pulse = (uint16_t)(SERVO_MID_PULSE + (angle * 1000 / 60));
    
    // Cập nhật giá trị Compare cho TIM2 Channel 1
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
}

/**
 * @brief  Di chuyển servo mượt từ góc hiện tại đến góc đích
 * @param  targetAngle: Góc đích (-60 đến +60)
 * @param  delayMs: Thời gian delay giữa mỗi bước (ms)
 * @retval None
 */
void Servo_SweepTo(int8_t targetAngle, uint16_t delayMs)
{
    static int8_t currentAngle = 0;
    
    while (currentAngle != targetAngle)
    {
        if (currentAngle < targetAngle)
            currentAngle++;
        else
            currentAngle--;
        
        Servo_SetAngle(currentAngle);
        HAL_Delay(delayMs);
    }
}
/* USER CODE END 0 */
```

### Trong hàm `main()`: Chạy Demo

```c
int main(void)
{
    /* USER CODE BEGIN 1 */
    /* USER CODE END 1 */

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_TIM2_Init();

    /* Khởi động PWM trên TIM2 Channel 1 */
    /* USER CODE BEGIN 2 */
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    
    // Đặt servo về vị trí giữa (0 độ)
    Servo_SetAngle(0);
    HAL_Delay(1000);
    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        /* USER CODE BEGIN WHILE */
        // ===== VÍ DỤ 1: QUÉT QUAY QUA LẠI (SWEEP) =====
        // Quay sang phải (+60°)
        Servo_SweepTo(60, 20);
        HAL_Delay(500);
        
        // Quay về giữa (0°)
        Servo_SweepTo(0, 20);
        HAL_Delay(500);
        
        // Quay sang trái (-60°)
        Servo_SweepTo(-60, 20);
        HAL_Delay(500);
        
        // Quay về giữa (0°)
        Servo_SweepTo(0, 20);
        HAL_Delay(500);
        /* USER CODE END WHILE */
    }
}
```

### Ví Dụ Mở Rộng: Điều Khiển Servo qua lệnh UART (Hoặc RS485)

Khung truyền nhận UART từ máy tính (ví dụ gửi chuỗi hex qua thư viện, hoặc custom command):

```c
/* Ví dụ đoạn code đặt trong vòng lặp while(1) sau khi nhận UART hoàn chỉnh */
// CMD_SERVO_PULSE: lệnh đổi xung PWM truyền sang
uint8_t cmd = rx_frame[0];

if (cmd == CMD_SERVO_PULSE) {
    // Trích xuất 16-bit payload Data (byte 1 và byte 2)
    uint16_t pulse = (rx_frame[1] << 8) | rx_frame[2];
    
    if (pulse >= 500 && pulse <= 2500) {
        // Áp dụng độ rộng xung trực tiếp
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
        printf("ACK: Servo adjusted.\n");
    } else {
        printf("NACK: Pulse out of bound!\n");
    }
}
```

---

## 📊 Bảng tra cứu góc - Pulse

| Góc (độ) | Pulse (µs) | Giá trị CCR |
|----------|------------|-------------|
| -60°     | 500        | 500         |
| -45°     | 750        | 750         |
| -30°     | 1000       | 1000        |
| -15°     | 1250       | 1250        |
| 0°       | 1500       | 1500        |
| +15°     | 1750       | 1750        |
| +30°     | 2000       | 2000        |
| +45°     | 2250       | 2250        |
| +60°     | 2500       | 2500        |

---

## 🔧 Khắc phục sự cố

| Vấn đề | Nguyên nhân | Giải pháp |
|--------|-------------|-----------|
| Servo không hoạt động | Nguồn không đủ | Dùng nguồn ngoài 5-6V, 2A |
| Servo rung, giật | GND không chung | Nối chung GND STM32 và nguồn servo |
| Servo không quay đủ góc | Pulse sai | Điều chỉnh `SERVO_MIN/MAX_PULSE` |
| Servo phát ra tiếng kêu | Tải quá nặng | Giảm tải hoặc dùng servo mạnh hơn |

---

## 📁 Cấu trúc Project

```
Digital_Servo/
├── Core/
│   ├── Inc/
│   │   └── main.h
│   └── Src/
│       └── main.c          ← Thêm code demo vào đây
├── Drivers/
├── Digital_Servo.ioc       ← File cấu hình CubeMX
└── README.md               ← File này
```

---

## 📚 Tài liệu tham khảo

- [STM32F103 Reference Manual](https://www.st.com/resource/en/reference_manual/rm0008-stm32f101xx-stm32f102xx-stm32f103xx-stm32f105xx-and-stm32f107xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)
- [HAL Driver Documentation](https://www.st.com/resource/en/user_manual/um1850-description-of-stm32f1-hal-and-lowlayer-drivers-stmicroelectronics.pdf)
