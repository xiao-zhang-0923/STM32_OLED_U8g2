# -*- coding: utf-8 -*-
"""Create the Keil MDK project file for OLED_U28G by adapting the CubeMX template."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEMPLATE = r"D:\Keil5\STM32projecthal\OLED_hanzidianzhen\MDK-ARM\OLED_hanzidianzhen.uvprojx"
OUT = os.path.join(ROOT, "MDK-ARM", "OLED_U28G.uvprojx")


def f(name, path, ftype=1):
    return ("            <File>\n"
            "              <FileName>%s</FileName>\n"
            "              <FileType>%d</FileType>\n"
            "              <FilePath>%s</FilePath>\n"
            "            </File>\n") % (name, ftype, path)


def group(name, files):
    return ("        <Group>\n"
            "          <GroupName>%s</GroupName>\n"
            "          <Files>\n%s          </Files>\n"
            "        </Group>\n") % (name, "".join(files))


def main():
    text = open(TEMPLATE, encoding="utf-8").read()
    text = text.replace("OLED_hanzidianzhen", "OLED_U28G")

    groups = []
    groups.append(group("Application/MDK-ARM", [
        f("startup_stm32f103xb.s", "startup_stm32f103xb.s", 2)]))

    groups.append(group("Application/User/Core", [
        f("main.c", "../Core/Src/main.c"),
        f("gpio.c", "../Core/Src/gpio.c"),
        f("i2c.c", "../Core/Src/i2c.c"),
        f("stm32f1xx_it.c", "../Core/Src/stm32f1xx_it.c"),
        f("stm32f1xx_hal_msp.c", "../Core/Src/stm32f1xx_hal_msp.c")]))

    hal = ["stm32f1xx_hal_gpio_ex.c", "stm32f1xx_hal_i2c.c", "stm32f1xx_hal.c",
           "stm32f1xx_hal_rcc.c", "stm32f1xx_hal_rcc_ex.c", "stm32f1xx_hal_gpio.c",
           "stm32f1xx_hal_dma.c", "stm32f1xx_hal_cortex.c", "stm32f1xx_hal_pwr.c",
           "stm32f1xx_hal_flash.c", "stm32f1xx_hal_flash_ex.c", "stm32f1xx_hal_exti.c"]
    groups.append(group("Drivers/STM32F1xx_HAL_Driver", [
        f(name, "../Drivers/STM32F1xx_HAL_Driver/Src/" + name) for name in hal]))

    groups.append(group("Drivers/CMSIS", [
        f("system_stm32f1xx.c", "../Core/Src/system_stm32f1xx.c")]))

    groups.append(group("App", [
        f("app_ui.c", "../App/Src/app_ui.c"),
        f("u8g2_port.c", "../App/Src/u8g2_port.c"),
        f("cn_font.c", "../App/Src/cn_font.c"),
        f("duck_anim.c", "../App/Src/duck_anim.c"),
        f("app_ui.h", "../App/Inc/app_ui.h", 5),
        f("app_config.h", "../App/Inc/app_config.h", 5),
        f("u8g2_port.h", "../App/Inc/u8g2_port.h", 5),
        f("cn_font.h", "../App/Inc/cn_font.h", 5),
        f("duck_anim.h", "../App/Inc/duck_anim.h", 5)]))

    u8g2 = ["u8g2_setup.c", "u8g2_buffer.c", "u8g2_hvline.c", "u8g2_ll_hvline.c",
            "u8g2_intersection.c", "u8g2_bitmap.c", "u8g2_box.c", "u8g2_line.c",
            "u8g2_circle.c", "u8g2_polygon.c", "u8g2_font.c", "u8g2_kerning.c",
            "u8g2_cleardisplay.c", "u8g2_d_setup.c", "u8g2_d_memory.c",
            "u8g2_fonts.c",
            "u8x8_setup.c", "u8x8_display.c", "u8x8_byte.c", "u8x8_cad.c",
            "u8x8_gpio.c", "u8x8_d_ssd1306_128x64_noname.c",
            "u8x8_8x8.c", "u8x8_capture.c", "u8x8_u16toa.c"]
    files = [f(name, "../Middlewares/Third_Party/u8g2/csrc/" + name) for name in u8g2]
    files += [f("u8g2.h", "../Middlewares/Third_Party/u8g2/csrc/u8g2.h", 5),
              f("u8x8.h", "../Middlewares/Third_Party/u8g2/csrc/u8x8.h", 5)]
    groups.append(group("Middlewares/u8g2", files))
    groups.append(group("::CMSIS", []))

    start = text.index("      <Groups>")
    end = text.index("      </Groups>", start) + len("      </Groups>\n")
    text = text[:start] + "      <Groups>\n" + "".join(groups) + "      </Groups>\n" + text[end:]

    old_inc = ("../Core/Inc;../Drivers/STM32F1xx_HAL_Driver/Inc;"
               "../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy;"
               "../Drivers/CMSIS/Device/ST/STM32F1xx/Include;../Drivers/CMSIS/Include")
    new_inc = old_inc + ";../App/Inc;../Middlewares/Third_Party/u8g2/csrc"
    assert old_inc in text
    text = text.replace(old_inc, new_inc)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print("written", OUT, len(text), "bytes")


if __name__ == "__main__":
    main()

