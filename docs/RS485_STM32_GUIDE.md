# Tích hợp RS485 trên STM32F1

Build demo theo [README](../README.md). Nếu tạo project CubeMX mới, bật TIM2_CH1 trên PA0 và USART1 trên PA9/PA10; đặt TIM2 PSC 71, ARR 19999, Pulse 1500 với xung timer 72 MHz. Bật USART1 global IRQ, 115200 8N1 và để `USART1_IRQHandler()` gọi `HAL_UART_IRQHandler()`. Driver tự cấu hình PA8 khi gọi `RS485_UART_Init()`.

Thêm [header và parser](../Core/Inc/rs485_protocol.h), [API UART](../Core/Inc/rs485_uart.h) cùng hai tệp nguồn tương ứng trong `Core/Src`. Trong callback RX, truyền byte đã nhận vào `RS485_UART_RxCallback()`, rồi đăng ký nhận lại một byte. Vòng lặp chính kiểm tra `RS485_UART_HasNewFrame()`, đọc khung và gọi `RS485_UART_ClearFrameFlag()` sau khi xử lý.

Đổi `RS485_NODE_ID` cho từng board khi thử nhiều node. Giữ một lệnh unicast đang chờ phản hồi tại mỗi thời điểm, vì driver giữ một khung chờ xử lý.

| Điểm debug | Dữ liệu |
| --- | --- |
| `debug_rx_count` | Số byte UART đã nhận. |
| `debug_last_cmd` | Mã lệnh hợp lệ gần nhất được xử lý. |
| `debug_pulse` | Độ rộng xung PWM gần nhất, µs. |
| `debug_angle` | Góc quy đổi từ xung, không phải góc đo. |

[Sơ đồ đấu nối](RS485_HARDWARE_SETUP.md) · [Bảng lệnh](RS485_PROTOCOL_UART_DETAIL.md)
