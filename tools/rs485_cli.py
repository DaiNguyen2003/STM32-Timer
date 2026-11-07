"""Send one command to the STM32-Timer demo through a USB-RS485 adapter."""
import argparse
import time

SOF = 0xAA
ACK = 0xFE
NACK = 0xFF
COMMANDS = {"on": 0x01, "off": 0x02, "toggle": 0x03,
            "pulse": 0x11, "status": 0x20, "ping": 0x30}


def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = ((crc << 1) ^ (0x07 if crc & 0x80 else 0)) & 0xFF
    return crc


def build_frame(node, command, payload=b""):
    body = bytes((node, command, len(payload))) + payload
    return bytes((SOF,)) + body + bytes((crc8(body),))


def decode_frame(frame):
    if len(frame) < 5 or frame[0] != SOF or len(frame) != frame[3] + 5:
        raise ValueError("invalid frame layout")
    if crc8(frame[1:-1]) != frame[-1]:
        raise ValueError("invalid CRC")
    return frame[1], frame[2], frame[4:-1]


def read_response(serial_port, node):
    deadline = time.monotonic() + 1.0
    pending = bytearray()
    while time.monotonic() < deadline:
        pending.extend(serial_port.read(1))
        if pending and pending[0] != SOF:
            pending.clear()
        if len(pending) >= 4 and len(pending) == pending[3] + 5:
            try:
                address, command, payload = decode_frame(pending)
                if address == node and command in (ACK, NACK):
                    return command, payload
            except ValueError:
                pass
            pending.clear()
    raise TimeoutError("no valid ACK or NACK from the selected node")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=COMMANDS)
    parser.add_argument("--port", help="serial port, for example COM5 or /dev/ttyUSB0")
    parser.add_argument("--node", type=lambda text: int(text, 0), default=1)
    parser.add_argument("--pulse", type=int, help="pulse width in microseconds")
    parser.add_argument("--dry-run", action="store_true", help="print the frame without opening a port")
    args = parser.parse_args()
    if not 1 <= args.node <= 254:
        parser.error("--node must be between 1 and 254")
    if args.command == "pulse":
        if args.pulse is None or not 500 <= args.pulse <= 2500:
            parser.error("pulse requires --pulse between 500 and 2500")
        payload = args.pulse.to_bytes(2, "big")
    elif args.pulse is not None:
        parser.error("--pulse is only valid with the pulse command")
    else:
        payload = b""

    frame = build_frame(args.node, COMMANDS[args.command], payload)
    print("TX", frame.hex(" ").upper())
    if args.dry_run:
        return
    if not args.port:
        parser.error("--port is required unless --dry-run is used")

    try:
        import serial
    except ImportError:
        parser.error("Install pyserial with python -m pip install pyserial")

    try:
        with serial.Serial(args.port, 115200, timeout=0.05, write_timeout=1) as port:
            port.reset_input_buffer()
            port.write(frame)
            command, data = read_response(port, args.node)
    except (TimeoutError, serial.SerialException) as error:
        parser.exit(1, f"{error}\n")
    if command == NACK:
        parser.exit(1, "RX NACK\n")
    if args.command == "status" and len(data) == 2:
        angle = int.from_bytes(data[1:], "big", signed=True)
        print(f"RX ACK · LED {data[0]} · góc quy đổi {angle}°")
    else:
        print("RX ACK", data.hex(" ").upper())


if __name__ == "__main__":
    main()
