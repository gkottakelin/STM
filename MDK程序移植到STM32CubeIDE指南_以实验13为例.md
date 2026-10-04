# MDK 程序移植到 STM32CubeIDE 指南——以实验13为例

适用于已有 STM32CubeIDE 目标工程、需要迁入 MDK-ARM（Keil）程序的情况。本文以实验13为例，按“保留原 HAL/CMSIS 和驱动、复制源码、适配 GNU 工具链”的路线完成首次移植。

操作顺序：**确认原程序可运行 → 准备文件 → 配置工程 → 修改编译器相关代码 → 编译下载 → 分阶段验证**。首次迁移先保持原驱动和目录结构，运行通过后再考虑升级 HAL 或重构。

## 1. 适用范围与准备条件

| 项目 | 本文使用的配置 |
| --- | --- |
| 原工程 | 实验13 TFTLCD（MCU屏）实验，`Projects/MDK-ARM/atk_f103.uvprojx` |
| MCU | STM32F103ZE，CubeIDE 中对应 STM32F103ZETx；Cortex-M3，无 FPU |
| 内存 | Flash：`0x08000000`，512 KiB；SRAM：`0x20000000`，64 KiB |
| 原编译器 | ARM Compiler 5.06 update 7，C99，MDK Level 1 优化 |
| 时钟 | 板载 8 MHz HSE，经 PLL × 9 得到 72 MHz |
| LCD | FSMC Bank1 NE4，16 位 8080 MCU 并口屏，A10 作为 RS；不适用于 RGB 屏 |

其他 STM32 型号需重新核对芯片容量、启动文件、HAL 接口、FSMC/FMC 功能和引脚映射，本文配置不能直接保证适用。

开始前确认原 MDK 工程可编译，并确认原 `Output/atk_f103.hex` 能在当前板卡和屏幕上运行。记录 LCD 型号及 ID、串口输出、LED 周期和接线，作为移植后的对照；若原程序也不能运行，先排查硬件和原工程。

## 2. 准备与筛选文件

### 2.1 复制源码并确定库来源

原工程位于本指南同目录下的：

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

将所需源码和头文件复制到目标工程，保持原 `Drivers/`、`User/` 的相对结构。目标工程采用实验13自带的 CMSIS、HAL、SYSTEM、BSP 和 `User/stm32f1xx_hal_conf.h`；CubeIDE 原有的同类实现不再参与构建或头文件搜索。

**整个工程只使用一套 HAL/CMSIS。** 不要直接把旧库覆盖到新库上形成混合目录。保留目标芯片对应的 GNU 启动文件和 `.ld` 链接脚本，并按第3章核对。需要共享原文件时，参见附录 A。

### 2.2 加入实际参与构建的源文件

以 `.uvprojx` 中的文件清单为准。实验13实际参与构建的 C 文件如下；不要把 HAL 目录中的模板、Legacy 或其他未使用实现一并加入。

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

同时保留这些源文件所需的头文件，尤其是 `User/stm32f1xx_hal_conf.h`、`User/stm32f1xx_it.h` 和 `Drivers/BSP/LCD/lcdfont.h`。`lcd_ex.c` 也要保留在 LCD 目录中，但按下一节排除其独立编译。

### 2.3 排除特殊文件与重复实现

对 Debug、Release 等所有使用的构建配置，核对以下文件的 **Exclude from Build** 状态：

| 文件 | 处理方式与原因 |
| --- | --- |
| `User/main1.c`、`User/main2.c` | 不参与构建；它们不是当前实验入口，且引用了未纳入本实验构建的 KEY 驱动。 |
| `Drivers/BSP/LCD/lcd_ex.c` | 保留文件但不单独编译；`lcd.c` 已通过 `#include "./BSP/LCD/lcd_ex.c"` 包含其实现。 |
| 原 `Templates/arm/startup_stm32f103xe.s` | 不参与构建；它使用 ARMASM 语法及 ARM C 库入口，需改用 GNU 启动文件。 |
| CubeIDE 生成的 `Core/Src/main.c` | 本文采用原 `User/main.c`，因此排除生成的入口。 |
| 重复的 HAL、`system_stm32f1xx.c`、中断及 MSP 实现 | 每个符号保留一套实现；中断和回调的原文件位置见第4.1节。 |

## 3. 配置 CubeIDE 工程

编译选项通常在 `Project Properties → C/C++ Build → Settings → Tool Settings` 中设置。不同版本菜单名称可能略有差异，以下配置需覆盖 Debug、Release 等实际使用的构建配置。

### 3.1 芯片、全局宏与头文件路径

