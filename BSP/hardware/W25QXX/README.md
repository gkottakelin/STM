# W25Qxx SPI Flash 模块

## 1. 快速接入

1. 复制 `W25QXX` 目录到目标工程，添加该目录到 Include paths，让 `w25qxx.c` 参与编译。
2. 在 CubeMX 配置 SPI，按下表修改 `w25qxx.h` 用户配置区。
3. 在 `MX_SPI2_Init()` 后调用 `w25qxx_init()`，检查返回状态和读取到的 JEDEC ID。
4. 先用读取功能验证连接；写入前明确选择允许擦除的测试扇区。

模块仅初始化 CS，不调用 `HAL_SPI_Init()`；通过 `extern SPI_HandleTypeDef W25QXX_SPI_HANDLE` 引用生成的全局句柄。依赖 `main.h`、HAL SPI/GPIO/RCC 和 HAL 毫秒时基。

## 2. 用户配置与引脚

| 宏 | 默认值 | 调整方法 |
| --- | --- | --- |
| `W25QXX_SPI_HANDLE` | `hspi2` | 与 CubeMX 生成的句柄一致 |
| `W25QXX_CS_GPIO_PORT / PIN` | `GPIOB / GPIO_PIN_12` | 修改为实际 CS 引脚 |
| `W25QXX_CS_GPIO_CLK_ENABLE()` | GPIOB 时钟 | 与 CS 端口一致 |
| `W25QXX_SPI_TIMEOUT_MS` | `100` ms | 每次 HAL SPI 调用超时 |
| `W25QXX_PROGRAM_TIMEOUT_MS` | `100` ms | 每页编程的忙等待上限 |
| `W25QXX_ERASE_TIMEOUT_MS` | `5000` ms | 扇区擦除或操作前忙等待上限 |

| Flash 信号 | F103ZE 默认引脚 | 模式 / 电平 |
| --- | --- | --- |
| CS# | PB12 | 普通推挽输出，低有效，由模块初始化 |
| CLK | PB13 / SPI2_SCK | 复用推挽 |
| DO / MISO | PB14 / SPI2_MISO | 输入 |
| DI / MOSI | PB15 / SPI2_MOSI | 复用推挽 |
| WP#、HOLD# | 按模块电路保持无效高电平 | 若已有上拉，按板卡原理图连接 |
| VCC、GND | 按具体 Flash 电压要求供电并共地 | 核对器件电压后连接，勿混用不同电压型号 |

## 3. CubeMX 配置

STM32F103ZE 示例：SPI2，Full-Duplex Master，8-bit，MSB first，CPOL Low、CPHA 1 Edge（Mode 0），NSS Software，TI mode Disable，CRC Disable，DMA/中断均无需启用。

PCLK1=36 MHz 时，分频 8 得到 SCK=4.5 MHz，可作为初次验证设置。SPI 的 GPIO/时钟由 CubeMX 配置，PB12 保留给本模块。初始化顺序为 HAL → 系统时钟 → GPIO → SPI2 → `w25qxx_init()`。

## 4. 最小使用示例

以下函数放在 `main.c` 用户代码区，在 `MX_SPI2_Init()` 后调用一次 `app_flash_test()`。**示例会擦除检测到的 Flash 最后一个 4 KiB 扇区**，运行前确认其中没有需要保留的数据。

```c
#include "w25qxx.h"
#include <string.h>

volatile uint32_t flash_jedec_id;

void app_flash_test(void)
{
    uint32_t id;
    uint32_t address;
    const uint8_t tx[] = "W25Q test";
    uint8_t rx[sizeof(tx)];
    if (w25qxx_init(&id) != HAL_OK) { Error_Handler(); return; }
    flash_jedec_id = id;
    address = w25qxx_capacity() - W25QXX_SECTOR_SIZE;
    if (w25qxx_erase_sector(address) != HAL_OK ||
        w25qxx_write(address, tx, sizeof(tx)) != HAL_OK ||
        w25qxx_read(address, rx, sizeof(rx)) != HAL_OK ||
        memcmp(tx, rx, sizeof(tx)) != 0) {
        Error_Handler();
    }
}
```

## 5. 接口与擦写规则

| 接口 | 作用 / 单位 |
| --- | --- |
| `w25qxx_init(&id)` | CS 初始化、退出掉电、读取 24-bit JEDEC ID；参数可为 NULL；不擦除 |
| `w25qxx_capacity()` | 返回字节容量；初始化失败为 0 |
| `w25qxx_read(address, data, length)` | 按字节读取，地址和长度必须在容量内 |
| `w25qxx_write(address, data, length)` | 自动跨 256-byte 页编程并回读比较，不自动擦除 |
| `w25qxx_erase_sector(address)` | 擦除并校验一个 4096-byte 扇区；参数为对齐的字节地址，不是扇区编号 |

除容量查询外均返回 HAL 状态：`HAL_OK` 成功，`HAL_ERROR` 包括参数/型号/校验错误，`HAL_TIMEOUT` 表示总线或忙等待超时，`HAL_BUSY` 为 HAL 总线忙。缓冲区不得为 NULL，读写长度不得为 0。

Flash 编程只能将 1 改为 0，修改已有数据通常需先擦除整个扇区。若要保留同扇区其他数据，应由应用先读出、合并、擦除再重写。掉电或中途失败可能留下部分写入，不提供原子参数存储、磨损均衡或文件系统。

所有操作阻塞、不可重入，使用约 256 字节临时校验缓冲区；不在中断中调用。同一 SPI 总线的其他设备由应用串行调度，其他 CS 保持高。超时按一次 HAL 调用/一次页或扇区操作计算，不是整次跨页写入的全局时限。错误后 CS 会释放；超时后 Flash 可能仍忙，不要立即断电或重复写入。

## 6. 支持范围、来源与验证

基于“实验24 SPI实验”的 NORFLASH 命令与页/扇区处理方式整理，保留原版权声明。已移除原 SPI2、delay、usart BSP 依赖，增加范围检查、写使能检查、有限超时和回读校验。

当前限定：W25Q80/16/32/64/128，JEDEC 厂商 `EF`、类型 `40` 或 `60`、容量码 `14..18`，标准 SPI、24-bit 地址模式。仅凭 ID 不保证所有后缀、电压、保护配置一致，需核对实物。要求上电后处于标准 SPI/3-byte 模式；不处理 QPI/4-byte 模式恢复、不自动解除写保护。**不支持 W25Q256、其他厂家兼容器件和 Quad-SPI**，这些虽出现在原驱动中，但不属于此版本范围。

默认 MCU 为 STM32F103ZE HAL；其他系列需重新编译及验证。实物验收：ID/容量正确，跨页写入回读一致，擦除后全为 FF，保护或拔线时可返回错误/超时。构建与软件测试记录见[BSP 总索引](../../README.md)。
