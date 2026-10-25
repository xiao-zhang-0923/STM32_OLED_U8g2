# OLED_U28G — STM32F103C8T6 + HAL + u8g2 综合例程

> 课程任务 3：用 CubeMX 建立 HAL 工程框架，裁剪并移植 u8g2 到 0.96 寸 I2C OLED，
> 完成 u8g2 演示、学号姓名显示、上下左右滑动显示和带动态效果的图案显示。  
> **姓名：张雅琳　学号：632404310105**

![界面预览](Docs/UI预览.png)

---

## 1. 功能与任务对应关系

| 任务要求 | 实现位置 | 说明 |
|---|---|---|
| ① u8g2 的 demo 例程 | 第 1、2 页 `page_demo()` / `page_font()` | 图形原语（线、框、圆、实心圆）、正弦波动画、5 种 ASCII 字体、中文点阵 |
| ② 显示学号和姓名 | 第 3 页 `page_id()` | 16×16 汉字“张雅琳 / 学号 / 姓名” + 12 位学号 `632404310105` |
| ③ 上下、左右滑动 | 第 4、5 页 + 全屏翻页 | 左右跑马灯、逐像素上下滚动（到顶/到底自动反向）、四方向滑动翻页 |
| ④ 带动画效果的图案 | 第 6 页 `page_duck()` | 把 `图例.jpg` 人像生成 3 帧 64×64 单色图，呼吸缩放 + 左右摇摆 + 星星闪烁 |
| 附加：运行状态诊断 | 第 7 页 `page_info()` | I2C 地址、成功/失败次数、FPS、页码，便于排查“显示 API 运行失败” |

- 上电先显示 1.5 s 启动画面，然后每 10 s 自动换页；按 **PA0（WK_UP）** 立即换页。
- 每次换页按 **左 → 上 → 右 → 下** 的顺序轮换滑动方向。
- 板载 LED（PC13）500 ms 闪烁，表示程序在运行。

---

## 2. 硬件连接

| STM32F103C8T6 | OLED / 外设 | 说明 |
|---|---|---|
| PB6 | OLED SCL | I2C1，复用开漏，400 kHz |
| PB7 | OLED SDA | I2C1，复用开漏，400 kHz |
| 3.3V | OLED VCC | 多数 0.96" 模块为 3.3V |
| GND | OLED GND | 共地 |
| PA0 | WK_UP 按键 | 高电平有效，内部下拉 |
| PC13 | 板载 LED | 低电平点亮 |

OLED 模块一般自带 4.7 kΩ 上拉电阻；若模块没有上拉，必须在 SCL/SDA 上各加一个 4.7 kΩ 到 3.3V。

---

## 3. 快速开始

1. 用 **Keil MDK 5** 打开 `MDK-ARM/OLED_U28G.uvprojx`。
2. 工程使用 **ARMCC V5.06 (AC5)**，与 CubeMX 生成的 STM32F1 工程一致；
   若本机只有 AC6，可在 `Options for Target → Target → ARM Compiler` 里切换（代码兼容 C99）。
3. `Rebuild`，应得到 `0 Error(s)`；输出文件为 `MDK-ARM/OLED_U28G/OLED_U28G.hex`。
4. 下载：`Options for Target → Debug` 选择 **ST-Link Debugger**，
   `Utilities → Settings` 勾选 **Reset and Run**，然后 `Download`。
5. 若屏幕不亮，先看第 7 页诊断信息或本文第 8 节。

> 时钟：HSE 8 MHz × PLL9 = **72 MHz**，APB1 = 36 MHz，I2C1 = 400 kHz。
> 若使用 STM32F103C8T6 最小系统板，请确认 BOOT0 = 0，且已焊接 8 MHz 晶振。

---

## 4. 目录结构