确认 MCU 为 `STM32F103ZETx`，CPU 为 Cortex-M3，指令集为 Thumb，FPU 为 None，使用 CubeIDE 随附的 GNU Tools for STM32；不要启用 hard 浮点 ABI。

在 MCU GCC Compiler 的 Preprocessor 中加入：

```text
USE_HAL_DRIVER
STM32F103xE
```

前者启用 HAL 入口，后者选择正确的设备寄存器和中断定义。直接设置工程宏，不修改 `stm32f1xx.h`。

在 C Compiler 的 Include paths 中加入以下路径，按目标工程实际位置填写相对路径或项目变量：

```text
Drivers/CMSIS/Device/ST/STM32F1xx/Include
Drivers/STM32F1xx_HAL_Driver/Inc
Drivers/CMSIS/Include
Drivers
User
Middlewares
```

`Drivers` 根路径必须保留，因为源码使用 `#include "./SYSTEM/sys/sys.h"`、`#include "./BSP/LCD/lcd.h"`。避免写死个人电脑路径。若 GNU 启动文件还包含设备头文件，再为 Assembler 添加相应 CMSIS 路径。

### 3.2 HAL 配置与时钟常量

确认实际被包含的是原 `User/stm32f1xx_hal_conf.h`，其中与本实验相关的配置包括：

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

启用模块宏不会自动把实现文件加入构建；对应源文件仍按第2.2节添加。原配置还启用了其他模块，无需因此把所有 HAL 源文件加入构建。

确认 `HSE_VALUE` 为 `8000000U`，与板载晶振一致，否则时钟和串口波特率的计算可能错误。

### 3.3 GNU 启动文件与链接脚本

只保留目标 STM32F103ZETx 对应的 GNU 启动文件，常见名称为 `startup_stm32f103zetx.s`，以目标工程实际文件为准。其向量表应匹配芯片，复位流程应调用 `SystemInit()`、完成 `.data` 复制、`.bss` 清零和 C/C++ 运行库初始化，再进入 `main()`。

链接脚本的内存定义应等价于：

```ld
MEMORY
{
  RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 64K
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}
```

检查 `.isr_vector` 位于 Flash 起始处并使用 `KEEP()`；`.text/.rodata` 位于 Flash，`.data` 在 Flash 中保存初值、运行于 RAM，`.bss` 位于 RAM。

原 MDK 启动文件预留栈 `0x400` 字节、堆 `0x200` 字节，可对照 `_Min_Stack_Size`、`_Min_Heap_Size` 检查，实际大小按调用深度和内存使用调整，RAM 总量不能超过 64 KiB。本实验无 Bootloader，向量表不额外偏移。

LCD 的 FSMC 地址属于外设总线窗口，**不要将其添加为链接脚本的普通 RAM 段**。地址与总线配置见第5.2节。

### 3.4 编译、编码与输出

| 项目 | 设置 |
| --- | --- |
| C 标准 | 原工程使用 C99，可选 `gnu99`、`gnu11` 或兼容的 GNU C 方言。 |
| 优化 | 初次调试用 `-Og` 或 `-O0`；通过后再验证 `-O1` 或 Release 配置。 |
| 警告 | 初次迁移先逐项处理警告，不必立即全部视为错误，也不要全局关闭警告。 |
| 编码 | 原主要源码为 GBK/本地 ANSI 风格。设置为 GBK，或备份后统一转换为 UTF-8；不要在乱码状态下保存。中文字符串的字节内容也受编码影响。 |
| HEX（按需） | 在 MCU Post build outputs 中启用 Convert to Intel Hex；等价命令为 `arm-none-eabi-objcopy -O ihex`。 |
| ELF/MAP | 保留 ELF 用于带符号调试；查看 MAP，确认实际链接的实现及 Flash/RAM 占用。使用 `--gc-sections` 时仍需保留向量表的 `KEEP()`。 |

## 4. 调整入口与编译器相关代码

### 4.1 保留唯一的入口和初始化路径

本文保留原 `User/main.c`，初始化顺序如下：

```c
HAL_Init();
sys_stm32_clock_init(RCC_PLL_MUL9);  /* HSE 8 MHz -> SYSCLK 72 MHz */
delay_init(72);
usart_init(115200);
led_init();
lcd_init();
```

延时系数依赖主频，LCD 初始化依赖延时，串口波特率依赖外设时钟。不要再叠加调用生成的 `SystemClock_Config()`、`MX_USART1_UART_Init()`、`MX_FSMC_Init()`；生成的 GPIO 初始化也不能在之后覆盖驱动设置。

