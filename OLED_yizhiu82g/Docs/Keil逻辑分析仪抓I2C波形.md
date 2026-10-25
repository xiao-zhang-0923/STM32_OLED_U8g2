# 用 Keil 虚拟逻辑分析仪抓 I2C（SDA/SCL）波形

> 现象：Logic Analyzer 里 SDA 是一条直线，没有任何翻转。  
> 结论：**这不是接线或代码错，而是 Keil 仿真器根本不模拟 I2C 外设的引脚。**
> 必须把 I2C 改成 GPIO 软件模拟（bit-bang），逻辑分析仪才能看到波形。

---

## 1. 为什么硬件 I2C 一定是直线

Keil 官方《Logic Analyzer Restrictions》明确写着：

> I/O pin toggling of UART, CAN, SPI, or I²C communication peripherals is not simulated.
> （UART、CAN、SPI、I²C 等通信外设的 I/O 引脚翻转不会被仿真。）

也就是说：

- 你用 `HAL_I2C_Master_Transmit()` 时，CPU 只是在写 I2C 寄存器；
- 仿真器不会真的把 `0x78`、`0x00`、数据一位一位地推到 PB6/PB7 上；
- 所以逻辑分析仪加 `PORTB.6` / `PORTB.7` 只能看到一条直线。

参考：

- <https://www.keil.com/support/man/docs/uv4cl/uv4cl_db_dbg_logicanalyzer_restrictions.htm>
- <https://www.keil.com/support/man/docs/uv4cl/uv4cl_db_dbg_logicanalyzer_setupex.htm>

---

## 2. 解决办法：软件 I2C（GPIO 翻转）

工程里已经写好了软件 I2C，只用一个宏切换：

```c
/* App/Inc/app_config.h */
#ifndef OLED_USE_SW_I2C
#define OLED_USE_SW_I2C     1      /* 1 = 软件 I2C，0 = 硬件 I2C1 */
#endif
```

`1` 时：

- `main.c` 不再调用 `MX_I2C1_Init()`，避免仿真时卡在 HAL 的 I2C 初始化；
- `SW_I2C_Init()` 把 PB6/PB7 配成开漏输出；
- `u8x8_byte_stm32_sw_i2c()` 用 GPIO 手动产生 START、地址、数据、ACK 时钟、STOP；
- `sw_delay()` 用 NOP 循环，不依赖 DWT，仿真器里不会死等。

> 当前工程默认就是 `1`，所以重新编译后就能抓波形。  
> 如果要跑硬件 I2C 400 kHz，把它改成 `0` 再编译即可。

---

## 3. 操作步骤

1. 打开 `MDK-ARM/OLED_U28G.uvprojx`，`Rebuild`。
2. `Debug → Start/Stop Debug Session`，确认使用的是 **Use Simulator**。
3. `View → Analysis Windows → Logic Analyzer`。
4. 点 `Setup...`，`New`，输入：
   - `PORTB.6`（SCL）
   - `PORTB.7`（SDA）
5. `Display Type` 选 **Bit**；采样率尽量高（例如 10 MHz）。
6. 点 `Close`，按 `F5` 全速运行，稍等 1～2 秒后按 `Stop`。
7. 应该能看到一串串方波：

```text
SCL  ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐ ┌─┐
   ──┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └─┘ └──
SDA  ──┐  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──
       └──┘  A6 A5 A4 A3 A2 A1 A0 W  ACK
```

一次完整的写事务：

```text
START | 0x78 | ACK | 0x00（命令）或 0x40（数据） | ACK | 数据… | ACK | STOP
```

- 地址 `0x78` 的二进制是 `0111 1000`，应能在 SDA 上数出来；
- 每 9 个 SCL 时钟为一组（8 位数据 + 1 位 ACK）；
- 一屏 8 页，每页 1 个控制字节 + 128 字节数据。

> 仿真里没有 OLED 从机模型，第 9 个时钟 **SDA 会保持高电平（NACK）**，
> 这是正常的；真实硬件上 OLED 会把 SDA 拉低（ACK）。

---

## 4. 还是直线？按这个顺序检查

| 检查项 | 正确做法 |
|---|---|
| 1. 宏是否真的生效 | `app_config.h` 里 `OLED_USE_SW_I2C` 为 `1`，重新 `Rebuild`；不要只点 `Build` 增量编译 |
| 2. 程序是否在跑 | 暂停后看 PC 是否停在 `Error_Handler()`；若停在那里，通常是硬件 I2C 初始化在仿真里超时，确认 `main.c` 已跳过 `MX_I2C1_Init()`。本工程已加入时钟回退：HSE 在仿真里起不来时自动改用 HSI 8 MHz，不会再卡死在时钟初始化 |
| 3. 信号名 | 必须是 `PORTB.6`、`PORTB.7`（不是 `PB6`、`GPIOB6`） |
| 4. Display Type | 选 `Bit`，不要选 Analog |
| 5. 仿真 DLL | `Options for Target → Debug → Simulator` 的 Dialog DLL 设为 `DARMSTM.DLL`，参数 `-pSTM32F103C8`（部分工程默认是 `DCM.DLL`，那样连 GPIO 都不模拟） |
| 6. 运行时间 | 全速运行至少几百毫秒再暂停；I2C 初始化很短 |
| 7. 芯片 | 网上反馈 Keil 软件仿真主要支持 STM32F103 系列；F4 等型号可能不支持外设仿真 |
| 8. 断点 | 不要在 `SW_I2C_Init()` 之前停住；确认已经执行到 `u8g2_InitDisplay()` |

如果第 3 步输入 `PORTB.6` 时提示信号不存在，说明仿真器没有加载 STM32 外设模型，
按第 5 项把 Dialog DLL 改成 `DARMSTM.DLL`。

---

## 5. 仿真波形的局限

- **只能看“位模式”，不能看真实 400 kHz 时序**：仿真按指令周期推进，SCL 周期和真实硬件不一致；
- **没有 ACK**：没有 OLED 从机模型，ACK 位为高；
- **不能验证上拉、上升沿、总线电容**：这些必须用真实硬件 + 示波器/逻辑分析仪。

所以：

- 想验证**协议位序** → Keil 软件 I2C + 逻辑分析仪就够了；
- 想验证**真实时序/ACK/上拉** → 用 Saleae / DSLogic / LA1010 + PulseView，或示波器；
- 想同时看显示效果和 I2C → Proteus / SimulIDE / Wokwi，或真实板子 + 逻辑分析仪。

---

## 6. 推荐的替代方案

| 方法 | 能看到什么 | 备注 |
|---|---|---|
| Keil + 软件 I2C + Logic Analyzer | START/地址/数据/STOP 位模式 | 免费，本工程已支持 |
| 真实逻辑分析仪 + PulseView | 完整 I2C 解码、ACK、时序 | 最推荐，几十元起 |
| 示波器双通道 | SCL/SDA 电平、上升沿、毛刺 | 查上拉和信号完整性 |
| 第二块 MCU 做 I2C 嗅探 | 长时间抓包、统计错误 | 成本低 |
| Proteus / SimulIDE / Wokwi | 带 SSD1306 模型的完整仿真 | 适合演示 |
| 固件自诊断 | HAL 返回值、错误计数、FPS | 第 7 页已实现 |
