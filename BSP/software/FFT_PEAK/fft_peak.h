#ifndef BSP_SOFTWARE_FFT_PEAK_H
#define BSP_SOFTWARE_FFT_PEAK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/************************ 用户配置区：移植时只修改这里 ************************/

/* 输入频谱对应的 FFT 点数和采样率。输入数组至少包含 FFT_PEAK_POINT_COUNT/2 个点。 */
#define FFT_PEAK_POINT_COUNT                  4096U
#define FFT_PEAK_SAMPLE_RATE_HZ               2000000.0f

/* 最多保存的有效谱峰数量。 */
#define FFT_PEAK_MAX_COUNT                    100U

/* 忽略直流附近频点，并在处理前把前若干点覆盖为固定值。 */
#define FFT_PEAK_IGNORED_BIN_COUNT          16U
#define FFT_PEAK_PRESET_BIN_COUNT           18U
#define FFT_PEAK_PRESET_BIN_VALUE           50U

/* 指数平均：右移 3 位等价于 1/8 新数据权重。 */
#define FFT_PEAK_EMA_SHIFT                  3U

/* CFAR 峰值检测参数。 */
#define FFT_PEAK_CFAR_REFERENCE_CELLS         16U
#define FFT_PEAK_CFAR_GUARD_CELLS             3U
#define FFT_PEAK_CFAR_THRESHOLD_MULTIPLIER    3U
#define FFT_PEAK_MIN_DISTANCE_BINS            19U

/* 经过标定后的有效峰筛选范围。 */
#define FFT_PEAK_MIN_AMPLITUDE_MV             3.0f
#define FFT_PEAK_MAX_FREQUENCY_HZ             600000.0f

/* 最终 Vpp 合成时相对于原采样率的过采样倍数。 */
#define FFT_PEAK_VPP_OVERSAMPLE_FACTOR       16U

/************************ 用户配置区结束 ***************************************/

#define FFT_PEAK_FREQUENCY_RESOLUTION_HZ \
    (FFT_PEAK_SAMPLE_RATE_HZ / (float)FFT_PEAK_POINT_COUNT)

#if ((FFT_PEAK_POINT_COUNT < 4U) || ((FFT_PEAK_POINT_COUNT % 2U) != 0U))
#error "FFT_PEAK_POINT_COUNT must be an even value of at least 4"
#endif

#if ((FFT_PEAK_MAX_COUNT < 2U) || (FFT_PEAK_MAX_COUNT > 255U))
#error "FFT_PEAK_MAX_COUNT must be in the range 2..255"
#endif

#if (FFT_PEAK_IGNORED_BIN_COUNT >= (FFT_PEAK_POINT_COUNT / 2U))
#error "FFT_PEAK_IGNORED_BIN_COUNT is outside the input spectrum"
#endif

#if (FFT_PEAK_PRESET_BIN_COUNT > (FFT_PEAK_POINT_COUNT / 2U))
#error "FFT_PEAK_PRESET_BIN_COUNT is outside the input spectrum"
#endif

#if ((FFT_PEAK_EMA_SHIFT < 1U) || (FFT_PEAK_EMA_SHIFT > 16U))
#error "FFT_PEAK_EMA_SHIFT must be in the range 1..16"
#endif

#if (FFT_PEAK_CFAR_REFERENCE_CELLS < 1U)
#error "FFT_PEAK_CFAR_REFERENCE_CELLS must be at least 1"
#endif

/** 单个谱峰的频率和幅值信息。 */
typedef struct
{
    uint16_t bin;          /**< 整数峰值 bin。 */
    float delta;           /**< 相邻谱线比例插值得到的 bin 偏移。 */
    float freq;            /**< 插值后的频率，单位 Hz。 */
    float raw_amp;         /**< 合并峰值附近谱线后的原始幅值。 */
    float amp;             /**< 标定后的正弦分量峰值，单位 mV。 */
} fft_peak_info_t;

/** 一帧频谱的有效分量和派生测量值。 */
typedef struct
{
    float fundamental_freq;                  /**< 最低频有效峰，单位 Hz。 */
    float fundamental_amp;                   /**< 最低频有效峰的标定峰值，单位 mV。 */
    float fundamental_raw_amp;               /**< 最低频有效峰的原始幅值。 */
    float harmonic_freq[FFT_PEAK_MAX_COUNT];      /**< 所有有效峰频率，按频率升序。 */
    float harmonic_amp[FFT_PEAK_MAX_COUNT];       /**< 所有有效峰的标定峰值，单位 mV。 */
    float harmonic_raw_amp[FFT_PEAK_MAX_COUNT];   /**< 所有有效峰的原始幅值。 */
    uint8_t harmonic_num;                    /**< 有效峰数量。 */
    float rms;                               /**< 所有有效分量合成的 RMS，单位 mV。 */
    float vpp;                               /**< 零初相位合成估算的峰峰值，单位 mV。 */
    float thd;                               /**< 除首峰外其余分量相对首峰的比例，单位 %。 */
} fft_peak_result_t;

