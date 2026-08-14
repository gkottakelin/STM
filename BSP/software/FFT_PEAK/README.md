# FFT 谱峰处理模块

## 1. 快速接入

本模块从 `EDC0801_1/Core/Src/fft_process.c` 提取，输入为已经完成 FFT 并取模后的 `uint16_t` 单边幅度谱，负责：

- 对连续频谱帧执行 1/8 权重指数平均，以稳定峰值检测。
- 使用三点合并幅度和 CFAR 阈值检测局部谱峰。
- 合并距离过近的峰，并使用相邻谱线比例插值频率。
- 根据原工程标定表把原始幅值换算为 mV。
- 输出有效谱峰、RMS、合成 Vpp 和分量比例。

本模块不执行时域到频域的 FFT 变换。调用者必须提供至少 `FFT_PEAK_POINT_COUNT/2` 个点的幅度谱。

接入步骤：

1. 复制整个 `FFT_PEAK` 目录到目标工程。
2. 把该目录加入头文件搜索路径，并让 `fft_peak.c` 参与编译。
3. 按采样系统修改 `fft_peak.h` 顶部的用户配置区。
4. 确保链接数学库；STM32CubeIDE 通常已经通过工具链配置提供 `sqrtf/sinf/cosf/fabsf`。
5. 对每帧幅度谱调用 `fft_peak_process()`。

## 2. 需要修改的配置

| 宏 | EDC0801_1 示例值 | 作用 | 如何确定目标值 |
| --- | --- | --- | --- |
| `FFT_PEAK_POINT_COUNT` | `4096U` | 原始 FFT 点数 | 与 FFT 变换长度一致 |
| `FFT_PEAK_SAMPLE_RATE_HZ` | `2000000.0f` | 采样率，单位 Hz | 使用实际 ADC/采集时钟 |
| `FFT_PEAK_MAX_COUNT` | `100U` | 最多保存的有效峰数 | 按 RAM 和业务需求调整 |
| `FFT_PEAK_IGNORED_BIN_COUNT` | `16U` | 找峰时忽略的低频 bin | 按直流泄漏范围调整 |
| `FFT_PEAK_PRESET_BIN_COUNT` | `18U` | 处理前覆盖的低频点数 | 通常略大于忽略范围 |
| `FFT_PEAK_EMA_SHIFT` | `3U` | EMA 新数据权重为 `1/2^n` | 数值越大越稳定、响应越慢 |
| `FFT_PEAK_CFAR_REFERENCE_CELLS` | `16U` | 单侧参考单元数 | 按噪声变化速度调整 |
| `FFT_PEAK_CFAR_GUARD_CELLS` | `3U` | 峰值两侧保护单元数 | 按主瓣宽度调整 |
| `FFT_PEAK_CFAR_THRESHOLD_MULTIPLIER` | `3U` | CFAR 噪声阈值倍数 | 按误报和漏报折中调整 |
| `FFT_PEAK_MIN_DISTANCE_BINS` | `19U` | 两个峰的最小 bin 间距 | 按频率分辨率和主瓣宽度调整 |
| `FFT_PEAK_MIN_AMPLITUDE_MV` | `3.0f` | 标定后最小峰值 | 按最低有效信号调整 |
| `FFT_PEAK_MAX_FREQUENCY_HZ` | `600000.0f` | 最高保留频率 | 不得超过奈奎斯特频率 |
| `FFT_PEAK_VPP_OVERSAMPLE_FACTOR` | `16U` | Vpp 合成过采样倍数 | 越大越准确但计算越慢 |

当前频率分辨率由以下公式自动计算：

```text
FFT_PEAK_FREQUENCY_RESOLUTION_HZ = FFT_PEAK_SAMPLE_RATE_HZ / FFT_PEAK_POINT_COUNT
```

EDC0801_1 当前为 `2 MHz / 4096 = 488.28125 Hz/bin`。

## 3. 输入与输出

输入不是 ADC 时域采样，而是 FFT 模值：

```c
uint16_t magnitude[FFT_PEAK_POINT_COUNT / 2U];
fft_peak_result_t result;

fft_peak_process(magnitude, &result);
```