```text
OLED_U28G/
├─ OLED_U28G.ioc                  CubeMX 工程文件（I2C1、GPIO、时钟）
├─ .mxproject                     CubeMX 生成记录
├─ Core/                          CubeMX 生成的 HAL 框架
│  ├─ Inc/  main.h gpio.h i2c.h stm32f1xx_hal_conf.h ...
│  └─ Src/  main.c gpio.c i2c.c stm32f1xx_it.c stm32f1xx_hal_msp.c system_stm32f1xx.c
├─ Drivers/                       STM32F1 HAL + CMSIS（只保留用到的部分）
├─ Middlewares/Third_Party/u8g2/
│  ├─ csrc/                       裁剪后的 u8g2 源码（24 个 .c + 2 个 .h）
│  └─ LICENSE                     u8g2 的 BSD 2-Clause 许可证
├─ App/
│  ├─ Inc/  app_ui.h app_config.h u8g2_port.h cn_font.h duck_anim.h
│  └─ Src/  app_ui.c u8g2_port.c cn_font.c duck_anim.c
├─ MDK-ARM/                       Keil 工程、启动文件、RTE
├─ Docs/                          任务报告、UI 预览图
└─ Tools/                         u8g2 裁剪脚本、图案取模脚本、PC 端渲染验证
```

---

## 5. CubeMX 框架说明

| 项目 | 配置 |
|---|---|
| MCU | STM32F103C8Tx，LQFP48，64 KB Flash / 20 KB RAM |
| RCC | HSE 8 MHz 晶振，PLL ×9，SYSCLK 72 MHz，APB1 /2，APB2 /1 |
| SYS | Serial Wire（PA13/PA14），SysTick 1 ms |
| I2C1 | 硬件 I2C：400 kHz，7 位地址，DutyCycle 2；PB6 = SCL，PB7 = SDA |
| 软件 I2C | 默认开启（`OLED_USE_SW_I2C=1`），PB6/PB7 开漏位翻转，便于 Keil 逻辑分析仪抓波形；改 0 即用硬件 I2C |
| GPIO | PC13 推挽输出（LED），PA0 输入下拉（按键） |
| 工程 | MDK-ARM V5，`USE_HAL_DRIVER, STM32F103xB`，堆 0x200 / 栈 0x400 |

`main.c` 严格保留 CubeMX 的 `USER CODE BEGIN/END` 区，重新生成代码不会覆盖：

```c
/* USER CODE BEGIN Includes */
#include "app_ui.h"
/* USER CODE END Includes */
...
  MX_GPIO_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  App_Init();
  /* USER CODE END 2 */
  while (1)
  {
    App_Task();
  }
```

> 注意：如果重新用 CubeMX 生成，`i2c.c` 的 400 kHz 参数可能被恢复为默认 100 kHz，
> 生成后请检查 `hi2c1.Init.ClockSpeed`。

---

## 6. u8g2 裁剪与移植

### 6.1 阅读原仓库得到的结论

- `.gitignore`、`.gitmodules`、`.gitpod.*`、`CMakeLists.txt`、`component.mk`、`SConscript`
  都是构建/开发环境文件，和 STM32 无关；`sys/` 下只有 Arduino、Linux、AVR、SDL 等移植示例，
  **没有 STM32 HAL 的现成例子**，需要自己写 `u8x8` 字节回调。
- `csrc/` 一共有 120 个文件，`u8g2_fonts.c` 单个文件 31 MB、`u8g2_d_setup.c` 372 KB，
  直接加入 Keil 工程会远超 64 KB Flash，必须裁剪。
- `LICENSE` 是 BSD 2-Clause，可以自由裁剪、商用，但需保留版权声明（已放在
  `Middlewares/Third_Party/u8g2/LICENSE`）。
- `ChangeLog` 显示源码版本约为 **v2.33.x**，API 与常见教程一致。

### 6.2 裁剪结果

