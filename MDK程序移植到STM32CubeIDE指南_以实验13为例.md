# MDK 程序移植到 STM32CubeIDE 指南——以实验13为例

> 适用对象：已经有一个可用的 STM32CubeIDE 目标工程，需要把 MDK-ARM（Keil）工程中的程序迁入其中。  
> 本文不讲如何新建 STM32CubeIDE 工程，也不直接创建实验13的 CubeIDE 工程；重点是移植时怎样分析、怎样取舍、需要配置什么、为什么这样配置，以及出现问题时怎样逐层定位。

---

## 1. 先明确“移植”的真正含义

MDK 工程不是一组可以被另一个 IDE 原样打开的源文件。一个可执行程序实际由以下几层共同决定：

1. **目标芯片和内存布局**：芯片型号、Flash/RAM 起始地址和容量、栈、堆、中断向量表位置。
2. **启动过程**：复位后设置栈、执行 `SystemInit()`、初始化 C 运行库、进入 `main()`。
3. **编译条件**：全局宏、头文件搜索路径、C 语言标准、优化级别、编译器扩展。
4. **参与构建的文件集合**：不是目录里所有 `.c` 文件，而是 MDK 工程实际勾选并编译的那些文件。
5. **链接和运行库**：链接脚本、标准库、半主机、`printf` 重定向、未使用段删除规则。
6. **下载和调试**：调试探针、下载地址、复位方式和调试接口。
7. **板级行为**：外部晶振、引脚、电源、外部总线时序和外设型号。

因此，移植的本质不是“把文件复制进 IDE”，而是把 MDK 中隐含的构建契约重新表达为 STM32CubeIDE/GNU 工具链能够理解的形式。

一个可靠的迁移顺序应当是：

```text
确认 MDK 基线可用
        ↓
提取 MDK 构建契约
        ↓
建立 CubeIDE 中等价的启动/链接环境
        ↓
只加入实际参与构建的源文件
        ↓
处理 ARMCC 与 GCC 的差异
        ↓
先通过编译和链接
        ↓
按“时钟→LED→串口→FSMC→LCD”顺序验证运行
```

不要一开始同时改目录结构、更新 HAL 版本、改用 CubeMX 生成的外设初始化、重写驱动并迁移编译器。变量越多，越难判断故障属于哪一层。

---

## 2. 实验13的已知基线

本文依据以下原始工程进行分析：

```text
2，标准例程-HAL库版本/
└─ 实验13 TFTLCD（MCU屏）实验/
   ├─ Drivers/
   ├─ Middlewares/
   ├─ Output/
   ├─ Projects/MDK-ARM/atk_f103.uvprojx
   ├─ User/
   └─ readme.txt
```

从 `atk_f103.uvprojx` 和实验源码可以得到下列事实：

| 项目        | 原 MDK 工程配置                            |
| --------- | ------------------------------------- |
| Target 名称 | `TFTLCD`                              |
| MCU       | `STM32F103ZE`                         |
| 内核        | Cortex-M3，无 FPU                       |
| Flash     | `0x08000000`，512 KiB                  |
| SRAM      | `0x20000000`，64 KiB                   |
| MDK 编译器   | ARM Compiler 5.06 update 7，非 AC6      |
| C 标准      | 启用 C99                                |
| 优化        | MDK Level 1                           |
| 全局宏       | `USE_HAL_DRIVER`、`STM32F103xE`        |
| 输出        | `atk_f103`，生成 HEX                     |
| 主时钟       | 板载 8 MHz HSE，经 PLL × 9 得到 72 MHz      |
| 串口        | USART1，PA9/PA10，115200 bit/s          |
| LCD 接口    | FSMC Bank1 NE4，16 位 8080 并口，A10 作为 RS |

原程序的可观察现象是：

- LCD 循环切换 12 种背景色并显示固定字符串和 LCD 控制器 ID；
- LED0 每约 1 秒翻转一次；
- 复位后串口输出一次 LCD ID；
- 支持的是 MCU 并口屏，不是 RGB 屏。

这些现象就是移植验收标准。仅仅做到“0 Error”不能算移植完成。

---

## 3. 移植前先做一张“构建清单”

### 3.1 以 `.uvprojx` 为准，不以磁盘目录为准

实验13目录中有大量 HAL 源文件，但 MDK 工程只编译其中一小部分。如果把整个 `Drivers/STM32F1xx_HAL_Driver/Src` 目录直接放进 CubeIDE 的自动构建范围，可能引入：

- 不需要的外设驱动；
- HAL 模板文件；
- Legacy 与当前实现并存；
- 额外的 MSP、时基或中断实现；
- 编译时间增加和重复符号。

实验13的 MDK 工程实际参与构建的 C 文件如下。

#### 用户和系统层

```text
User/main.c
Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
User/stm32f1xx_it.c
Drivers/SYSTEM/delay/delay.c
Drivers/SYSTEM/sys/sys.c
Drivers/SYSTEM/usart/usart.c
```

#### HAL/LL 层

```text
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_usart.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_sram.c
Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_ll_fsmc.c
```

#### BSP 层

```text
Drivers/BSP/LED/led.c
Drivers/BSP/LCD/lcd.c
```

对应的头文件不需要作为独立编译单元，但必须能通过头文件搜索路径找到，尤其是：

```text
User/stm32f1xx_hal_conf.h
User/stm32f1xx_it.h
Drivers/SYSTEM/**/*.h
Drivers/BSP/**/*.h
Drivers/BSP/LCD/lcdfont.h
Drivers/CMSIS/**/*.h
Drivers/STM32F1xx_HAL_Driver/Inc/**/*.h
```

