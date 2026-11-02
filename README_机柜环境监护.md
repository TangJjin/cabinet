# STM32H747 工业机柜环境监护系统

沿用当前工程的 `CM4 / CM7 / APP / Common`、`bsp_xxx.c/h`、`bsp_system.h` 和 Keil 框架。屏幕沿用已运行的 10.1 寸 MIPI、1280×800、ILI9881C 初始化序列和现有触摸驱动。界面直接用 LVGL C 代码编写。

软件可在没有传感器和 TF 卡时进入模拟监测界面；板载 SDRAM、QSPI Flash、屏幕和双核启动仍需正常工作。默认开启模拟模式，界面顶部会明确标识。

## 先看这几个入口

| 内容 | 入口 |
|---|---|
| 文件用途与双核归属 | [本文文件导读](#工程文件导读与双核归属) |
| 四目标 Keil 工程 | [MDK-ARM/MIPIProject.uvprojx](MDK-ARM/MIPIProject.uvprojx) |
| CM7 启动、显示初始化 | [CM7/Core/Src/main.c](CM7/Core/Src/main.c) |
| CM4 采集任务 | [APP/SENSOR/bsp_sensor.c](APP/SENSOR/bsp_sensor.c) |
| 控制、存储、心跳任务 | [APP/SYSTEM/bsp_cabinet.c](APP/SYSTEM/bsp_cabinet.c) |
| 中文 LVGL 页面 | [APP/UI/bsp_cabinet_ui.c](APP/UI/bsp_cabinet_ui.c) |
| 报警与 Modbus 协议 | [Common/Src/cabinet_core.c](Common/Src/cabinet_core.c)、[cabinet_modbus.c](Common/Src/cabinet_modbus.c) |
| 升级状态机 | [Common/Src/cabinet_boot.c](Common/Src/cabinet_boot.c) |
| 接线与引脚 | [docs/接线与资源分配.md](docs/接线与资源分配.md) |
| RS485 Modbus RTU 入门 | [docs/Modbus入门与寄存器.md](docs/Modbus入门与寄存器.md) |
| 上板验收 | [docs/上板联调清单.md](docs/上板联调清单.md) |
| 编译、测试及交付文件记录 | [docs/软件验证记录.md](docs/软件验证记录.md) |
| 首次黑屏修正与诊断 | [docs/黑屏故障定位.md](docs/黑屏故障定位.md) |

历史飞机姿态界面和示例任务不参与当前目标编译；工程不再依赖工作空间外面的 GUI Guider 生成文件。`.ioc` 保留为原板卡配置参考，新加入的外设和分区由手写 BSP、scatter 文件管理，直接用 CubeMX 重新生成会覆盖部分改动。

## 编译与首次烧录

环境：Keil µVision 5、ARM Compiler 6.7、STM32H7xx DFP 2.7.0、ST-LINK。四个目标统一使用 Compiler 6，OpenAMP 的原子操作使用 libmetal 的编译器内建实现。

在工程根目录运行：

```powershell
./tools/build.ps1
# 需要全量重编译时：
./tools/build.ps1 -Rebuild
# Keil 安装路径不同时：
./tools/build.ps1 -Keil 'C:\Keil_v5\UV4\UV4.exe'
```

脚本只编译和导出，不连接或烧录板卡。若 Keil 已经打开这个工程，请重新加载磁盘上的工程文件。

编译得到四组 `.hex / .bin / .axf`，导出的 HEX/BIN 和日志在 `build/`，AXF/map 在 `MDK-ARM/<目标名>/`。首次烧录必须配齐四个目标：

| 目标 | Flash 起始地址 | 分区容量 | 功能 |
|---|---:|---:|---|
| Cabinet_Bootloader | 0x08000000 | 128 KiB | CM7 启动、校验、安装和回滚 |
| MIPIProject_CM7 | 0x08020000 | 896 KiB | 界面、报警、日志、升级管理 |
| Cabinet_CM4_Gate | 0x08100000 | 128 KiB | CM4 固定启动门，等待 CM7 放行 |
| MIPIProject_CM4 | 0x08120000 | 896 KiB | 传感器、水浸、核间通信 |

用 Python 3 合并首次烧录文件：

```powershell
python tools/pack_firmware.py factory build/Cabinet_Bootloader.hex build/MIPIProject_CM7.hex build/Cabinet_CM4_Gate.hex build/MIPIProject_CM4.hex --out build/FACTORY.hex
```

通过 STM32CubeProgrammer + ST-LINK 烧录 `FACTORY.hex`，勾选写后校验。初次迁移应擦除内部 Flash 后完整烧录，不能仅下载 CM7 应用。该操作会替换原内部固件，原工作空间已另做 ZIP 备份。

启动选项保持双核启动：CM7 启动地址 `0x08000000`，CM4 启动地址 `0x08100000`，不交换 Flash Bank。不要把 CM4 的硬件启动地址直接改到新应用地址，否则会绕过升级期间的启动隔离。初次烧录后断开调试运行并按板上复位键。

第一次启动会在板载 QSPI 中创建元数据，随后进入默认模拟模式。第一次建立基线信任 ST-LINK 的完整写入和校验；后续每次启动及离线升级都有镜像 CRC 检查。常规开发重新用 ST-LINK 覆盖应用时，也要处理 QSPI 中旧的版本/CRC 元数据，见联调文档。

## 功能与运行规则

- 温湿度：每秒读取一次，单位内部使用 0.01。默认温度 ≥35 ℃持续 5 秒报警，≤33 ℃解除；湿度 ≥80 %RH 持续 5 秒报警，≤75 %RH 解除。可在屏幕设置中修改。
- 水浸：20 ms 扫描，输入连续稳定 200 ms 后生效；低电平表示水浸。
- RS485：9600、8N1，读取功能码 03、起始寄存器 0、两个寄存器，单次等待最多 250 ms。连续 3 个轮询周期失败判离线，正常响应即恢复。暂不在同一个周期内额外重发。
- 数据失效：未得到首个有效样本、轮询失败或双核消息超过 1 秒未更新时，页面显示 `--`，不会把旧温湿度当作有效值。通信故障期间保留已触发的环境报警，直到有效新样本满足解除条件。
- 消音：确认当前活动告警。新的告警位、已经解除后再次发生的告警会响铃；同一持续告警不会定时重新响铃。可以按“重新响铃”撤销确认。
- 输出：绿灯通道 500 ms 翻转作为心跳；蓝灯通道用于报警，环境报警常亮，传感器/双核故障优先 200 ms 翻转。有源蜂鸣器 500 ms 间歇鸣叫。无继电器。
- 页面：运行总览、120 秒实时趋势、最近事件、设置、TF 卡升级。模拟按钮提供正常、温湿度超限、水浸、离线四种场景。
- RTC：LSE 时钟，本地时间由屏幕设置。未设置时日志写 `UNSET`；保持断电走时需要板卡 VBAT 供电。时间并非联网自动校准。
- 参数保存：应用后立即生效，静置 1 秒后由存储任务保存到 QSPI，保存尝试间隔至少 3 秒。界面区分“已保存”和“等待保存”。

## 日志与 TF 卡

使用 FAT16/FAT32 卡，不支持 exFAT；工程不会自动格式化卡。使用板载 TF 插槽的 SDMMC1 四线接口，不使用板载 SD NAND 的 SDMMC2。

`EVENTS.CSV` 记录报警触发/解除、消音、恢复响铃、配置保存、启动、升级请求和版本确认；`DATA.CSV` 每 10 秒记录采样快照。两类文件超过约 4 MiB 时各保留一份 `.OLD`，避免无限占满卡。

CSV 没有标题行，列依次为：

```text
本地写入时间,事件产生时的启动毫秒数,事件类型,值1,值2,
温度(0.01℃),湿度(0.01%RH),水浸,在线,模拟模式,连续失败次数,
双核在线,数据有效,固件版本整数
```

`ALARM` 的值1/值2分别是新增/解除的告警位掩码；`BOOT` 为 RCC 复位标志和最近升级结果；`UPDATE` 的值1为操作返回值（以无符号整数记录）。最近升级结果：0 出厂、1 确认成功、2 回滚、3 升级包被拒绝。

只有存储任务调用 FatFs 和可写 QSPI 接口。日志队列 32 条，满队列、无卡或写卡失败会累计“未保存日志”，历史不会无限缓存在 RAM。插卡后每 5 秒重试挂载。每条记录写完关闭文件同步；突然断电仍可能损坏 FAT 文件系统或最后一条日志，升级恢复不依赖 CSV 完整性。

## TF 卡升级与配套回滚

1. 修改 `Common/Inc/cabinet.h` 中 `CABINET_FW_VERSION`，例如从 `0x00010000`（1.0.0）改成 `0x00010001`（1.0.1）。两核一起重新编译。
2. 打包并检查：

```powershell
python tools/pack_firmware.py pack --cm7 build/MIPIProject_CM7.bin --cm4 build/MIPIProject_CM4.bin --out build/UPDATE.BIN
python tools/pack_firmware.py inspect build/UPDATE.BIN
```

3. 将 `UPDATE.BIN` 放到 TF 卡根目录，在“离线升级”页面确认安装。与当前版本相同的包会被拒绝。首次交付包是 1.0.0，烧入同版本 FACTORY 后再次安装它会被拒绝，这是预期行为。
4. CM7 应用检查包和两个镜像，把包写到 QSPI 暂存区，最后提交升级请求并重启。Bootloader 先备份当前两个固件，确认备份完整，再擦写两核应用。
5. 安装期间掉电：下次启动从 QSPI 重新安装完整的一对镜像；回滚期间掉电：重新恢复完整的一对旧镜像。固定 Bootloader/CM4 启动门不参与离线包更新。
6. 新固件处于试运行状态。CM7 三个工作任务和 CM4 采集/水浸任务持续健康 10 秒后确认版本。连续 3 次启动未确认，在第 4 次 Bootloader 执行时恢复上一对固件。确认后的常规看门狗复位不自动回滚。

包包含板卡 ID、格式、版本、地址、长度、各镜像 CRC32 和头部 CRC32；镜像内部还检查核类型、协议版本、向量表和 +0x400 的版本结构。CRC32 用于检查传输或存储损坏，不是数字签名。

| QSPI 区域 | 地址 | 大小 |
|---|---:|---:|
| 启动日志 A / B | 0x000000 / 0x001000 | 各 4 KiB |
| 配置 A / B | 0x002000 / 0x003000 | 各 4 KiB |
| 候选升级包 | 0x100000 | 2 MiB |
| 配套旧版本备份 | 0x300000 | 2 MiB |

启动和配置元数据均采用交替扇区、CRC、最后写提交字的形式。当前只使用 W25Q256 的低 16 MiB，以专用四字节地址命令 0x0C/0x12/0x21/0xDC 和单线数据操作，不依赖此前板卡例程保存的 ADP 地址模式。若 QSPI 损坏、两个应用都无有效镜像且备份也无效，Bootloader 停止放行、闪故障灯，此时使用 ST-LINK 恢复；未实现该状态下直接从 TF 恢复出厂固件的菜单。

## 任务与学习顺序

| 核 | 任务 | 优先级 | 主要周期/职责 |
|---|---|---:|---|
| CM4 | openamp | 3 | 5 ms 处理消息，100 ms 发布快照 |
| CM4 | rs485 | 2 | 1 s 采集，DMA 环形接收、任务通知、CRC/帧间隔判断 |
| CM4 | water | 2 | 20 ms 扫描与消抖 |
| CM7 | watchdog | 4 | 250 ms 检查任务进度，健康才喂 IWDG1 |
| CM7 | control | 3 | 20 ms 处理 OpenAMP、报警、输出和日志投递 |
| CM7 | lvgl | 2 | 5 ms 调度 LVGL，250 ms 刷新文字 |
| CM7 | storage | 1 | 队列消费、CSV、参数保存、升级、版本确认 |

看门狗约 32 秒（受 LSI 误差影响），应用只由监督任务喂狗。传感器物理断线属于设备故障，任务只要仍能按期完成超时处理就保持心跳，不会因此进入复位循环。存储长操作通过进度回调更新任务心跳，依然由监督任务决定是否喂狗。

建议按“模拟报警 → RS485 Modbus RTU → 单核采集 → OpenAMP 共享内存 → 队列与心跳 → 升级状态机”阅读。项目里包含协议、报警、QSPI 和升级状态机的独立 C 测试。

## 验证边界

已完成 ARM 编译、协议/报警/掉电状态机测试和真实 LVGL 内存渲染；页面预览位于 [docs/ui/overview.png](docs/ui/overview.png)。首次联调发现的 QSPI 引脚与地址模式问题已修正，随后经用户授权完整烧录并写后校验，实板双核、OpenAMP 模拟数据、任务心跳及 LVGL 刷新已验证。详细证据和尚待验收的真实外设、屏幕操作、升级物理断电项目见 [软件验证记录](docs/软件验证记录.md)。

原工作空间备份：`D:\Desktop\stm32_hal\STM32H747\MIPIProject\_codex_backups\cabinet_before_20261009_141313.zip`。备份包含原工程、外部界面文件以及原 Git 状态/差异；未使用 Git 重置或检出命令回退工作区。

## 工程文件导读与双核归属

以下按当前 Keil 工程的实际编译开关核对。APP/Common 目录名不能决定核归属；共用源文件会分别编进对应目标，不会让普通变量自动成为双核共享变量。
表中目标表示 `.c` 文件被哪个目标编译；配对 `.h` 提供接口和类型，随包含它的源文件使用，不单独编译。

| 固件目标 | 核 | 作用 |
|---|---|---|
| `Cabinet_Bootloader` | CM7 | 固定启动程序：校验、备份、安装、试运行计数、回滚 |
| `MIPIProject_CM7` | CM7 | 业务应用：界面、报警、日志、参数保存、TF 升级管理 |
| `Cabinet_CM4_Gate` | CM4 | 固定启动门：等待 CM7 放行，避免升级时运行半套固件 |
| `MIPIProject_CM4` | CM4 | 采集应用：温湿度、水浸、OpenAMP 通信 |

### APP：业务和板级驱动

| 文件（含配对头文件） | 编译归属 | 用途 |
|---|---|---|
| [APP/BOARD/bsp_board.c](APP/BOARD/bsp_board.c)<br>[APP/BOARD/bsp_board.h](APP/BOARD/bsp_board.h) | CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门 | 板级基础：系统时钟、MPU、看门狗、报警输出、CM4 放行和应用跳转；按编译宏选择核及启动阶段。 |
| [APP/IPC/bsp_ipc.c](APP/IPC/bsp_ipc.c)<br>[APP/IPC/bsp_ipc.h](APP/IPC/bsp_ipc.h) | CM7 应用、CM4 应用 | 双核消息接口：登记业务回调、建立 RPMsg 端点、发送消息，并在任务中轮询接收。 |
| [APP/LCD/bsp_mipi_lcd.c](APP/LCD/bsp_mipi_lcd.c)<br>[APP/LCD/bsp_mipi_lcd.h](APP/LCD/bsp_mipi_lcd.h) | CM7 应用 | MIPI 屏 BSP：复位、面板命令、背光控制和基础绘图；与 DSI/LTDC 配置配合。 |
| [APP/LED/bsp_led.c](APP/LED/bsp_led.c)<br>[APP/LED/bsp_led.h](APP/LED/bsp_led.h) | CM7 应用 | 原板卡 LED BSP：封装绿色、紫色通道 GPIO 控制；现用报警节奏由 bsp_board.c 管理。 |
| [APP/QSPI/bsp_qspi.c](APP/QSPI/bsp_qspi.c)<br>[APP/QSPI/bsp_qspi.h](APP/QSPI/bsp_qspi.h) | CM7 应用、CM7 Bootloader | 外部 W25Q256 Flash 驱动：初始化、读取、按页写入及扇区擦除，供配置和固件暂存使用。 |
| [APP/RTC/bsp_rtc.c](APP/RTC/bsp_rtc.c)<br>[APP/RTC/bsp_rtc.h](APP/RTC/bsp_rtc.h) | CM7 应用、CM7 Bootloader | RTC 时间服务：LSE 初始化、文本时间校验与设置、日志时间和 FatFs 时间戳。 |
| [APP/RTOS/bsp_rtos.c](APP/RTOS/bsp_rtos.c)<br>[APP/RTOS/bsp_rtos.h](APP/RTOS/bsp_rtos.h) | 保留代码，当前未编译 | 保留的旧演示任务，当前四个 Keil 目标均未编译；现用任务在 bsp_cabinet.c、bsp_sensor.c 等文件。 |
| [APP/SDRAM/bsp_sdram.c](APP/SDRAM/bsp_sdram.c)<br>[APP/SDRAM/bsp_sdram.h](APP/SDRAM/bsp_sdram.h) | CM7 应用 | 外部 SDRAM BSP：执行芯片上电命令序列并配置刷新，提供基础内存读写接口。 |
| [APP/SENSOR/bsp_sensor.c](APP/SENSOR/bsp_sensor.c)<br>[APP/SENSOR/bsp_sensor.h](APP/SENSOR/bsp_sensor.h) | CM4 应用 | CM4 采集：RS485 Modbus RTU 温湿度轮询、水浸消抖，以及 OpenAMP 参数接收和采样发布。 |
| [APP/STORAGE/bsp_sd.c](APP/STORAGE/bsp_sd.c)<br>[APP/STORAGE/bsp_sd.h](APP/STORAGE/bsp_sd.h) | CM7 应用、CM7 Bootloader | TF 卡块设备：通过 SDMMC1 初始化卡、读写 512 字节逻辑扇区并等待卡完成操作。 |
| [APP/STORAGE/diskio.c](APP/STORAGE/diskio.c) | CM7 应用、CM7 Bootloader | FatFs 适配：把文件系统的磁盘操作转接到 bsp_sd.c，并向文件系统提供 RTC 时间。 |
| [APP/SYSTEM/bsp_cabinet.c](APP/SYSTEM/bsp_cabinet.c)<br>[APP/SYSTEM/bsp_cabinet.h](APP/SYSTEM/bsp_cabinet.h) | CM7 应用 | CM7 业务中心：维护 model，运行控制、存储、监督任务，并提供界面使用的业务接口。 |
| [APP/SYSTEM/bsp_system.c](APP/SYSTEM/bsp_system.c)<br>[APP/SYSTEM/bsp_system.h](APP/SYSTEM/bsp_system.h) | CM7 应用 | 显示屏参数：保存原板卡已经验证的 ILI9881C 10.1 寸 MIPI 屏初始化命令表。 |
| [APP/TOUCH/bsp_touch.c](APP/TOUCH/bsp_touch.c)<br>[APP/TOUCH/bsp_touch.h](APP/TOUCH/bsp_touch.h) | CM7 应用 | 触摸屏 BSP：初始化触摸控制器、读取触点并转换坐标，供 LVGL 输入适配使用。 |
| [APP/TOUCH/bsp_touch_i2c.c](APP/TOUCH/bsp_touch_i2c.c)<br>[APP/TOUCH/bsp_touch_i2c.h](APP/TOUCH/bsp_touch_i2c.h) | CM7 应用 | 触摸 I2C 访问：封装触摸控制器寄存器的总线读写。 |
| [APP/UART/bsp_uart.c](APP/UART/bsp_uart.c)<br>[APP/UART/bsp_uart.h](APP/UART/bsp_uart.h) | CM7 应用 | CM7 调试串口：USART1 的打印重定向、DMA 接收及环形缓冲接入。 |
| [APP/UART/ringbuffer.c](APP/UART/ringbuffer.c)<br>[APP/UART/ringbuffer.h](APP/UART/ringbuffer.h) | CM7 应用 | 串口环形缓冲区：管理读写位置、可读数据量及剩余空间。 |
| [APP/UI/bsp_cabinet_ui.c](APP/UI/bsp_cabinet_ui.c)<br>[APP/UI/bsp_cabinet_ui.h](APP/UI/bsp_cabinet_ui.h) | CM7 应用 | 当前机柜 LVGL 界面：创建页面、处理按钮和输入，读取业务快照并刷新显示。 |
| [APP/UI/bsp_ui.c](APP/UI/bsp_ui.c)<br>[APP/UI/bsp_ui.h](APP/UI/bsp_ui.h) | 保留代码，当前未编译 | 保留的旧姿态仪表界面，当前四个 Keil 目标均未编译；机柜界面使用 bsp_cabinet_ui.c。 |
| [APP/UI/cabinet_font_24.c](APP/UI/cabinet_font_24.c) | CM7 应用 | 自动生成的 24 px 中文字体数据，由 LVGL 使用。 |
| [APP/UPDATE/bsp_update.c](APP/UPDATE/bsp_update.c)<br>[APP/UPDATE/bsp_update.h](APP/UPDATE/bsp_update.h) | CM7 应用、CM7 Bootloader | 存储与升级适配：TF 包写入 QSPI、配置双副本保存，以及 Bootloader 的内部 Flash 擦写接口。 |

### CM7：应用入口和外设配置

| 文件（含配对头文件） | 编译归属 | 用途 |
|---|---|---|
| [CM7/Core/Src/dma.c](CM7/Core/Src/dma.c)<br>[CM7/Core/Inc/dma.h](CM7/Core/Inc/dma.h) | CM7 应用 | CubeMX DMA 时钟与中断基础配置；CM4 当前采集 DMA 由 bsp_sensor.c 自行初始化。 |
| [CM7/Core/Src/dma2d.c](CM7/Core/Src/dma2d.c)<br>[CM7/Core/Inc/dma2d.h](CM7/Core/Inc/dma2d.h) | CM7 应用 | DMA2D 初始化：配置图形内存搬运/填充硬件。 |
| [CM7/Core/Src/dsihost.c](CM7/Core/Src/dsihost.c)<br>[CM7/Core/Inc/dsihost.h](CM7/Core/Inc/dsihost.h) | CM7 应用 | DSI Host 初始化：配置 STM32 到 MIPI 面板的显示链路。 |
| [CM7/Core/Src/fmc.c](CM7/Core/Src/fmc.c)<br>[CM7/Core/Inc/fmc.h](CM7/Core/Inc/fmc.h) | CM7 应用 | FMC 初始化：配置外部 SDRAM 控制器时序及对应 GPIO。 |
| [CM7/Core/Src/gpio.c](CM7/Core/Src/gpio.c)<br>[CM7/Core/Inc/gpio.h](CM7/Core/Inc/gpio.h) | CM7 应用 | CubeMX GPIO 初始化：保留板卡引脚配置，新增业务输出还由各 BSP 配置。 |
| [CM7/Core/Src/i2c.c](CM7/Core/Src/i2c.c)<br>[CM7/Core/Inc/i2c.h](CM7/Core/Inc/i2c.h) | CM7 应用 | I2C2 初始化：提供触摸控制器访问所需的硬件总线。 |
| [CM7/Core/Src/ltdc.c](CM7/Core/Src/ltdc.c)<br>[CM7/Core/Inc/ltdc.h](CM7/Core/Inc/ltdc.h) | CM7 应用 | LTDC 初始化：配置显示时序、图层及帧缓冲地址。 |
| [CM7/Core/Src/main.c](CM7/Core/Src/main.c)<br>[CM7/Core/Inc/main.h](CM7/Core/Inc/main.h) | CM7 应用 | CM7 应用入口：板级初始化、SDRAM/显示屏、业务与 IPC 初始化、CM4 放行、LVGL 和调度器启动。 |
| [CM7/Core/Src/stm32h7xx_hal_msp.c](CM7/Core/Src/stm32h7xx_hal_msp.c) | CM7 应用 | HAL MSP 底层回调：为外设初始化时钟、GPIO 或 DMA 等底层资源。 |
| [CM7/Core/Src/stm32h7xx_it.c](CM7/Core/Src/stm32h7xx_it.c)<br>[CM7/Core/Inc/stm32h7xx_it.h](CM7/Core/Inc/stm32h7xx_it.h) | 保留代码，当前未编译 | 保留的原 CubeMX 中断文件，当前目标未编译；现用入口在 Common/Src/cabinet_interrupts.c。 |
| [CM7/Core/Src/usart.c](CM7/Core/Src/usart.c)<br>[CM7/Core/Inc/usart.h](CM7/Core/Inc/usart.h) | CM7 应用 | USART1 初始化：配置 CM7 调试串口和 RX DMA。 |

### CM4：应用入口和外设配置

| 文件（含配对头文件） | 编译归属 | 用途 |
|---|---|---|
| [CM4/Core/Src/dma.c](CM4/Core/Src/dma.c)<br>[CM4/Core/Inc/dma.h](CM4/Core/Inc/dma.h) | 保留代码，当前未编译 | CubeMX DMA 时钟与中断基础配置；CM4 当前采集 DMA 由 bsp_sensor.c 自行初始化。 |
| [CM4/Core/Src/main.c](CM4/Core/Src/main.c)<br>[CM4/Core/Inc/main.h](CM4/Core/Inc/main.h) | CM4 应用 | CM4 应用入口：更新本核时钟信息，初始化 HAL，创建采集任务并启动本核 FreeRTOS。 |
| [CM4/Core/Src/stm32h7xx_hal_msp.c](CM4/Core/Src/stm32h7xx_hal_msp.c) | CM4 应用 | HAL MSP 底层回调：为外设初始化时钟、GPIO 或 DMA 等底层资源。 |
| [CM4/Core/Src/stm32h7xx_it.c](CM4/Core/Src/stm32h7xx_it.c)<br>[CM4/Core/Inc/stm32h7xx_it.h](CM4/Core/Inc/stm32h7xx_it.h) | 保留代码，当前未编译 | 保留的原 CubeMX 中断文件，当前目标未编译；现用入口在 Common/Src/cabinet_interrupts.c。 |

### Common：算法、协议和双核适配

| 文件（含配对头文件） | 编译归属 | 用途 |
|---|---|---|
| [Common/Src/cabinet_boot.c](Common/Src/cabinet_boot.c)<br>[Common/Inc/cabinet_boot.h](Common/Inc/cabinet_boot.h) | CM7 应用、CM7 Bootloader | 双核升级状态机：启动记录双副本、镜像校验、配套备份/安装、试运行确认和回滚。 |
| [Common/Src/cabinet_core.c](Common/Src/cabinet_core.c) | CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门 | 通用业务算法：CRC32、默认配置和校验、报警迟滞/延时/消音、升级包及向量表基本检查。 |
| [Common/Src/cabinet_image.c](Common/Src/cabinet_image.c) | CM7 应用、CM4 应用 | 应用镜像说明：在 CABINET_INFO 段写入板卡、固件、核类型和协议版本，供升级校验读取。 |
| [Common/Src/cabinet_interrupts.c](Common/Src/cabinet_interrupts.c) | CM7 应用、CM4 应用 | 当前应用中断入口：SysTick 同时服务 HAL/FreeRTOS，按核接入 HSEM 和 CM7 调试串口中断。 |
| [Common/Src/cabinet_modbus.c](Common/Src/cabinet_modbus.c) | CM7 应用、CM4 应用 | Modbus RTU 协议：传感器请求组包、CRC16 校验、温湿度响应解析。 |
| [Common/Src/cabinet_retarget.c](Common/Src/cabinet_retarget.c) | CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门 | C 运行库适配：关闭半主机依赖，处理断言/退出，并按目标提供最小打印占位。 |
| [Common/Src/cabinet_rtos_hooks.c](Common/Src/cabinet_rtos_hooks.c) | CM7 应用、CM4 应用 | FreeRTOS 错误钩子：内存申请失败或任务栈溢出时进入统一故障处理。 |
| [Common/Src/mbox_hsem.c](Common/Src/mbox_hsem.c)<br>[Common/Inc/mbox_hsem.h](Common/Inc/mbox_hsem.h) | CM7 应用、CM4 应用 | 核间门铃：HSEM 0/1 负责通知，ISR 置 pending，任务轮询时才推进 OpenAMP 接收。 |
| [Common/Src/openamp.c](Common/Src/openamp.c)<br>[Common/Inc/openamp.h](Common/Inc/openamp.h) | CM7 应用、CM4 应用 | STM32 OpenAMP 平台适配：登记共享 SRAM，建立 VirtIO/RPMsg 及两个 VRing，接入 HSEM 通知。 |
| [Common/Src/rsc_table.c](Common/Src/rsc_table.c)<br>[Common/Inc/rsc_table.h](Common/Inc/rsc_table.h) | CM7 应用、CM4 应用 | 共享资源表：由 CM7 写入两条 VRing 的地址、容量和通知编号，CM4 读取同一份布局。 |
| [Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c](Common/Src/system_stm32h7xx_dualcore_boot_cm4_cm7.c) | CM7 应用、CM4 应用、CM7 Bootloader、CM4 启动门 | 芯片启动支持：SystemInit、系统时钟频率计算，以及按构建目标选择向量表位置。 |

### Bootloader：两个固定启动入口

| 文件（含配对头文件） | 编译归属 | 用途 |
|---|---|---|
| [Bootloader/CM4/main.c](Bootloader/CM4/main.c) | CM4 启动门 | CM4 固定启动门：在 STOP 中等待 HSEM 15，校验 CM7 放行信息后跳转 CM4 应用。 |
| [Bootloader/CM7/main.c](Bootloader/CM7/main.c) | CM7 Bootloader | CM7 固定 Bootloader：等待 CM4 启动门停驻，校验或安装双核固件，成功后跳转 CM7 应用。 |

### 独立头文件

| 文件 | 用途/归属 |
|---|---|
| [Common/Inc/cabinet.h](Common/Inc/cabinet.h) | 四目标使用的类型、固件/协议版本、Flash 地址、双核消息、升级包格式。 |
| [Common/Inc/openamp/env.h](Common/Inc/openamp/env.h) | OpenAMP/libmetal 的最小平台兼容声明，供两核应用编译使用。 |
| [Common/Inc/openamp/errno.h](Common/Inc/openamp/errno.h) | OpenAMP/libmetal 的最小平台兼容声明，供两核应用编译使用。 |
| [Common/Inc/openamp/fcntl.h](Common/Inc/openamp/fcntl.h) | OpenAMP/libmetal 的最小平台兼容声明，供两核应用编译使用。 |
| [Common/Inc/openamp/libgen.h](Common/Inc/openamp/libgen.h) | OpenAMP/libmetal 的最小平台兼容声明，供两核应用编译使用。 |
| [Common/Inc/openamp/unistd.h](Common/Inc/openamp/unistd.h) | OpenAMP/libmetal 的最小平台兼容声明，供两核应用编译使用。 |
| [Common/Inc/openamp_conf.h](Common/Inc/openamp_conf.h) | 两个应用共用的共享 SRAM 地址、VRing 容量和 libmetal 设置。 |
| [CM7/Core/Inc/stm32h7xx_hal_conf.h](CM7/Core/Inc/stm32h7xx_hal_conf.h) | 本核 HAL 模块开关和参数，相应固定启动目标也使用该核的配置。 |
| [CM4/Core/Inc/stm32h7xx_hal_conf.h](CM4/Core/Inc/stm32h7xx_hal_conf.h) | 本核 HAL 模块开关和参数，相应固定启动目标也使用该核的配置。 |

### 工程、第三方组件和电脑工具

| 路径/文件 | 用途和归属 |
|---|---|
| `MDK-ARM/MIPIProject.uvprojx` | 四目标源码清单、编译宏和包含目录；核归属的实际依据。 |
| `MDK-ARM/MIPIProject_CM7.sct、MIPIProject_CM4.sct、Cabinet_Bootloader.sct、Cabinet_CM4_Gate.sct` | 四目标的 Flash/RAM 链接分区，其他旧 scatter 文件保留参考。 |
| `MDK-ARM/startup_stm32h747xx_CM7.s、startup_stm32h747xx_CM4.s` | 对应核的向量表、初始栈和复位入口，分别供应用和启动目标使用。 |
| `MDK-ARM/armlib_lock_glue.c、stm32_lock.h` | 保留的 C 库锁适配，当前目标未编译该 C 文件。 |
| `FreeRTOS/tasks.c、queue.c、list.c、timers.c、event_groups.c、stream_buffer.c` | 两个应用分别编译的内核组件；编译组件不表示业务一定使用对应 API。 |
| `FreeRTOS/include/FreeRTOSConfig.h` | 两核应用共用的 RTOS 配置：优先级、tick、钩子、堆等。 |
| `FreeRTOS/portable/GCC/ARM_CM7/r0p1/port.c、ARM_CM4F/port.c` | CM7 与 CM4 各自使用的上下文切换和临界区底层。 |
| `FreeRTOS/portable/MemMang/heap_4.c` | 两核各自的 RTOS 动态内存管理，两份堆互相独立。 |
| `Middlewares/OpenAMP/open-amp、libmetal` | 两个应用使用的 RPMsg、VirtIO、共享队列和平台抽象库。 |
| `Middlewares/FatFs` | CM7 应用和 Bootloader 目标编译的文件系统组件；实际 TF 日志和升级准备由应用存储任务执行。 |
| `Middlewares/WIZnet` | 历史网络驱动资料，已从全部目标的源码、宏和包含目录移除，当前固件不使用。 |
| `lvgl/src、lvgl/lv_conf.h` | CM7 的界面库和配置。 |
| `lvgl/examples/porting/lv_port_disp_template.c` | CM7 LVGL 显示适配：将绘图数据送到现有屏幕帧缓冲。 |
| `lvgl/examples/porting/lv_port_indev_template.c` | CM7 LVGL 输入适配：读取原触摸 BSP。 |
| `Drivers/CMSIS、Drivers/STM32H7xx_HAL_Driver` | 核/芯片寄存器定义和 HAL 驱动，各目标选择需要的模块。 |
| `tools/build.ps1` | 电脑端：编译四目标并导出 HEX/BIN，不执行烧录。 |
| `tools/pack_firmware.py` | 电脑端：合并出厂 HEX、制作/检查配套升级包。 |
| `tools/test.ps1、tests/test_core.c、test_boot.c、test_qspi.c、test_tools.py` | 电脑端：RTU/报警、升级掉电状态机、QSPI 和打包工具验证。 |
| `tools/preview_ui.ps1、tools/render_preview.py、tests/ui` | 电脑端：运行真实 LVGL、验证交互并生成预览图。 |
| `tools/configure_project.py` | 工程重建工具，已同步移除网络；它会重写工程清单/scatter，日常编译使用 build.ps1。 |
| `tools/font/generate.ps1、package.json、package-lock.json、OFL.txt` | 生成中文字体，记录工具版本和字体许可。 |
| `.clang-format` | 本次统一的 4 空格缩进、展开短语句/函数、100 列排版规则。 |
| `MIPIProject.ioc、原根目录 scatter、STM32H747_UI` | 原板卡和旧界面参考；当前四目标不使用旧生成界面源文件。 |
| `build、MDK-ARM/<目标名>` | 构建产物和日志，业务修改应在源文件中进行。 |

### 建议阅读顺序

1. CM7 的 `main.c`：初始化、共享资源准备和 CM4 放行。
2. `bsp_cabinet.h`、`cabinet.h`：model、配置、采样和双核消息结构。
3. `bsp_cabinet.c`：控制、存储和监督任务。
4. CM4 的 `main.c`、`bsp_sensor.c`：温湿度采集、水浸和通信任务。
5. `bsp_ipc.c`、`mbox_hsem.c`：双核业务消息和硬件通知。
6. `bsp_update.c`、`cabinet_boot.c`、两个 Bootloader 入口：配置保存、安装与回滚。