| 类别 | 保留内容 |
|---|---|
| 核心 | `u8g2_setup.c`、`u8g2_buffer.c`、`u8g2_hvline.c`、`u8g2_ll_hvline.c`、`u8g2_intersection.c`、`u8g2_font.c`、`u8g2_kerning.c`、`u8g2_cleardisplay.c` |
| 绘图层 | `u8g2_bitmap.c`、`u8g2_box.c`、`u8g2_line.c`、`u8g2_circle.c`、`u8g2_polygon.c` |
| u8x8 底层 | `u8x8_setup.c`、`u8x8_display.c`、`u8x8_byte.c`、`u8x8_cad.c`、`u8x8_gpio.c`、`u8x8_8x8.c`、`u8x8_capture.c`、`u8x8_u16toa.c` |
| 显示屏 | 只保留 `u8x8_d_ssd1306_128x64_noname.c` |
| 初始化 | `u8g2_d_setup.c` 只保留 `..._noname_1 / _2 / _f` 三个函数 |
| 显存 | `u8g2_d_memory.c` 只保留 `u8g2_m_16_8_1 / _2 / _f`（128/256/1024 字节） |
| 字体 | `4x6_tf`、`5x7_tf`、`6x10_tf`、`7x13B_tf`、`10x20_tf`，共 5 个 |
| 删除 | 其余 90 多个驱动、所有大字体、Arduino/SPI/串口示例、C++ 封装 |

裁剪脚本：`Tools/trim_u8g2.py`，可重复执行。

### 6.3 三层调用关系

```text
App (page_xxx)  →  u8g2_DrawStr / DrawLine / DrawXBMP / SendBuffer
                     ↓
                  u8g2 核心（1024 字节全屏缓存）
                     ↓
                  u8x8_cad_ssd13xx_fast_i2c（自动加 0x00/0x40 控制字节）
                     ↓
                  u8x8_byte_stm32_hw_i2c（本项目写的 HAL 回调）
                     ↓
                  HAL_I2C_Master_Transmit(&hi2c1, 0x78, buf, len, 20)
```

关键回调（`App/Src/u8g2_port.c`）：

```c
case U8X8_MSG_BYTE_END_TRANSFER:
    if (HAL_I2C_Master_Transmit(&hi2c1, g_oled_addr,
                                i2c_tx_buf, i2c_tx_len, 20) != HAL_OK)
    {
        g_i2c_err_count++;
        return 0;
    }
    g_i2c_ok_count++;
    return 1;
```

- `u8g2` 的 CAD 层已经把命令/数据控制字节（0x00 / 0x40）放进缓冲区，
  所以一次 `HAL_I2C_Master_Transmit` 就对应总线上的一个完整 I2C 写事务。
- 初始化时自动扫描 **0x78 / 0x7A** 两个地址，兼容 0x3C/0x3D 两种模块。
- 若总线被从机拉死，`U8G2_Port_BusRecover()` 会发 9 个 SCL 时钟 + 手动 STOP，
  再重新初始化 I2C1。

### 6.4 资源占用（Keil AC5，-O3）

```text
Code=15430  RO-data=13266  RW-data=116  ZI-data=3348
Flash ≈ 28.8 KB / 64 KB      RAM ≈ 3.4 KB / 20 KB
```

---

## 7. 显示功能与滑动实现

### 7.1 学号姓名（第 3 页）

u8g2 自带字体不含汉字，本工程用 SimHei 生成 16×16 点阵，作为 `u8g2_DrawXBMP`
的位图字库（`App/Src/cn_font.c`，只保留 7 个字：张、雅、琳、学、号、姓、名，
共 7×32 = 224 字节），再按 Unicode 码点查表绘制。这样比引入完整 GB2312 字库
（几十 KB）省得多。

```c
CnFont_DrawStr(&u8g2, 6, 14, xuehao);       // “学号”
u8g2_DrawStr(&u8g2, 50, 26, "632404310105");
CnFont_DrawStr(&u8g2, 6, 36, xingming);     // “姓名”
CnFont_DrawStr(&u8g2, 50, 36, name);        // “张雅琳”
```

### 7.2 左右滑动（第 4 页）

```c
x = 128 - pos;                    // 文本左移 pos 像素
if (x >= 0)  DrawStr(x, text);
else {                            // 已经移出左边界：从下一个字符边界开始画剩余部分
    skip = (pos + 5) / 6;
    DrawStr(skip*6 - pos, text + skip);
}
```