### 3.2 三个必须排除的特殊文件

#### 1. `User/main1.c` 和 `User/main2.c`

它们存在于磁盘中，但不在 MDK 工程构建清单内，而且还引用了本实验目录中没有纳入当前工程的 KEY 驱动。CubeIDE 若自动编译它们，会出现多个 `main()` 或缺少头文件等问题。

处理原则：**不加入构建，或对 Debug/Release 全部执行 Exclude from Build。**

#### 2. `Drivers/BSP/LCD/lcd_ex.c`

`lcd.c` 中已经直接写了：

```c
#include "./BSP/LCD/lcd_ex.c"
```

这不是常规写法，但它是原工程当前的组织方式。因此 `lcd_ex.c` 的函数会在编译 `lcd.c` 时一起进入 `lcd.o`。若 CubeIDE 又单独编译 `lcd_ex.c`，链接阶段会出现大量 LCD 初始化函数重复定义。

处理原则：**保留 `lcd.c` 中的包含关系时，必须把 `lcd_ex.c` 排除在独立构建之外。**

较长期的重构方案是把 `lcd_ex.c` 改为正常独立编译单元，并为其提供头文件声明，但这属于驱动重构，不应与第一次 IDE 迁移同时进行。

#### 3. `startup_stm32f103xe.s`

原文件位于：

```text
Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/arm/startup_stm32f103xe.s
```

文件中使用 `AREA`、`SPACE`、`PRESERVE8`、`EXPORT`、`IMPORT` 等 ARMASM 语法，并且跳转到 ARM C 库的 `__main`。GNU assembler 不能直接编译它。

处理原则：**不要迁移这个 ARMASM 启动文件；使用 CubeIDE 为 STM32F103ZETx 提供的 GNU 启动文件。** 常见文件名类似 `startup_stm32f103zetx.s`，实际以目标 CubeIDE 工程生成或选用的文件为准。

---

## 4. 源码复制还是链接：先决定工程边界

将原文件放入 CubeIDE 工程有两种常见方式。

### 4.1 复制到目标工程

优点：

- 工程自包含，换电脑和归档更容易；
- 不会因为修改原例程而影响目标工程；
- 路径变量较少，构建更稳定。

缺点：

- 后续若修复原驱动，需要手动同步；
- 可能出现多份 HAL/BSP 副本。

对于第一次完成实验13迁移，通常推荐复制，并保持 `Drivers`、`User` 的相对目录结构不变，这样原代码中的 `./SYSTEM/...`、`./BSP/...` 包含路径无需大改。

### 4.2 使用 Linked Resource 链接原文件/目录

优点：

- 原例程与 CubeIDE 工程共用一份源代码；
- 适合多个工程共享 BSP。

缺点：

- 绝对路径很容易导致工程只能在当前电脑使用；
- 在 CubeIDE 中修改链接文件就是修改原例程；
- 链接整个目录时，CubeIDE 可能把不应参与构建的 `.c` 文件一并发现。

如果使用链接方式，应定义工作区路径变量，例如 `EXP13_ROOT`，不要把 `D:\...` 写死在工程设置中；同时逐个核对 Source Location 的 exclusion pattern 或资源的 Exclude from Build 状态。

### 4.3 不要混用两套 HAL/CMSIS

目标 CubeIDE 工程通常已经带有一套 CMSIS 和 HAL，原 MDK 例程也自带一套。两套同时参与编译最容易导致：

- 同名类型或宏来自不同版本；
- `stm32f1xx_hal_conf.h` 选错；
- 同名 HAL 源文件重复链接；
- 某个头文件来自 A 版本、实现却来自 B 版本。

第一次迁移建议选择下面两种路线之一：

| 路线   | 做法                                                    | 适用场景               |
| ---- | ----------------------------------------------------- | ------------------ |
| 保真路线 | 使用实验13自带 CMSIS、HAL、SYSTEM、BSP，只替换 GNU 启动文件和链接脚本       | 先复现实验现象，变量最少       |
| 升级路线 | 使用目标 CubeIDE 工程的 CMSIS/HAL，仅迁移 SYSTEM、BSP、应用代码并适配 API | 明确需要升级 HAL，且愿意逐项验证 |

本文后续默认采用“保真路线”。升级 HAL 应当在保真版本运行通过后单独进行。

---

## 5. CubeIDE 中需要重建的编译契约

以下内容不是创建工程步骤，而是目标工程必须满足的配置条件。不同 CubeIDE 版本菜单文字可能略有差异，通常位于：

```text
Project Properties
└─ C/C++ Build
   └─ Settings
      └─ Tool Settings
```

### 5.1 MCU 和工具链

应确认：

- MCU/Part Number：`STM32F103ZETx`；
- CPU：Cortex-M3；
- 指令集：Thumb；
- FPU：None；
- Float ABI：无硬件 FPU 配置，不要误设为 hard；
- 工具链：STM32CubeIDE 随附的 GNU Tools for STM32。

`STM32F103ZE` 的封装和容量信息必须正确。选成同系列的中容量器件，即使部分代码能编译，也可能使用错误的启动文件、链接容量或中断向量定义。

### 5.2 全局预处理宏

在 MCU GCC Compiler 的 Preprocessor 中加入：

```text
USE_HAL_DRIVER
STM32F103xE
```

两个宏作用不同：

- `STM32F103xE` 决定 `stm32f1xx.h` 最终包含 `stm32f103xe.h`，从而得到正确的寄存器和中断定义；
- `USE_HAL_DRIVER` 决定设备头文件继续包含 HAL 入口头文件。

