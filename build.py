"""Build the Blue Pill demo with GNU Arm Embedded and STM32CubeF1."""
import argparse
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parent
SDK_DEFAULT = Path.home() / "STM32Cube/Repository/STM32Cube_FW_F1_V1.8.7"
MODULES = ('cortex', 'gpio', 'rcc', 'rcc_ex', 'flash', 'flash_ex', 'uart', 'dma', 'tim', 'tim_ex')
APPLICATION = ('Core/Src/main.c', 'Core/Src/rs485_uart.c', 'Core/Src/rs485_protocol.c')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, default=SDK_DEFAULT, help="STM32CubeF1 v1.8.7 root")
    parser.add_argument("--toolchain", type=Path, help="directory containing arm-none-eabi-gcc")
    args = parser.parse_args()

    drivers = args.sdk.resolve() / "Drivers"
    hal = drivers / "STM32F1xx_HAL_Driver"
    cmsis = drivers / "CMSIS"
    device = cmsis / "Device/ST/STM32F1xx"
    templates = device / "Source/Templates"
    hal_config = hal / "Inc/stm32f1xx_hal_conf_template.h"
    if not hal_config.is_file():
        parser.error(f"STM32CubeF1 package missing at {args.sdk}. Pass --sdk with the package root.")

    build = ROOT / "build"
    build.mkdir(exist_ok=True)
    config = hal_config.read_text(encoding="utf-8")
    config = re.sub(r"(?m)^#define HAL_(\w+)_MODULE_ENABLED[^\n]*",
                    lambda match: match[0] if match[1].lower() in MODULES else "",
                    config)
    (build / "stm32f1xx_hal_conf.h").write_text(config, encoding="utf-8")

    def run(tool, *options):
        executable = f"arm-none-eabi-{tool}"
        if args.toolchain:
            executable = str(args.toolchain.resolve() / executable)
        subprocess.run([executable, *map(str, options)], check=True, cwd=ROOT)

    includes = [build, ROOT / "firmware", ROOT, ROOT / "Core/Inc",
                hal / "Inc", cmsis / "Include", device / "Include"]
    hal_sources = ["stm32f1xx_hal", *(f"stm32f1xx_hal_{module}" for module in MODULES)]
    elf = build / "firmware.elf"
    try:
        run("gcc", "-mcpu=cortex-m3", "-mthumb", "-DSTM32F103xB", "-DUSE_HAL_DRIVER",
            "-std=gnu11", "-Os", "-g", "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra",
            *(f"-I{path}" for path in includes), *APPLICATION, "firmware/board.c",
            templates / "system_stm32f1xx.c", templates / "gcc/startup_stm32f103xb.s",
            *(hal / "Src" / f"{source}.c" for source in hal_sources),
            "-nostartfiles", "--specs=nano.specs", "--specs=nosys.specs", "-Tfirmware/stm32f103c8.ld",
            "-Wl,--gc-sections", f"-Wl,-Map={build / 'firmware.map'}", "-o", elf)
        run("objcopy", "-O", "ihex", elf, build / "firmware.hex")
        run("objcopy", "-O", "binary", elf, build / "firmware.bin")
        run("size", elf)
    except FileNotFoundError as error:
        parser.error(f"{error}. Add GNU Arm Embedded to PATH or pass --toolchain.")


if __name__ == "__main__":
    main()