- 第一行 6×10 字体、2 px/帧；第二行 4×6 字体、1 px/帧，形成层次。
- u8g2 的坐标是 `u8g2_uint_t`（默认 8 位），**负数会被当成 255 而画不出来**，
  所以不能直接 `x = -5`，必须像上面这样按字符边界跳转。

### 7.3 上下滑动（第 5 页）

u8g2 全屏缓存是 8 页 × 128 字节，每字节代表纵向 8 个像素。垂直滚动直接对缓存
做移位，再补画新进入的行，速度快且真正逐像素：

```c
static void lcd_shift_up(uint8_t n)   // 上移 n 像素
{
    uint8_t *buf = u8g2_GetBufferPtr(&u8g2);
    for (page = 0; page < 8; page++)
        for (x = 0; x < 128; x++) {
            lo = buf[page*128 + x] >> n;
            hi = (page < 7) ? buf[(page+1)*128 + x] << (8-n) : 0;
            buf[page*128 + x] = lo | hi;
        }
}
```

- 向上滚到内容末尾自动反向，变成向下滚，演示“上、下”两个方向。
- 顶部 16 行作为不透明标题栏，滚动内容从其下方穿过。

### 7.4 四方向翻页

每次切换页面按 左/上/右/下 轮换，用“先画底页、再用黑色 `DrawBox` 清出一块、
最后画新页”的方式实现不透明滑动，避免 u8g2 不支持负坐标的问题：

```c
case 0:  render_page(old, 0, 0);  clear_box(128-off, 0, off, 64);
         render_page(new, 128-off, 0);          break;   // 左
case 1:  render_page(old, 0, 0);  clear_box(0, 64-off, 128, off);
         render_page(new, 0, 64-off);           break;   // 上
case 2:  render_page(new, 0, 0);  clear_box(off, 0, 128-off, 64);
         render_page(old, off, 0);              break;   // 右
default: render_page(new, 0, 0);  clear_box(0, off, 128, 64-off);
         render_page(old, 0, off);              break;   // 下
```

### 7.5 动态图案（第 6 页）

- `Tools/generate_assets.py` 读取工程根目录的 `图例.jpg`（蓝底证件照）：
  先用“蓝色背景 + 高亮度”判据去掉蓝底，再转灰度、缩放到 48×64、按阈值 185 二值化，
  并沿人物轮廓补一圈白线，最后居中放进 64×64 画布。
- 生成 3 帧（人像放大 0%、约 5%、约 10%），形成“呼吸”效果；
  播放序列 `0 → 1 → 2 → 1`，每 260 ms 一帧。
- 叠加水平正弦摇摆（±5 px）和四角星星闪烁（7×7 / 9×9 两档）。
- 数据放在 `App/Src/duck_anim.c`，3 帧共 1536 字节，另加两颗星星。

---

## 8. “显示 API 运行失败”怎么排查

> 如果你说的是**网页/接口调用**报“API 运行失败”，那是会话侧的临时错误，
> 重新发送或刷新页面即可，与下面的 OLED 代码无关。
> 如果指 **OLED 显示相关 API 不工作**，按下面顺序排查，本项目已经内置了大部分防护。