按符号检查重复实现，原代码中的归属为：

| 符号 | 原实现位置 |
| --- | --- |
| `main()` | `User/main.c` |
| `SystemInit()`、`SystemCoreClock` | `system_stm32f1xx.c` |
| `SysTick_Handler()` | `User/stm32f1xx_it.c` |
| `USART1_IRQHandler()`、`HAL_UART_MspInit()` | `Drivers/SYSTEM/usart/usart.c` |
| `HAL_SRAM_MspInit()` | `Drivers/BSP/LCD/lcd.c` |
| `HAL_Delay()` | `Drivers/SYSTEM/delay/delay.c`，覆盖 HAL 弱实现 |

如果生成的 `stm32f1xx_hal_msp.c`、`stm32f1xx_it.c` 也包含同名强实现，应排除重复实现或合并所需逻辑。继续使用 `.ioc` 生成代码前，按附录 B 整理代码边界，避免重新引入重复入口或初始化。

`sys.c` 中的 `__ASM volatile(...)`、`__set_MSP()` 已由 CMSIS 按编译器适配，使用正确 CMSIS 时通常可以保留。

### 4.2 适配 GCC 的 `printf` 输出

修改 `Drivers/SYSTEM/usart/usart.c` 的重定向部分。原 `fputc()` 和半主机处理面向 ARMCC；GCC/newlib 通常通过 `_write()` 输出，CubeIDE 的 `syscalls.c` 可能进一步调用 `__io_putchar()`。按实际调用链选择以下一种接法。

**方案 A：已有 `syscalls.c`，且其中 `_write()` 调用 `__io_putchar()`。** 保留该 `_write()`，在串口源文件中提供唯一的强实现：

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

**方案 B：没有可用的 `syscalls.c` 输出路径。** 在串口源文件中实现 `_write()`，并确认构建中没有另一套强实现：

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

这些代码使用原串口模块中的 `USART_UX` 定义。将原 ARMCC 重定向代码和选定的 GCC 实现放在明确的条件分支中：

```c
#if defined(__CC_ARM) || defined(__ARMCC_VERSION)
/* 原 MDK/ARMCC 重定向代码 */
#elif defined(__GNUC__)
/* GCC 重定向代码 */
#else
#error Unsupported compiler
#endif
```

原 `#if __ARMCC_VERSION ...` 在 GCC 中可能因宏未定义而落入 AC5 分支；应隔离整段 ARMCC 专用代码，包括 `#pragma import(__use_no_semihosting)`、`__ARM_use_no_argv`、`_ttywrch()`、`_sys_exit()`、`_sys_command_string()` 等。

每个输出函数只保留一套强实现；使用 `--specs=nosys.specs` 时仍可提供自己的系统调用。先完成 USART 初始化再打印。本实验使用整数和字符串，不需要开启 `printf` 浮点格式化。

### 4.3 完整初始化 FSMC 时序结构体

修改 `Drivers/BSP/LCD/lcd.c` 中的 `lcd_init()`。原时序结构体未给 `BusTurnAroundDuration`、`CLKDivision`、`DataLatency` 赋值，而 HAL/LL 会读取这些成员；编译器或优化级别变化后可能暴露未初始化问题。

将两个时序结构体的声明和赋值整理为：

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

原 `AddressHoldTime=0` 不符合该 HAL 的 1～15 参数约束，示例改为 1，避免启用 `USE_FULL_ASSERT` 后停在断言中。异步模式下 `CLKDivision`、`DataLatency` 不参与实际异步时序，仍应赋予 HAL 接受的确定值。

首次验证保留上面的读写时序，并启用 Extended Mode。若接线、主频正确后仍有不稳定，再逐项调整 `DataSetupTime`，不要同时改变 Bank、总线宽度和地址线。

### 4.4 建议限制格式化输出长度

原 `User/main.c` 使用 `sprintf()` 生成 LCD ID 字符串，12 字节缓冲区刚好容纳当前内容。可改成下面的写法，明确参数类型并限制写入长度：

```c
snprintf((char *)lcd_id,
         sizeof(lcd_id),
         "LCD ID:%04X",
         (unsigned int)lcddev.id);
```

## 5. 接线、下载与运行验证

### 5.1 核对硬件接线

基础外设：

| 功能 | 引脚/参数 |
| --- | --- |
| LED0 / LED1 | PB5 / PE5，主程序主要使用 LED0 |
| USART1 TX / RX | PA9 / PA10，确认与板载 USB 转串口跳线连接 |
| 串口参数 | 115200 bit/s，8N1 |

LCD 控制信号：