不要直接修改 `stm32f1xx.h` 去“永久打开”芯片宏。宏属于构建配置，应当在 Debug、Release 等所有需要的 Configuration 中明确设置。

### 5.3 头文件搜索路径

原 MDK 工程配置了：

```text
Drivers/CMSIS/Device/ST/STM32F1xx/Include
Drivers/STM32F1xx_HAL_Driver/Inc
Drivers/CMSIS/Include
Drivers
User
Middlewares
```

实验13必须保留 `Drivers` 这一层搜索路径，因为源码使用了：

```c
#include "./SYSTEM/sys/sys.h"
#include "./BSP/LCD/lcd.h"
```

编译器需要从 `Drivers` 根目录开始解析 `SYSTEM` 和 `BSP`。

建议通过 Workspace/Project 浏览按钮添加路径，让 IDE 使用 `${workspace_loc:...}`、`${ProjDirPath}` 或相对路径，不要硬编码当前电脑的绝对路径。

至少要为 C Compiler 配置上述路径。GNU 启动文件若只使用自身符号，Assembler 不一定需要全部 C 头文件路径；若启动文件包含设备头文件，则应将相关 CMSIS 路径同时加到 Assembler。

### 5.4 语言标准、警告和优化

原工程启用了 C99，CubeIDE 可以使用 `gnu99`、`gnu11` 或项目默认的 GNU C 方言。实验13本身不依赖特别新的语法，重点是不要启用与源码不兼容的严格模式。

建议分阶段使用优化：

- 初次运行和单步调试：`-Og` 或 `-O0`；
- 与 MDK Level 1 做行为/尺寸对比：`-O1`；
- Release：验证通过后再选择更高优化。

初次迁移不建议直接开启“所有警告均视为错误”。先看清警告来源，再修正未使用参数、格式化参数类型和编译器专用代码。不要用全局关闭警告掩盖真实问题。

原 MDK 工程还启用了短枚举相关选项。实验13没有预编译第三方库，也没有明显依赖枚举尺寸的接口，因此通常不必机械地加 `-fshort-enums`。只有当 ABI、通信结构体、二进制文件格式或预编译库明确依赖枚举宽度时才需要保持一致。

### 5.5 源文件编码

实验13的主要 `.c`、`.h` 和 `readme.txt` 不是 UTF-8，实际是 GBK/本地 ANSI 风格。CubeIDE 若按 UTF-8 打开，会出现中文注释乱码；在乱码状态下保存，还可能不可逆地破坏原文件。

可选做法：

1. 在工程或文件属性中把 Text file encoding 设置为 GBK；
2. 先备份，再一次性把源码转换为 UTF-8，并在团队中统一编码。

编码只影响注释时通常不阻止编译，但字符串常量中存在中文时还会改变固件字节内容，因此不能只把它当显示问题。

---

## 6. 启动文件和链接脚本：不能“看起来差不多”

### 6.1 启动文件必须属于 GNU 工具链和准确芯片

CubeIDE 工程中应只有一个有效启动文件，且应满足：

- 为 STM32F103ZETx/高密度 F103 器件提供正确的中断向量表；
- 使用 GNU assembler 语法；
- 定义 `Reset_Handler`；
- 调用 `SystemInit()`；
- 完成 `.data` 复制、`.bss` 清零和 C/C++ 运行库初始化；
- 最终调用 `main()`。

必须排除原 MDK 的 ARMASM 启动文件，也不能同时保留两个 GNU 启动文件。

典型错误现象：

- 编译时报 `AREA`、`PRESERVE8`、`EXPORT` 等“未知指令”；
- 链接时报多个 `Reset_Handler` 或 `.isr_vector`；
- 下载后停在复位或 HardFault，进不了 `main()`。

### 6.2 链接脚本的内存区域

STM32F103ZE 的基本内存定义应等价于：

```ld
MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}
```

同时检查：

- 向量表所在的 `.isr_vector` 被放在 Flash 起始位置并使用 `KEEP()`；
- `.text`、`.rodata` 位于 Flash；
- `.data` 的加载地址在 Flash、运行地址在 RAM；
- `.bss` 位于 RAM；
- 栈和堆预留没有超过 64 KiB RAM。

原 MDK 启动文件中栈为 `0x400` 字节、堆为 `0x200` 字节。如果需要尽量等价，可检查 CubeIDE 链接脚本中的 `_Min_Stack_Size`、`_Min_Heap_Size`；是否需要增加则取决于实际调用深度和动态内存使用，而不是只照抄数值。

实验13没有 Bootloader，标准链接地址是 `0x08000000`，向量表也不应额外偏移。如果未来放到 Bootloader 之后，Flash ORIGIN、可用 LENGTH、下载地址和 `SCB->VTOR` 必须成套修改。

### 6.3 FSMC 的 `0x6C000000` 不需要写入链接脚本

LCD 通过内存映射 I/O 访问 FSMC Bank1 NE4。`lcd.h` 根据 NE4 和 A10 计算：

```text
LCD_REG = 0x6C0007FE
LCD_RAM = 0x6C000800
```

这些地址是外设总线窗口，不是用来放置 `.data`、`.bss` 或堆的普通 RAM，因此不需要为 LCD 在链接脚本中增加一个 MEMORY 段。真正需要保证的是：

- 芯片确实具有 FSMC；
- FSMC 和相关 GPIO 时钟已打开；
- NE4、A10、NWE、NOE、D0~D15 的引脚连接正确；
- 访问之前已经完成 `HAL_SRAM_Init()`。