| 现象 | 最可能原因 | 处理 |
|---|---|---|
| 全黑，`g_i2c_err_count` 一直涨 | 地址不对（0x78 vs 0x7A，7 位 0x3C/0x3D） | 代码自动扫描两个地址；也可在 `app_config.h` 改 `OLED_I2C_ADDR` |
| `HAL_I2C_Master_Transmit` 返回 `HAL_ERROR` | 从机无应答：没接屏、没供电、SDA/SCL 接反 | 万用表量 3.3V 和 GND；交换 PB6/PB7 试 |
| 返回 `HAL_BUSY` / 卡死 | 上次传输失败后总线被拉低 | 调用 `U8G2_Port_BusRecover()`（9 个时钟 + STOP） |
| 花屏、随机点 | I2C 速率太高 / 上拉不足 / 杜邦线太长 | 降到 100 kHz；加 4.7 kΩ 上拉；缩短线 |
| 有背光无内容 | 只初始化没开显示 | 必须 `u8g2_InitDisplay()` + `u8g2_SetPowerSave(&u8g2, 0)` |
| 画了但屏上不变 | 只 `DrawXxx` 没 `SendBuffer` | 每帧结束调用 `u8g2_SendBuffer()` |
| 一运行就 HardFault | 用了 `_f` 全屏模式但缓冲区不够 / 局部大数组爆栈 | 用 `_f` 时缓冲区需 1024 字节；本工程栈 0x400，缓冲区在静态区 |
| 文字乱码 / 中文不显示 | u8g2 字体不含汉字，或源文件编码不是 UTF-8 | 汉字用 `CnFont_Draw` 的 XBMP，不用 `DrawStr` |
| 调 `u8g2_Setup_...` 后无反应 | 选错显示屏型号或接口（SPI 函数用于 I2C 屏） | 本工程固定 `u8g2_Setup_ssd1306_i2c_128x64_noname_f` |
| Keil 下载失败 | ST-Link 驱动 / Reset 模式 / 读保护 | 选 `Connect under Reset`；用 STM32CubeProgrammer 解除读保护 |
| 只在 Keil 仿真里不显示 | 仿真器不模拟 I2C 外设 | 见第 9 节，改用软件 I2C 或真实硬件 |

第 7 页会实时显示 `OK / ERR` 计数、地址和 FPS：

```text
ADDR 0x3C/0x78
OK 1234  ERR 0
FPS 25.0  PAGE 7/7
```

- `ERR` 为 0 且画面正常 → 总线健康。
- `ERR` 持续增长 → 按上表检查接线/地址/上拉。

---

## 9. Keil 逻辑分析仪能采到 I2C 波形吗？

**结论：用硬件 I2C 时抓不到，SDA 一定是直线；改成 GPIO 软件 I2C 后才能看到位波形。**
本工程为了这件事，**默认已经使用软件 I2C**（`OLED_USE_SW_I2C=1`）。

原因（Keil 官方文档《Logic Analyzer Restrictions》原文）：

> I/O pin toggling of UART, CAN, SPI, or I²C communication peripherals is not simulated.

即：Keil 仿真器只模拟 CPU 和部分 GPIO，**不模拟 I2C 外设的引脚翻转**，
`HAL_I2C_Master_Transmit()` 不会真的在 PB6/PB7 上产生波形。逻辑分析仪本身也只是
波形查看器，不带 I2C 协议解码。

可以改用下面任一方法（本项目已实现第 ① 种）：

| 方法 | 做法 | 优点 |
|---|---|---|
| ① 软件 I2C + Keil 逻辑分析仪 | 默认已是 `OLED_USE_SW_I2C=1`；Logic Analyzer 添加 `PORTB.6`、`PORTB.7` | 无需硬件，能看到 START/地址/数据/STOP 位模式 |
| ② 真实逻辑分析仪 | Saleae / DSLogic / LA1010 + PulseView 的 I2C 解码器 | 最真实，能解出 ACK 和真实 400 kHz 时序 |
| ③ 示波器 | 看 SCL/SDA 电平、上升沿、毛刺 | 适合查硬件问题 |
| ④ 第二块单片机做 I2C 嗅探 | 从机模式或 GPIO 采样，通过 UART 打印 | 成本低，可长时间抓包 |
| ⑤ Proteus / SimulIDE / Wokwi | 带 SSD1306 外设模型，直接看 I2C 活动 | 适合教学演示 |
| ⑥ 固件自诊断 | 统计 HAL 返回值、翻转调试 IO、UART/SWO 打印 | 不用仪器也能定位 |

### 9.1 真实 I2C 写时序长什么样

以写一个命令字节 `0xAF`（SSD1306 开显示）为例：