| LCD 信号 | FSMC/MCU 引脚 |
| ------ | ----------- |
| CS     | NE4 / PG12  |
| RS     | A10 / PG0   |
| WR     | NWE / PD5   |
| RD     | NOE / PD4   |
| BL     | PB0         |

LCD 16 位数据总线：

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

### 5.2 核对 FSMC 配置与地址

原驱动负责 FSMC 初始化，其配置应保持为：

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

读写时序见第4.3节。NE4 和 A10 对应原 `lcd.h` 中的访问地址：

```text
LCD_REG = 0x6C0007FE
LCD_RAM = 0x6C000800
```

访问前必须开启 FSMC 和相关 GPIO 时钟并完成 `HAL_SRAM_Init()`。避免后续 GPIO 初始化把 FSMC 引脚改回普通 GPIO。

### 5.3 按阶段验证

先完成编译和链接，再用 ELF 下载调试。使用 ST-LINK/SWD 时确认下载地址从 `0x08000000` 开始；可在 `Reset_Handler`、`main`、`sys_stm32_clock_init`、`HAL_SRAM_Init` 和 `HardFault_Handler` 设置断点。

1. **启动与时钟**：确认能进入 `main()`，且不会反复复位或进入 HardFault。时钟初始化后检查 `SystemCoreClock=72 MHz`，RCC 的 SYSCLK 来源为 PLL，AHB=72 MHz、APB1=36 MHz、APB2=72 MHz。IDE 中的 CPU Frequency 设置不能代替 RCC 初始化。
2. **LED 与延时**：先验证 LED0 翻转和 `delay_ms(1000)` 的量级，确认 SysTick 运行正常。周期不对时先查时钟和 `delay_init(72)`。
3. **串口**：在 `lcd_init()` 前临时打印固定 ASCII 文本，确认 GCC 输出路径生效；若使用串口中断，检查 `USART1_IRQHandler()` 调用 `HAL_UART_IRQHandler()`。
4. **FSMC 访问**：进入 `lcd_init()` 后检查 FSMC、GPIOD、GPIOE、GPIOG、GPIOB 时钟，确认 `HAL_SRAM_Init()` 返回 `HAL_OK`，LCD 地址访问不触发 BusFault；需要时观察 NE4、A10、NWE、NOE 波形。
5. **LCD 显示**：确认 ID 稳定且被当前驱动支持，例如 `0x9341`、`0x7789`、`0x5310`、`0x7796`、`0x5510`、`0x9806`、`0x1963`，随后恢复完整主循环。异常现象按第6章排查。

### 5.4 最终验收

- [ ] LCD 按顺序循环显示 12 种背景色，固定字符串位置和颜色正确。
- [ ] LCD 显示的 ID 与复位后串口输出一致。
- [ ] LED0 约每秒翻转一次。
- [ ] 冷启动、复位、重新下载后均能正常运行。
- [ ] Debug 和 Release 均已编译并完成运行验证。

以上是待执行的验收项目，编译通过不能代替上板验证。

## 6. 常见问题速查

从最早失败的阶段开始排查：先解决编译和链接，再确认时钟，最后排查外设。

| 现象 | 优先检查 |
| --- | --- |
| 找不到 `stm32f1xx.h` | CMSIS Device Include 路径。 |
| 找不到 `./BSP/LCD/lcd.h` | 是否加入 `Drivers` 根路径。 |
| 提示未选择 STM32F1 器件 | `STM32F103xE` 宏及其生效的构建配置。 |
| `SRAM_HandleTypeDef` 未知 | `HAL_SRAM_MODULE_ENABLED`、`stm32f1xx_hal_sram.h` 及配置头文件来源。 |
| 汇编器不认识 `AREA`、`EXPORT` | 错将 ARMASM 启动文件交给 GNU assembler。 |
| 多个 `main` / LCD 初始化函数 | 生成的 `main.c`、`main1.c/main2.c` 是否排除；`lcd_ex.c` 是否被重复编译。 |
| 多个 `SystemInit`、`SystemCoreClock`、`Reset_Handler` | 重复的系统源文件或启动文件；结合 MAP 确认实际来源。 |
| 多个中断或 MSP 函数 | 原驱动与 CubeMX 生成代码同时提供强实现，按第4.1节处理。 |
| `HAL_SRAM_Init` / `FSMC_NORSRAM_Init` 未定义 | 缺少 `stm32f1xx_hal_sram.c` / `stm32f1xx_ll_fsmc.c`。 |
| `HAL_UART_Init` 或 GPIO/RCC HAL 符号未定义 | 对应模块宏和 `.c` 文件是否同时具备。 |
| 进不了 `main`，停在复位或 HardFault | 启动文件、链接布局和向量表；早期调试连接失败时尝试 Connect under reset。 |
| `printf` 无输出 | `_write()` / `__io_putchar()` 调用链、串口初始化、是否误用半主机。 |
| 串口乱码或延时不对 | `HSE_VALUE`、PLL、PCLK2、波特率、`delay_init(72)` 和 SysTick 实现。 |
| 停在 FSMC 参数断言 | `AddressHoldTime` 等字段是否合法，两个时序结构体是否完整初始化。 |
| 首次访问 LCD 即 HardFault | FSMC 使能、芯片和启动配置；读取 `SCB->CFSR`、`SCB->HFSR`、`SCB->BFAR` 定位故障。 |
| LCD ID 总是 `0x0000` | 屏供电、数据线被拉低、读控制或片选未工作。 |
| LCD ID 总是 `0xFFFF` | 屏未连接、总线悬空、片选或 RD 未工作。 |
| LCD ID 每次不同 | 读时序、电源、接触不稳或数据线冲突。 |
| ID 正确但白屏 / 花屏 | 背光、屏型及初始化序列、写时序、数据位映射和供电。 |
| Debug 正常、Release 异常 | 未初始化变量、越界、`volatile` 和依赖优化的时序。 |
| 中文注释乱码 | 原文件 GBK/ANSI 与 IDE 编码设置不一致。 |