---

## 7. `main()`、时钟和 CubeMX 代码所有权

CubeIDE 目标工程可能包含 `.ioc`，并生成 `Core/Src/main.c`、`SystemClock_Config()`、`MX_GPIO_Init()` 等代码。原实验也有自己的 `main()` 和初始化函数。两套初始化不能不加判断地叠加。

### 7.1 首次保真迁移的推荐做法

为了尽量保持实验13行为：

- 只保留原 `User/main.c` 作为唯一 `main()`；
- 保留原 `sys_stm32_clock_init(RCC_PLL_MUL9)`；
- 保留 `usart_init()`、`led_init()`、`lcd_init()`；
- 不再调用 CubeMX 生成的 `SystemClock_Config()`、`MX_USART1_UART_Init()`、`MX_FSMC_Init()`；
- 确保 Cube 生成的另一个 `main.c` 不参与构建。

原初始化顺序是：

```c
HAL_Init();
sys_stm32_clock_init(RCC_PLL_MUL9);  /* HSE 8 MHz -> SYSCLK 72 MHz */
delay_init(72);
usart_init(115200);
led_init();
lcd_init();
```

这个顺序有依赖关系：延时系数依赖 72 MHz，LCD 初始化过程依赖延时，串口波特率也依赖正确的外设时钟。

### 7.2 需要继续用 `.ioc` 生成代码时

若项目将长期由 CubeMX/CubeIDE 维护，不建议让生成器管理的 `main.c` 被整文件替换。可在保真版本验证成功后再做第二阶段整理：

- 保留生成的 `main.c`；
- 把实验主循环迁入 `app.c/app.h`，例如 `app_init()`、`app_process()`；
- 只在 `USER CODE BEGIN/END` 区域调用应用接口；
- 时钟、USART、FSMC 到底由 CubeMX 生成代码初始化，还是由原 BSP 初始化，必须逐个外设确定唯一所有者；
- 若改用 `MX_FSMC_Init()`，必须逐字段对照原 `lcd_init()` 的 Bank、总线宽度、扩展模式和读写时序，不能只做到“启用 FSMC”。

最重要的规则是：**同一硬件资源只保留一套初始化路径。**

### 7.3 避免重复的系统文件和回调

以下符号在 CubeIDE 自动生成代码和原实验中都可能存在：

```text
main
SystemInit
SystemCoreClock
SysTick_Handler
USART1_IRQHandler
HAL_UART_MspInit
HAL_SRAM_MspInit
HAL_Delay
```

迁移时应按“符号”检查，而不是只按文件名检查。例如原实验的：

- `USART1_IRQHandler()` 和 `HAL_UART_MspInit()` 位于 `usart.c`；
- `HAL_SRAM_MspInit()` 位于 `lcd.c`；
- `HAL_Delay()` 位于 `delay.c`，会覆盖 HAL 中的弱实现；
- `SysTick_Handler()` 位于 `stm32f1xx_it.c`。

如果 Cube 生成的 `stm32f1xx_hal_msp.c`、`stm32f1xx_it.c` 也定义同名强符号，就必须选择一套实现或合并逻辑。

---

## 8. ARMCC 到 GCC 的代码差异

### 8.1 CMSIS 汇编封装通常可以直接保留

`sys.c` 使用：

```c
__ASM volatile("wfi");
__ASM volatile("cpsid i");
__ASM volatile("cpsie i");
__set_MSP(addr);
```

这里的 `__ASM` 和 `__set_MSP()` 由 CMSIS 根据编译器适配。只要使用正确的 CMSIS 头文件，GCC 下通常无需改写。不要因为看见汇编就立刻替换；先区分“CMSIS 已封装的内联汇编”和“ARMASM 独立启动文件”。

### 8.2 实验13的 `printf` 重定向必须处理

原 `usart.c` 的重定向逻辑专门面向 ARMCC：

- AC5 使用 `#pragma import(__use_no_semihosting)`；
- AC6 使用 `__use_no_semihosting`、`__ARM_use_no_argv`；
- 定义 `_ttywrch()`、`_sys_exit()`、`_sys_command_string()`；
- 通过重写 `fputc()` 把字符写入 USART。

在 GCC 中，未定义的 `__ARMCC_VERSION` 在 `#if` 表达式中会按 0 处理，因此代码会落入 AC5 分支。GCC 可能只警告并忽略 `#pragma import`，但这并不等于重定向正确。GNU/newlib 的 `printf` 通常最终需要 `_write()` 系统调用，或者由 CubeIDE 的 `syscalls.c` 间接调用 `__io_putchar()`。

#### 方案 A：目标工程已有 `syscalls.c`

先查看其中的 `_write()` 是否调用 `__io_putchar()`。如果是，只需提供唯一的强实现：

```c
#if defined(__GNUC__)
int __io_putchar(int ch)
{
    while ((USART_UX->SR & USART_SR_TXE) == 0U)
    {
    }

    USART_UX->DR = (uint8_t)ch;
    return ch;
}
#endif
```

#### 方案 B：没有可用的 `syscalls.c`

提供 `_write()`：

```c
#if defined(__GNUC__)
int _write(int file, char *ptr, int len)
{
    int i;
    (void)file;

    for (i = 0; i < len; i++)
    {
        while ((USART_UX->SR & USART_SR_TXE) == 0U)
        {
        }

        USART_UX->DR = (uint8_t)ptr[i];
    }

    return len;
}
#endif
```

