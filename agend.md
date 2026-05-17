# USARTdebug / F103 发射端维护议程

## 当前项目状态

本项目包含 Qt 串口上位机和 STM32F103C8T6 发射端代码。

当前 F103 支持两种音频输入模式：

- 数字音频模式：PC Qt 上位机通过 USART1 发送 44100Hz / mono / unsigned 8-bit PCM。
- 模拟音频模式：F103 使用 ADC1 采样 PA0，转换为 8bit PCM 后通过 nRF24L01 发射。

模式选择：

- `PB12` 内部上拉输入。
- `PB12 = 高电平`：数字 UART PCM 模式。
- `PB12 = 低电平`：ADC 模拟输入模式。
- `PB13` 推挽输出低电平，可用跳线帽短接 `PB12-PB13`，方便选择 ADC 模式。

OLED 当前定位为上电自检屏，不做运行期刷新，避免阻塞主循环影响音频实时性。

## F103 代码结构

```text
F103发射端/
  User/
    main.c

  App/
    app_audio_stream.c/.h    数字 UART PCM 输入任务
    app_adc_stream.c/.h      ADC 模拟输入任务
    app_config.c/.h          模式选择配置

  Bsp/
    bsp_time.c/.h            DWT 延时和周期计数
    bsp_spi.c/.h             SPI1 基础传输
    bsp_usart_dma.c/.h       USART1 + DMA1_Channel5 接收
    bsp_timer.c/.h           TIM2 数字音频发包节拍
    bsp_adc_dma.c/.h         ADC1 + DMA1_Channel1 + TIM3_TRGO
    bsp_gpio.c/.h            PB12 模式选择，PB13 固定低电平
    bsp_i2c.c/.h             PB8/PB9 软件 I2C

  Drivers/
    nrf24l01.c/.h            nRF24L01 驱动
    oled_i2c.c/.h            SSD1306 OLED 驱动
    oled_font.h              OLED 8x16 ASCII 字库

  Protocol/
    audio_defs.h             公共参数、包大小、ACK 标志
    audio_packet.c/.h        32B nRF 音频 payload 打包

  Debug/
    debug_probe.c/.h         Keil Watch 调试变量
```

## 硬件资源占用

数字 UART PCM 模式：

```text
USART1 TX/RX: PA9 / PA10
DMA1_Channel5: USART1_RX
TIM2: 680.27us 音频发包节拍
SPI1: nRF24L01
```

ADC 模拟输入模式：

```text
ADC1_IN0: PA0
DMA1_Channel1: ADC1
TIM3_TRGO: ADC 外部触发采样
SPI1: nRF24L01
```

nRF24L01：

```text
CE   PB0
CSN  PA4
SCK  PA5
MISO PA6
MOSI PA7
```

模式选择和 OLED：

```text
PB12: 内部上拉输入，低电平选择 ADC 模式
PB13: 推挽输出低电平，用于跳线帽短接 PB12
PB8 : OLED SCL，软件 I2C
PB9 : OLED SDA，软件 I2C
```

## OLED 上电自检

OLED 当前只显示启动时的自检内容，`oled_i2c_task()` 只渲染一次。

显示内容：

```text
BOOT SELF CHECK
MODE:UART PCM / MODE:ADC
SPI:OK/ERR RF:076
CFG:xx SET:xx
```

含义：

- `MODE`：PB12 检测后的输入模式。
- `SPI`：nRF SPI 自检结果，来自 `DBG_NrfSpiSelfTestPass`。
- `RF`：nRF `RF_CH` 寄存器，当前期望为 `076`。
- `CFG`：nRF `CONFIG` 寄存器，TX 初始化后通常期望为 `3E`。
- `SET`：nRF `RF_SETUP` 寄存器，当前 2Mbps / 0dBm 通常期望为 `0F`。

注意：当前 OLED 是阻塞式软件 I2C，不建议放进音频主循环周期刷新。后续如果要显示运行状态，应开发非阻塞 OLED 刷新任务。

## 关键协议

nRF24L01 payload 固定 32B：

```text
Byte0: 0xA1
Byte1: seq，自增 uint8
Byte2~31: 30 个 unsigned 8-bit PCM 采样点
```

数字模式 PC -> F103：

```text
450B PCM 数据块
15 * 30B = 450B
```

F103 -> PC ACK：

```text
Byte0: 0xAC
Byte1: 0xCA
Byte2: status low
Byte3: status high
```

当前 ACK 逻辑：