/**
 * @brief 读取最接近指定频率的原始幅值标定点。
 * @param frequencyHz 目标频率，单位 Hz。
 * @return 原工程 250 mVpp 标定输入对应的原始频谱幅值。
 */
float fft_peak_get_calibration_amplitude(float frequencyHz);

/**
 * @brief 把原始峰值幅度换算为正弦分量峰值。
 * @param rawAmplitude 峰值附近谱线平方和开方后的原始幅值。
 * @param frequencyHz 峰值频率，单位 Hz。
 * @return 标定后的正弦分量峰值，单位 mV。
 */
float fft_peak_calibrate_amplitude(float rawAmplitude,
                                 float frequencyHz);

/**
 * @brief 对多帧筛选后的所有有效分量应用第二级幅值修正。
 * @param signal 待修正的信号信息，不能为 NULL。
 */
void fft_peak_apply_secondary_calibration(fft_peak_result_t *signal);

/**
 * @brief 设置运行时整机增益修正系数。
 * @param scaleFactor 大于 0 的幅值乘数；无效值会被忽略。
 */
void fft_peak_set_amplitude_scale(float scaleFactor);

/**
 * @brief 读取当前运行时整机增益修正系数。
 * @return 当前幅值乘数，默认值为 1.0。
 */
float fft_peak_get_amplitude_scale(void);

/**
 * @brief 在正频率幅度谱中查找忽略直流后的最大 bin。
 * @param fft 至少包含 FFT_PEAK_POINT_COUNT/2 个 uint16_t 点的幅度谱。
 * @return 最大谱线的整数 bin。
 */
uint16_t fft_peak_find_max_bin(uint16_t *fft);

/**
 * @brief 根据峰值左右相邻谱线进行比例插值，并计算频率和幅值。
 * @param fft 至少包含 FFT_PEAK_POINT_COUNT/2 个点的幅度谱。
 * @param peakIndex 整数峰值 bin。
 * @param peak 输出峰值信息，不能为 NULL。
 */
void fft_peak_interpolate(uint16_t *fft,
                            uint16_t peakIndex,
                            fft_peak_info_t *peak);

/**
 * @brief 使用 CFAR、最小间距和幅值阈值查找全部有效谱峰。
 * @param fft 至少包含 FFT_PEAK_POINT_COUNT/2 个点的幅度谱。
 * @param signal 输出信号信息，不能为 NULL。
 * @note 本接口不执行指数平均；检测和幅值计算都使用传入频谱。
 */
void fft_peak_find(uint16_t *fft,
                       fft_peak_result_t *signal);

/**
 * @brief 清除指数平均状态，使下一帧频谱成为新的平均初值。
 */
void fft_peak_reset_average(void);

/**
 * @brief 根据所有有效分量的峰值计算总体 RMS。
 * @param signal 已完成找峰和幅值标定的信号信息。
 * @return 总体 RMS，单位 mV。
 */
float fft_peak_calculate_rms(fft_peak_result_t *signal);

/**
 * @brief 以所有分量零初相位重建波形并估算峰峰值。
 * @param signal 已完成找峰和幅值标定的信号信息。
 * @return 合成波形峰峰值，单位 mV。
 */
float fft_peak_calculate_vpp(fft_peak_result_t *signal);

/**
 * @brief 计算除首峰以外所有有效分量相对首峰的幅值比例。
 * @param signal 已完成找峰和幅值标定的信号信息。
 * @return 比例，单位 %；有效峰不足两个时返回 0。
 * @note 只有输入峰确实是整数次谐波时，该结果才等同传统 THD。
 */
float fft_peak_calculate_thd(fft_peak_result_t *signal);

/**
 * @brief 对一帧 uint16_t 幅度谱执行平均、CFAR 找峰、插值和测量计算。
 * @param fft 至少包含 FFT_PEAK_POINT_COUNT/2 个点的可写幅度谱。
 * @param signal 输出结果，不能为 NULL。
 * @note 本函数会覆盖 fft[0..FFT_PEAK_PRESET_BIN_COUNT-1]，且内部状态不可重入。
 */
void fft_peak_process(uint16_t *fft,
                 fft_peak_result_t *signal);

#ifdef __cplusplus
}
#endif

#endif