然后把 MDK 专用半主机代码放进明确的编译器条件中，例如：

```c
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
/* 原 MDK/ARMCC 重定向代码 */
#elif defined(__GNUC__)
/* GCC 重定向代码 */
#else
#error Unsupported compiler
#endif
```

注意事项：

- `_write()`、`__io_putchar()` 不要同时存在多套强实现；
- 若链接参数使用了 `--specs=nosys.specs`，仍可由用户实现覆盖需要的系统调用；
- 实验13只使用 `%x/%X` 和字符串，不需要为 `printf` 开启浮点格式化；
- 若在中断中打印或多任务环境中打印，还要考虑阻塞和重入，本实验主流程暂不涉及；
- USART 初始化之前不要调用这套串口输出。

### 8.3 格式化参数类型

原 `main.c` 中有：

```c
sprintf((char *)lcd_id, "LCD ID:%04X", lcddev.id);
```

缓冲区 12 字节刚好能容纳 11 个可见字符和结尾 `\0`，但迁移时更建议显式限制长度并匹配参数类型：

```c
snprintf((char *)lcd_id,
         sizeof(lcd_id),
         "LCD ID:%04X",
         (unsigned int)lcddev.id);
```

这样可以消除较严格 GCC 格式检查下的类型警告，也避免后续格式变化造成越界。

---

## 9. 实验13最值得注意的可移植性隐患：FSMC 时序结构体

`lcd_init()` 当前声明了：

```c
FSMC_NORSRAM_TimingTypeDef fsmc_read_handle;
FSMC_NORSRAM_TimingTypeDef fsmc_write_handle;
```

随后只设置了：

```text
AddressSetupTime
AddressHoldTime
DataSetupTime
AccessMode
```

但是该 HAL 版本的 `FSMC_NORSRAM_TimingTypeDef` 还包含：

```text
BusTurnAroundDuration
CLKDivision
DataLatency
```

HAL/LL 初始化代码会读取这些成员并写入寄存器。它们若未初始化，就取决于当前栈内容；编译器、优化级别或前序函数稍有变化，结果就可能不同。这类问题经常被误判成“Keil 能跑，CubeIDE 不能跑”。

另外，原代码将 `AddressHoldTime` 设为 0，而本 HAL 的参数约束要求 1~15。当前 `USE_FULL_ASSERT` 未启用，因此不会立即报告；一旦开启完整断言，初始化可能停在断言中。

建议在进行运行结果对比前先把结构体完整、确定地初始化。例如：

```c
FSMC_NORSRAM_TimingTypeDef fsmc_read_handle = {0};
FSMC_NORSRAM_TimingTypeDef fsmc_write_handle = {0};

fsmc_read_handle.AddressSetupTime      = 0;
fsmc_read_handle.AddressHoldTime       = 1;
fsmc_read_handle.DataSetupTime         = 15;
fsmc_read_handle.BusTurnAroundDuration = 0;
fsmc_read_handle.CLKDivision           = 2;
fsmc_read_handle.DataLatency           = 2;
fsmc_read_handle.AccessMode            = FSMC_ACCESS_MODE_A;

fsmc_write_handle.AddressSetupTime      = 0;
fsmc_write_handle.AddressHoldTime       = 1;
fsmc_write_handle.DataSetupTime         = 1;
fsmc_write_handle.BusTurnAroundDuration = 0;
fsmc_write_handle.CLKDivision           = 2;
fsmc_write_handle.DataLatency           = 2;
fsmc_write_handle.AccessMode            = FSMC_ACCESS_MODE_A;
```

说明：异步 SRAM/LCD 模式下，`CLKDivision` 和 `DataLatency` 不参与实际异步访问时序，但仍应赋予 HAL 接受的确定值；`AddressHoldTime` 在当前访问模式下通常不起关键作用，但也应满足参数约束。

第一次验证建议先保留原实验的关键读写时序：

- 读：`AddressSetupTime = 0`，`DataSetupTime = 15`；
- 写：`AddressSetupTime = 0`，`DataSetupTime = 1`；
- 异步模式 A；
- 读写使用不同的时序（Extended Mode Enable）。

如果读取 ID 不稳定、屏幕偶发花屏或某些屏型不能初始化，可在确认接线和主频正确后适当增大 `DataSetupTime`，尤其是写时序。调时序应一次只改一个参数并记录结果，不要同时修改总线宽度、Bank 和地址线。

---

## 10. 实验13的硬件映射必须保持一致

### 10.1 基础外设

| 功能        | 引脚/参数              |
| --------- | ------------------ |
| LED0      | PB5                |
| LED1      | PE5（本主程序主要使用 LED0） |
| USART1 TX | PA9                |
| USART1 RX | PA10               |
| 串口波特率     | 115200             |
| HSE       | 8 MHz              |
| SYSCLK    | 72 MHz             |

### 10.2 LCD 控制信号

| LCD 信号 | FSMC/MCU 引脚 |
| ------ | ----------- |
| CS     | NE4 / PG12  |
| RS     | A10 / PG0   |
| WR     | NWE / PD5   |
| RD     | NOE / PD4   |
| BL     | PB0         |

### 10.3 LCD 16 位数据总线

| LCD 数据 | MCU 引脚 | LCD 数据 | MCU 引脚 |
| ------ | ------ | ------ | ------ |
| D0     | PD14   | D8     | PE11   |
| D1     | PD15   | D9     | PE12   |
| D2     | PD0    | D10    | PE13   |
| D3     | PD1    | D11    | PE14   |
| D4     | PE7    | D12    | PE15   |
| D5     | PE8    | D13    | PD8    |
| D6     | PE9    | D14    | PD9    |
| D7     | PE10   | D15    | PD10   |