```text
        START                                                     STOP
SDA ────┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌───┐   ┌──────
        └───┘   └───┘   └───┘   └───┘   └───┘   └───┘   └───┘   └───┘
SCL ──────┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌──────
          └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘
           A6  A5  A4  A3  A2  A1  A0  W  ACK  D7  D6 ... D0  ACK
          └────────── 0x78（0x3C<<1）──────────┘  └── 0xAF ──┘
```

总线上的实际字节流是：

```text
START | 0x78 | ACK | 0x00 | ACK | 0xAF | ACK | STOP
       地址+写      命令控制字节    命令数据
```

- 起始条件：SCL 高电平期间 SDA 由高变低。
- 停止条件：SCL 高电平期间 SDA 由低变高。
- 每个字节 8 位，MSB 先发，第 9 个时钟为 ACK（从机把 SDA 拉低）。
- 400 kHz 时 SCL 周期 2.5 µs，高/低电平约 1.25 µs；上升时间应 < 300 ns
  （4.7 kΩ 上拉 + 短走线）。

一次全屏刷新（8 页 × 128 字节）在总线上是 8 个事务，每个事务：

```text
START | 0x78 | ACK | 0x40 | ACK | 128 字节显存 | ACK ... | STOP
```

### 9.2 用软件 I2C 在 Keil 里抓波形

1. 确认 `App/Inc/app_config.h` 里 `OLED_USE_SW_I2C` 为 `1`（工程默认），`Rebuild`。
2. 进入 Debug，选择 **Use Simulator**。
3. `View → Analysis Windows → Logic Analyzer`，`Setup` 里添加：
   `PORTB.6`（SCL）、`PORTB.7`（SDA），Display Type 选 **Bit**。
4. 采样率调到 ≥ 10 MHz，全速运行 1～2 秒后暂停。
5. 应能看到 9 个时钟一组、地址 0x78（0111 1000）、控制字节 0x00/0x40、数据位。
6. 用 `File → Save` 导出，或截图后手动按位解码。

> 注意：Keil 仿真里没有 OLED 从机模型，**第 9 个 ACK 位会保持高电平（NACK）**；
> 真实硬件上 OLED 会把 SDA 拉低。仿真也**不能验证真实 400 kHz 时序**。

如果还是直线：

- 确认不是停在 `Error_Handler()`（软件 I2C 模式下 `main.c` 已跳过 `MX_I2C1_Init()`；
  若仿真器没有晶振模型，`SystemClock_Config()` 会自动回退到 HSI 8 MHz 继续运行）；
- 信号名必须是 `PORTB.6` / `PORTB.7`；
- 若提示信号不存在，`Options for Target → Debug → Simulator` 的 Dialog DLL 改为
  `DARMSTM.DLL`，参数 `-pSTM32F103C8`；
- 仍不行就说明该版本的 Keil 仿真不支持 GPIO，改用真实逻辑分析仪或 Proteus。

> 完整步骤、波形图和排查表见 [Docs/Keil逻辑分析仪抓I2C波形.md](Docs/Keil逻辑分析仪抓I2C波形.md)。

---

## 10. 验证记录

- Keil AC5 全编译：`0 Error(s), 0 Warning(s)`，生成 `OLED_U28G.hex`。
- 用 PC 端仿真器（`Tools/sim`）直接编译真实的 `app_ui.c` + u8g2 源码，
  导出 128×64 帧缓冲，逐页检查布局（见 `Docs/UI预览.png`）。
- 逐页检查过：启动页、图形演示、字体、学号姓名、水平滚动、垂直滚动、
  动态图案、系统信息；翻页四个方向均正常。

## 11. 参考资料

- u8g2 官方仓库与 Wiki：<https://github.com/olikraus/u8g2>
- SSD1306 数据手册（128×64，I2C 地址 0x3C/0x3D）
- STM32F103x8/xB 参考手册（RM0008）I2C 章节
- 正点原子/野火 OLED 教程（取模与 I2C 时序部分）