`fft_peak_process()` 会修改输入数组的前 `FFT_PEAK_PRESET_BIN_COUNT` 个点。需要保留原始数据时，应传入副本。

主要输出：

| 字段 | 作用 |
| --- | --- |
| `fundamental_freq` | 最低频有效峰的插值频率，单位 Hz |
| `fundamental_amp` | 最低频有效峰的标定峰值，单位 mV |
| `harmonic_freq[]` | 全部有效峰，按频率升序 |
| `harmonic_amp[]` | 全部有效峰的标定峰值，单位 mV |
| `harmonic_num` | 有效峰数量 |
| `rms` | 所有有效分量合成 RMS，单位 mV |
| `vpp` | 假定各分量零初相位时的合成峰峰值，单位 mV |
| `thd` | 除首峰外其他有效峰相对首峰的比例，单位 % |

## 4. 标定数据

模块包含两个从原工程复制的必要数据文件：

- `fft_amplitude_calibration.h`：10 kHz 到 500 kHz、500 Hz 间隔的第一阶段幅值标定表。
- `fft_secondary_amplitude_calibration.h`：10 kHz 到 500 kHz、5 kHz 间隔的第二阶段修正表。

这些表对应 EDC0801_1 的原采集链路和 250 mVpp 标定源。换 ADC、模拟前端、增益、参考电压或采样链路后，频率检测仍可使用，但 mV 幅值、RMS 和 Vpp 不再保证准确，应重新生成标定表。

可在运行时使用：

```c
fft_peak_set_amplitude_scale(scale_factor);
```

修正同型号设备之间的整体增益偏差。

## 5. 最小使用示例

```c
#include "fft_peak.h"

static uint16_t magnitude[FFT_PEAK_POINT_COUNT / 2U];
static fft_peak_result_t signal;

void process_one_spectrum(void)
{
    /* 在这里通过 CMSIS-DSP 或采集端得到 magnitude[]。 */
    fft_peak_process(magnitude, &signal);

    if (signal.harmonic_num > 0U)
    {
        float first_frequency_hz = signal.fundamental_freq;
        float first_peak_mv = signal.fundamental_amp;
        (void)first_frequency_hz;
        (void)first_peak_mv;
    }
}
```

开始一批互不相关的新测量前，清除上一批 EMA 状态：

```c
fft_peak_reset_average();
```

## 6. 公开接口

| 接口 | 作用 |
| --- | --- |
| `fft_peak_process()` | 完成 EMA、CFAR 找峰、频率插值和测量计算 |
| `fft_peak_reset_average()` | 清除跨帧指数平均状态 |
| `fft_peak_find()` | 对单帧频谱直接查找全部有效峰 |
| `fft_peak_interpolate()` | 对指定整数峰进行频率和幅值插值 |
| `fft_peak_find_max_bin()` | 查找忽略直流后的最大谱线 |
| `fft_peak_calculate_rms()` | 根据有效分量峰值计算 RMS |
| `fft_peak_calculate_vpp()` | 通过过采样合成波形估算 Vpp |
| `fft_peak_calculate_thd()` | 计算其余分量相对首峰的幅值比例 |
| `fft_peak_set_amplitude_scale()` | 设置运行时整体增益修正 |

## 7. 资源、限制与验证

- 模块只依赖标准整数类型和单精度数学函数，不依赖 STM32 HAL。
- 默认静态 EMA 缓冲占用约 12 KiB RAM。
- Vpp 合成默认执行 `4096 × 16 × 有效峰数` 次分量累加，耗时明显；不需要 Vpp 时可减少过采样倍数。
- 内部使用静态 EMA 缓冲和增益系数，接口不可重入，也不能同时处理两路独立频谱。
- `fft_peak_calculate_thd()` 把首峰以外的所有有效峰都计入分子；若这些峰不是首峰的整数倍，它表示“其他分量比”，不等同严格定义的 THD。
- 已保持 EDC0801_1 原算法和标定表，并完成源文件依赖检查；仍需在目标采样链路上验证阈值、频率精度和幅值标定。