对应的 FSMC 参数必须是：

```text
Bank                 = FSMC_NORSRAM_BANK4
DataAddressMux       = Disable
MemoryDataWidth      = 16 bit
WriteOperation       = Enable
ExtendedMode         = Enable
AsynchronousWait     = Disable
Burst/WriteBurst     = Disable
AccessMode           = Mode A
```

屏幕是内存映射设备，代码能正常编译并不说明引脚和总线配置正确。尤其要避免让 `.ioc` 生成的 GPIO 初始化在 `lcd_init()` 之后又把 FSMC 引脚改回普通 GPIO。

---

## 11. HAL 配置与实现文件要成对出现

实验13使用原 `User/stm32f1xx_hal_conf.h`。其中启用了许多 HAL 模块，但原 MDK 工程并没有把所有模块的 `.c` 实现都加入构建。这本身不一定有问题：头文件被包含不等于对应函数一定会被链接。

与当前实验直接相关的关键模块包括：

```text
HAL_RCC_MODULE_ENABLED
HAL_FLASH_MODULE_ENABLED
HAL_GPIO_MODULE_ENABLED
HAL_CORTEX_MODULE_ENABLED
HAL_DMA_MODULE_ENABLED
HAL_UART_MODULE_ENABLED
HAL_USART_MODULE_ENABLED
HAL_SRAM_MODULE_ENABLED
```

如果改用 CubeIDE 工程自己的 `stm32f1xx_hal_conf.h`，必须至少确保实际调用到的模块已开启。典型对应关系为：

| 报错/缺失符号                 | 优先检查                                             |
| ----------------------- | ------------------------------------------------ |
| `SRAM_HandleTypeDef` 未知 | `HAL_SRAM_MODULE_ENABLED`、`stm32f1xx_hal_sram.h` |
| `HAL_SRAM_Init` 未定义     | `stm32f1xx_hal_sram.c`                           |
| `FSMC_NORSRAM_Init` 未定义 | `stm32f1xx_ll_fsmc.c`                            |
| `HAL_UART_Init` 未定义     | `stm32f1xx_hal_uart.c`                           |
| GPIO/RCC HAL 符号未定义      | 对应 HAL 模块宏和 `.c` 文件                              |

还要确认 `HSE_VALUE` 为 `8000000U`。如果头文件来自另一套 HAL 且 HSE 数值不同，`SystemCoreClock` 和波特率计算可能错误，即使 PLL 配置代码表面上仍是 ×9。

---

## 12. 推荐的分阶段移植和验证方法

### 阶段 0：冻结 MDK 基线

在改动前记录：

- MDK 是否能无错误编译；
- 现有 `Output/atk_f103.hex` 是否能正常运行；
- LCD 型号和读取到的 ID；
- LED 周期和串口输出；
- 使用的板卡、屏、电源、跳线和下载器。

若原始 HEX 都不能在当前硬件上运行，就不能把后续问题简单归因于移植。

### 阶段 1：只验证启动、链接和时钟

目标：能够进入唯一的 `main()`，`SystemInit()` 被调用，程序不在复位和 HardFault 中循环。

检查点：

- `Reset_Handler` 能命中；
- `main()` 能命中；
- `SystemCoreClock` 在时钟初始化后为 72 MHz；
- RCC 的 SYSCLK 来源为 PLL，AHB=72 MHz、APB1=36 MHz、APB2=72 MHz。

注意：IDE 中填写的 CPU Frequency 主要影响调试显示或工具设置，不能代替程序实际配置 RCC。

### 阶段 2：验证 LED 和延时

暂时不要把“LCD 不亮”作为第一个运行判断。先验证：

- LED0 引脚能被正确配置；
- `delay_ms(1000)` 的量级正确；
- SysTick 在运行且没有两个 `SysTick_Handler()`。

如果 LED 周期明显不对，先回头检查时钟和 `delay_init(72)`，不要调 FSMC。

### 阶段 3：验证串口

检查：

- PA9/PA10 跳线连接到板载 USB 转串口；
- 115200、8N1；
- GCC 的 `_write()` 或 `__io_putchar()` 已生效；
- 没有启用意外的 semihosting；
- USART1 中断入口只有一个，并调用 `HAL_UART_IRQHandler()`。

可以在 `lcd_init()` 之前临时打印一条固定 ASCII 文本，以区分“串口重定向问题”和“LCD 初始化卡死”。

### 阶段 4：验证 FSMC 基础访问

进入 `lcd_init()` 后检查：

- FSMC、GPIOD、GPIOE、GPIOG、GPIOB 时钟已开启；
- `HAL_SRAM_Init()` 返回 `HAL_OK`；
- `0x6C0007FE` 和 `0x6C000800` 的访问没有触发 BusFault；
- NE4、A10、NWE、NOE 上能观察到访问波形；
- 数据总线宽度为 16 位。

如果在首次 LCD 地址访问时进入 HardFault，查看：

```text
SCB->CFSR
SCB->HFSR
SCB->BFAR
```

这比只看 PC 停在 `HardFault_Handler()` 更有价值。

### 阶段 5：验证 LCD ID 和显示

实验驱动支持多种控制器，代码会依次尝试读取 ID。合理结果应是驱动已支持的 ID，例如：

```text
0x9341  0x7789  0x5310  0x7796  0x5510  0x9806  0x1963
```

常见异常含义：