4.3/7 寸等较大屏幕还需留意供电能力；供电不足可能造成复位、白屏或初始化失败。

## 附录 A：链接原文件或升级 HAL

需要多个工程共用源码时，可使用 Linked Resource。定义 `EXP13_ROOT` 等路径变量，避免绝对路径；修改链接文件会同时修改原例程。链接整个目录后，仍需按第2章核对 Source Location 的 exclusion pattern 或 Exclude from Build。

若必须使用目标工程的新 CMSIS/HAL，则只迁入 SYSTEM、BSP 和应用代码，并逐项适配 API、配置头文件与初始化参数。升级 HAL 建议在原库版本运行通过后单独进行，仍保持库来源唯一。

## 附录 B：移植成功后继续使用 CubeMX

需要长期由 `.ioc` 维护工程时，再按以下方式整理：

1. 保留生成的 `main.c`，将实验逻辑迁到 `app.c/app.h`，例如 `app_init()`、`app_process()`；排除旧入口。
2. 只在 `USER CODE BEGIN/END` 区域调用应用接口。
3. 为时钟、USART、FSMC 逐项确定唯一初始化来源；若改用 `MX_FSMC_Init()`，逐字段核对 Bank、总线宽度、扩展模式及读写时序。
4. 将 `lcd_ex.c` 改成独立编译单元时，同时移除对它的直接包含，并补齐头文件声明；板级引脚集中定义。
5. 保持 HAL/CMSIS 来源唯一，路径不依赖个人电脑，Debug/Release 配置同步。

可采用以下目录结构：

```text
Core/                    CubeIDE/CubeMX 维护的启动入口和系统文件
App/                     app_init、app_process 等应用逻辑
Drivers/CMSIS/           单一 CMSIS 版本
Drivers/STM32F1xx_HAL_Driver/
Drivers/SYSTEM/          sys、delay、usart 的平台服务
Drivers/BSP/LED/
Drivers/BSP/LCD/
```

每次整理后重新执行第5章验收，便于定位由结构调整引入的问题。

## 附录 C：其他场景的注意事项

- **短枚举与 ABI**：原 MDK 工程启用了短枚举相关选项，本实验通常无需机械加入 `-fshort-enums`；只有预编译库、通信结构或二进制格式依赖枚举尺寸时，才需核对一致性。
- **Bootloader**：应用放到 Bootloader 后方时，Flash ORIGIN、可用 LENGTH、下载地址与 `SCB->VTOR` 必须配套修改。
- **中断或多任务打印**：第4.2节示例为阻塞输出，用于这些场景前还需考虑阻塞时间与重入。

## 附录 D：迁移其他 MDK 例程时的信息对照

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

## 参考

- 原工程配置：`2，标准例程-HAL库版本/实验13 TFTLCD（MCU屏）实验/Projects/MDK-ARM/atk_f103.uvprojx`
- 原实验说明：`2，标准例程-HAL库版本/实验13 TFTLCD（MCU屏）实验/readme.txt`
- ST 官方：[STM32CubeIDE user guide（UM2609）](https://www.st.com/resource/en/user_manual/um2609-stm32cubeide-user-manual-stmicroelectronics.pdf)