- 预缓冲阶段允许 PC 先填充缓冲。
- 运行阶段每消费满 450B 产生一次 ACK 资格。
- 发出 READY 后，必须等 USART DMA 收到下一块 450B，才允许下一次运行期 ACK。
- USART ORE 会自动清除并重启 RX DMA。

## 维护原则

1. `main.c` 只负责公共初始化和模式选择，不写具体业务逻辑。
2. `App` 层负责音频流状态机，不直接操作寄存器。
3. `Bsp` 层负责 MCU 外设初始化、中断和基础读写。
4. `Drivers` 层负责外部器件，例如 nRF24L01、OLED。
5. `Protocol` 层只放协议常量、payload 打包、ACK 格式等硬件无关内容。
6. `Debug` 层集中管理 Watch 变量，避免调试变量散落到各模块。
7. 数字和模拟两种输入可以同时存在于工程，但运行时只能二选一。
8. 修改定时器、DMA、USART、ADC 前，先检查资源占用表。
9. 阻塞式外设操作不要放到实时音频路径中。

## 代码规范

- 新模块提供 `.h` 和 `.c`，接口保持最小。
- 头文件只暴露必要函数和类型，不暴露模块内部变量。
- 中断函数只做轻量工作：清中断、置标志、记录计数。
- 大块数据搬运放在主循环 task 中处理。
- 模块内部状态优先用 `static` 限制作用域。
- 公共常量放入 `Protocol/audio_defs.h`。
- Debug 变量统一以 `DBG_` 开头。
- Watch 计数变量保持 `volatile`。
- 不在中断里调用 nRF 发送、串口阻塞发送、OLED 刷新或复杂循环。

## 重点 Watch 变量

数字 UART 模式：

```c
DBG_PcmRxBlockCount
DBG_PcmRingAvailable
DBG_PcmRingAvailableMin
DBG_PcmRingAvailableMax
DBG_PcmEmptyCount
DBG_PcmOverflowCount
DBG_AckSendCount
DBG_AckConsumedBytes
DBG_AckWaitingBlock
DBG_UsartOreCount
DBG_DmaCndtr
DBG_UsartSr
```

ADC 模式：

```c
adc_half_count
adc_half_overrun_count
DBG_AdcHalfIntervalUs
DBG_TxBacklog
DBG_TxIntervalUs
DBG_TxSendBusyUs
```

nRF 通用：

```c
nrf_tx_packet_count
nrf_tx_ok_count
nrf_tx_fail_count
nrf_tx_fifo_full_count
DBG_NrfLastStatus
DBG_NrfFifoStatus
DBG_NrfSpiSelfTestPass
DBG_LastTxPayload
```

## 注意事项

- TIM2 已用于数字串口模式发包节拍，ADC 模式使用 TIM3_TRGO。
- ADC 模式和数字模式共享 SPI1/nRF24L01，不能同时运行两个 task。
- USART1 RX DMA 使用 `DMA1_Channel5`，ADC1 DMA 使用 `DMA1_Channel1`。
- 数字模式收不到数据时，先看 `DMA1_Channel5->CCR`、`DBG_DmaCndtr`、`DBG_UsartOreCount`。
- `DBG_PcmEmptyCount` 增长说明发包时环形缓冲不足 30B。
- `DBG_AckSendCount` 明显大于 `DBG_PcmRxBlockCount` 时，说明 ACK 发出但 PC 数据没有正常到达。
- Keil 新增文件后，确认 include path 包含 `App/Bsp/Drivers/Protocol/Debug`。

## 后续拓展

短期：

- 固化当前 UART PCM 和 ADC 两种模式。
- 保持 OLED 仅用于上电自检。
- 给 `debug_probe` 增加计数清零函数。

中期：

- 开发非阻塞 OLED 刷新任务，用于低频显示运行状态。
- 增加版本号、自检结果、错误码。
- 设计可扩展 payload 类型，支持音频、调试变量、状态上报。

长期：

- 支持更多音频输入源。
- 支持可配置采样率。
- 支持接收端状态回传或双向链路。

## Qt 上位机打包注意事项

- Release 构建后使用 `windeployqt` 收集 Qt 运行库。
- 当前音频转换依赖 `ffmpeg.exe`，发布目录需要能找到 ffmpeg。
- 可以把 `ffmpeg.exe` 放到 exe 同目录，或者要求用户把 ffmpeg 加入 PATH。
- 打包前测试串口列表、打开串口、音频转换、ACK 驱动发送四个流程。