- 总是 `0x0000`：数据线被拉低、读控制/片选未工作或屏未供电；
- 总是 `0xFFFF`：总线悬空、片选未选中、RD 未工作或屏未连接；
- 每次 ID 不同：读时序过快、数据线冲突、电源或接触不稳定；
- ID 正确但白屏：背光、写时序、初始化序列或屏型判断问题；
- ID 正确但花屏：写时序、总线数据位映射或供电完整性问题。

4.3/7 寸等较大屏幕可能需要更大电流。USB 供电不足会造成看似随机的复位、白屏或初始化失败，这不是编译器问题。

### 阶段 6：恢复完整主循环并做回归

最后确认：

- 12 种背景色按顺序切换；
- 固定字符串位置和颜色正确；
- LCD ID 显示和串口打印一致；
- LED0 约每秒翻转；
- 连续复位、重新下载、冷启动均可运行；
- Debug 与 Release 至少各验证一次。

---

## 13. 编译、链接和运行错误的分层排查表

| 现象                                | 所属层        | 优先检查                                      |
| --------------------------------- | ---------- | ----------------------------------------- |
| 找不到 `stm32f1xx.h`                 | 预处理        | CMSIS Device Include 路径                   |
| 找不到 `./BSP/LCD/lcd.h`             | 预处理        | 是否加入 `Drivers` 根路径                        |
| 提示未选择 STM32F1 器件                  | 预处理        | `STM32F103xE` 宏                           |
| `SRAM_HandleTypeDef` 未知           | HAL 配置     | `HAL_SRAM_MODULE_ENABLED` 和配置头文件来源        |
| 汇编器不认识 `AREA`/`EXPORT`            | 启动文件       | 错把 ARMASM 文件交给 GNU assembler              |
| 多个 `main`                         | 文件集合       | 排除生成的 main 或 `main1.c/main2.c`            |
| 多个 LCD 初始化函数                      | 文件集合       | `lcd_ex.c` 被包含后又单独编译                      |
| 多个 `SystemInit`/`SystemCoreClock` | CMSIS      | 两份 `system_stm32f1xx.c`                   |
| 多个中断或 MSP 函数                      | 代码所有权      | 原 BSP 与 CubeMX 生成代码同时实现                   |
| `HAL_SRAM_Init` 未定义               | 链接         | 缺少 `stm32f1xx_hal_sram.c`                 |
| `FSMC_NORSRAM_Init` 未定义           | 链接         | 缺少 `stm32f1xx_ll_fsmc.c`                  |
| `printf` 无输出                      | C 运行库      | `_write`/`__io_putchar`、串口初始化、半主机配置       |
| 程序停在断言                            | 参数检查       | FSMC `AddressHoldTime` 等参数不合法             |
| 能进 `main`，延时不对                    | 时钟/SysTick | HSE_VALUE、PLL、`delay_init(72)`、重复 SysTick |
| 串口乱码                              | 时钟/UART    | HSE、PCLK2、波特率、串口参数                        |
| 首次访问 LCD 即 HardFault              | 总线         | FSMC 未启用、芯片/启动配置错误、查看 BFAR/CFSR           |
| LCD ID 为 0 或 FFFF                 | 硬件/时序      | 供电、排线、CS/RD、数据线、读时序                       |
| Debug 正常、Release 异常               | 可移植性       | 未初始化变量、越界、`volatile`、优化相关时序               |
| 中文注释乱码                            | 编码         | 原文件为 GBK/ANSI，IDE 被设为 UTF-8               |

排错时始终从最早失败的一层开始。预处理错误没有解决时，不要研究链接脚本；时钟还没确认时，不要开始调 LCD 初始化寄存器。

---

## 14. 构建产物和调试设置

### 14.1 HEX 输出

原 MDK 工程生成 `Output/atk_f103.hex`。CubeIDE 中可在 MCU Post build outputs 中启用 Convert to Intel Hex，或使用等价的 `arm-none-eabi-objcopy -O ihex` 后处理。

HEX 只是 ELF 的一种烧录表示。日常调试仍应保留 ELF，因为符号、源代码行和调试信息都在 ELF 中。

### 14.2 MAP 文件和尺寸检查

建议启用或查看链接 MAP，并确认：

- 只有一个 `main`、`SystemInit`、`Reset_Handler`；
- `lcd_ex_*` 函数只来自 `lcd.o`；
- Flash/RAM 使用量没有越界；
- 中断入口来自预期文件；
- 没有意外链接另一套 HAL。

启用 `--gc-sections` 通常没有问题，但链接脚本必须对中断向量表使用 `KEEP()`，否则未被普通函数引用的关键段可能被删除。

### 14.3 调试器

使用 ST-LINK/SWD 时，初次移植建议：

- 在 `Reset_Handler`、`main`、`sys_stm32_clock_init`、`HAL_SRAM_Init`、`HardFault_Handler` 设置断点；
- 早期启动故障时尝试 Connect under reset；
- 确认下载地址从 `0x08000000` 开始；
- 观察调用栈、寄存器和反汇编，不只看编辑器当前行。

---

## 15. 建议的最终工程组织思路

第一次保真运行通过后，可以再做结构整理。一个清晰的长期结构可以是：

```text
Core/                    CubeIDE/CubeMX 维护的启动入口和系统文件
App/                     app_init、app_process 等应用逻辑
Drivers/CMSIS/           单一 CMSIS 版本
Drivers/STM32F1xx_HAL_Driver/
Drivers/SYSTEM/          sys、delay、usart 的平台服务
Drivers/BSP/LED/
Drivers/BSP/LCD/
```

整理原则：

- 生成代码与手写代码边界明确；
- 每个 `.c` 正常独立编译，逐步消除“包含 `.c` 文件”的做法；
- HAL/CMSIS 只有一个版本来源；
- 板级引脚集中定义；
- 初始化函数的所有权唯一；
- Debug/Release 构建配置中的宏和路径同步；
- 工程不依赖个人电脑绝对路径。

“先保真移植，再重构”并不意味着永远保留旧结构，而是让每一步都可以验证。若保真版本运行、重构版本不运行，差异范围就很小。

---

## 16. 实验13移植检查清单

### 构建前

- [ ] 已验证原 MDK HEX 在当前硬件上可运行。
- [ ] 目标 MCU 是 STM32F103ZETx，Cortex-M3，无 FPU。
- [ ] 已决定使用原 HAL/CMSIS 还是 CubeIDE HAL/CMSIS，不混用。
- [ ] 已确定源码采用复制还是链接方式。
- [ ] 已记录原程序的 LCD ID、串口输出和 LED 现象。

### 编译配置

- [ ] Debug 和 Release 都定义了 `USE_HAL_DRIVER`、`STM32F103xE`。
- [ ] 已加入 CMSIS、HAL Inc、`Drivers`、`User` 等头文件路径。
- [ ] `stm32f1xx_hal_conf.h` 来自预期目录。
- [ ] 文件编码按 GBK 打开，或已安全转换为 UTF-8。
- [ ] 初次调试使用 `-Og/-O0`，之后再验证 `-O1` 或 Release。

### 文件集合

- [ ] 只编译 `.uvprojx` 中列出的实验源文件。
- [ ] `main1.c`、`main2.c` 已排除。
- [ ] `lcd_ex.c` 未被独立编译。
- [ ] ARMASM 版本 `startup_stm32f103xe.s` 已排除。
- [ ] 只有一个 `main()`、`SystemInit()` 和各中断入口。
- [ ] HAL SRAM 与 LL FSMC 实现均已加入。

### 启动和链接

- [ ] 使用 STM32F103ZETx 对应的 GNU 启动文件。
- [ ] Flash 为 `0x08000000/512K`，RAM 为 `0x20000000/64K`。
- [ ] `.isr_vector` 位于 Flash 起始处且被 `KEEP()`。
- [ ] 未把 FSMC LCD 地址错误地当作普通 RAM 段。
- [ ] 需要时已启用 Intel HEX 输出。

### 编译器适配

- [ ] MDK 半主机代码只在 ARMCC 下编译。
- [ ] GCC 下有且只有一套 `_write()` 或 `__io_putchar()` 路径。
- [ ] FSMC 时序结构体已确定性初始化。
- [ ] `sprintf` 已检查缓冲区大小和格式参数类型。

### 运行验证

- [ ] 能从 Reset_Handler 进入 main。
- [ ] SYSCLK=72 MHz，APB1=36 MHz，APB2=72 MHz。
- [ ] LED0 和 1 秒延时正确。
- [ ] USART1 以 115200 输出正常。
- [ ] FSMC 地址访问不产生 BusFault。
- [ ] LCD ID 稳定且属于驱动支持范围。
- [ ] LCD 背景色、字符串和 LED 行为符合原实验。
- [ ] 冷启动、复位、Debug、Release 均做过验证。

---

## 17. 可复用于其他 MDK 例程的移植模板

迁移其他实验时，可按以下表格先做静态分析：

| 分类  | 要从 MDK 提取的内容                  | 在 CubeIDE 中的对应物                    |
| --- | ----------------------------- | ---------------------------------- |
| 芯片  | Device、CPU、FPU、容量             | MCU Settings、GNU 启动文件、链接脚本         |
| 宏   | Define/Undefine               | MCU GCC Compiler > Preprocessor    |
| 路径  | IncludePath                   | Include paths                      |
| 文件  | Groups/Files、单文件排除            | Source Location、Exclude from Build |
| 编译  | C 标准、优化、警告、ABI                | MCU GCC Compiler 各选项               |
| 启动  | ARMASM/IAR/GNU 版本             | 使用 GNU 对应版本                        |
| 链接  | Scatter、ROM/RAM、库             | `.ld`、libraries、linker flags       |
| 运行库 | MicroLIB、semihosting、retarget | newlib/newlib-nano、syscalls        |
| 中断  | 向量表和 Handler 实现位置             | GNU startup + 唯一 ISR 实现            |
| 外设  | 时钟、引脚、DMA、中断、外部器件             | 原驱动或 CubeMX 生成代码，二选一               |
| 验收  | 原工程可观察现象                      | 分阶段硬件测试和回归标准                       |

每次移植都应回答四个问题：

1. **原工程究竟编译了什么？**
2. **哪些内容属于编译器/IDE，不能直接复制？**
3. **哪些硬件初始化已经由原代码完成，不能再生成一套？**
4. **怎样用最小可观察现象证明每一层已经正确？**

能够回答这四个问题，移植就从“碰运气改报错”变成了可重复的工程过程。

---

## 参考

- 原工程配置：`实验13 TFTLCD（MCU屏）实验/Projects/MDK-ARM/atk_f103.uvprojx`
- 原实验说明：`实验13 TFTLCD（MCU屏）实验/readme.txt`
- ST 官方《STM32CubeIDE user guide（UM2609）》：<https://www.st.com/resource/en/user_manual/um2609-stm32cubeide-user-manual-stmicroelectronics.pdf>
