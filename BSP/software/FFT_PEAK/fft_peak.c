#include "fft_peak.h"
#include "fft_amplitude_calibration.h"
#include "fft_secondary_amplitude_calibration.h"

#include <math.h>
#include <stddef.h>

static uint32_t fft_ema_accumulator[FFT_PEAK_POINT_COUNT/2];
static uint16_t fft_averaged_spectrum[FFT_PEAK_POINT_COUNT/2];
static uint8_t fft_ema_initialized;
static float fft_amplitude_scale_factor=1.0f;

void fft_peak_reset_average(void)
{
    fft_ema_initialized=0u;
}

static uint16_t *fft_peak_update_average(uint16_t *fft)
{
    uint16_t i;
    const uint32_t rounding=1u<<(FFT_PEAK_EMA_SHIFT-1u);

    if(fft_ema_initialized==0u)
    {
        for(i=0u;i<(FFT_PEAK_POINT_COUNT/2);i++)
        {
            fft_ema_accumulator[i]=((uint32_t)fft[i])<<FFT_PEAK_EMA_SHIFT;
            fft_averaged_spectrum[i]=fft[i];
        }

        fft_ema_initialized=1u;
        return fft_averaged_spectrum;
    }

    for(i=0u;i<(FFT_PEAK_POINT_COUNT/2);i++)
    {
        /* y[n]=7/8*y[n-1]+1/8*x[n]，使用Q3累加器和移位实现。 */
        fft_ema_accumulator[i]=fft_ema_accumulator[i]
                -(fft_ema_accumulator[i]>>FFT_PEAK_EMA_SHIFT)
                +(uint32_t)fft[i];
        fft_averaged_spectrum[i]=(uint16_t)
                ((fft_ema_accumulator[i]+rounding)>>FFT_PEAK_EMA_SHIFT);
    }

    return fft_averaged_spectrum;
}

static uint32_t fft_peak_get_merged_amplitude(uint16_t *fft,
                                       uint16_t bin)
{
    uint32_t merged=(uint32_t)fft[bin];

    if(bin>FFT_PEAK_IGNORED_BIN_COUNT)
    {
        merged+=(uint32_t)fft[bin-1u];
    }

    if(bin<((FFT_PEAK_POINT_COUNT/2)-1u))
    {
        merged+=(uint32_t)fft[bin+1u];
    }

    return merged;
}

static uint16_t fft_peak_refine_bin(uint16_t *fft,
                                  uint16_t mergedPeakBin)
{
    uint16_t left=mergedPeakBin;
    uint16_t right=mergedPeakBin;
    uint16_t maxBin;
    uint16_t i;

    if(mergedPeakBin>FFT_PEAK_IGNORED_BIN_COUNT)
    {
        left=mergedPeakBin-1u;
    }

    if(mergedPeakBin<((FFT_PEAK_POINT_COUNT/2)-1u))
    {
        right=mergedPeakBin+1u;
    }

    maxBin=left;
    for(i=left+1u;i<=right;i++)
    {
        if(fft[i]>fft[maxBin])
        {
            maxBin=i;
        }
    }

    return maxBin;
}

static float fft_peak_calculate_raw_amplitude(uint16_t *fft,
                                        uint16_t peakIndex,
                                        float fractionalOffset)
{
    uint16_t radius;
    uint16_t firstIndex;
    uint16_t lastIndex;
    uint16_t i;
    float value;
    float squareSum=0.0f;

    if((fractionalOffset>=0.3f) && (fractionalOffset<=0.7f))
    {
        /* 小数偏移0.3~0.7：峰值左右各3点，共7点。 */
        radius=3u;
    }
    else if((fractionalOffset>=0.1f) && (fractionalOffset<0.3f))
    {
        /* 小数偏移0.1~0.3：峰值左右各2点，共5点。 */
        radius=2u;
    }
    else
    {
        /* 小数偏移小于0.1：峰值左右各1点，共3点。 */
        radius=1u;
    }

    if(peakIndex>radius)
    {
        firstIndex=peakIndex-radius;
    }
    else
    {
        firstIndex=0u;
    }
    lastIndex=peakIndex+radius;

    if(lastIndex>=(FFT_PEAK_POINT_COUNT/2))
    {
        lastIndex=(FFT_PEAK_POINT_COUNT/2)-1u;
    }

    for(i=firstIndex;i<=lastIndex;i++)
    {
        value=(float)fft[i];
        squareSum+=value*value;
    }

    return sqrtf(squareSum);
}

float fft_peak_get_calibration_amplitude(float frequencyHz)
{
    uint32_t tableIndex;

    if(frequencyHz<=FFT_PEAK_CALIBRATION_MIN_FREQ_HZ)
    {
        tableIndex=0u;
    }
    else if(frequencyHz>=FFT_PEAK_CALIBRATION_MAX_FREQ_HZ)
    {
        tableIndex=FFT_PEAK_CALIBRATION_POINT_COUNT-1u;
    }
    else
    {
        /* 加0.5后取整，选择与当前频率最接近的500 Hz标定点。 */
        tableIndex=(uint32_t)
                (((frequencyHz-FFT_PEAK_CALIBRATION_MIN_FREQ_HZ)
                /FFT_PEAK_CALIBRATION_STEP_HZ)+0.5f);

        if(tableIndex>=FFT_PEAK_CALIBRATION_POINT_COUNT)
        {
            tableIndex=FFT_PEAK_CALIBRATION_POINT_COUNT-1u;
        }
    }

    return (float)fft_peak_calibration_amplitude[tableIndex];
}

void fft_peak_set_amplitude_scale(float scaleFactor)
{
    if(scaleFactor>0.0f)
    {
        fft_amplitude_scale_factor=scaleFactor;
    }
}

float fft_peak_get_amplitude_scale(void)
{
    return fft_amplitude_scale_factor;
}

float fft_peak_calibrate_amplitude(float rawAmplitude,
                                 float frequencyHz)
{
    float calibrationAmplitude;

    calibrationAmplitude=fft_peak_get_calibration_amplitude(frequencyHz);
    if(calibrationAmplitude<=0.0f)
    {
        return 0.0f;
    }

    /* 标定表对应250 mVpp，因此单个正弦分量的峰值为125 mV。 */
    return rawAmplitude
            *fft_amplitude_scale_factor
            *(FFT_PEAK_CALIBRATION_VPP_MV*0.5f)
            /calibrationAmplitude;
}

static float fft_peak_get_secondary_correction(float frequencyHz)
{
    uint32_t lowerIndex;
    float tablePosition;
    float fraction;
    float lowerValue;
    float upperValue;
    float measuredVpp;

    if(frequencyHz<=FFT_PEAK_SECOND_CAL_MIN_FREQ_HZ)
    {
        measuredVpp=(float)fft_peak_second_calibration_measured_vpp[0];
    }
    else if(frequencyHz>=FFT_PEAK_SECOND_CAL_MAX_FREQ_HZ)
    {
        measuredVpp=(float)fft_peak_second_calibration_measured_vpp[
                FFT_PEAK_SECOND_CAL_POINT_COUNT-1u];
    }
    else
    {
        tablePosition=(frequencyHz-FFT_PEAK_SECOND_CAL_MIN_FREQ_HZ)
                /FFT_PEAK_SECOND_CAL_STEP_HZ;
        lowerIndex=(uint32_t)tablePosition;
        fraction=tablePosition-(float)lowerIndex;
        lowerValue=(float)fft_peak_second_calibration_measured_vpp[lowerIndex];
        upperValue=(float)fft_peak_second_calibration_measured_vpp[lowerIndex+1u];
        measuredVpp=lowerValue+(upperValue-lowerValue)*fraction;
    }

    if(measuredVpp<=0.0f)
    {
        return 1.0f;
    }

    return FFT_PEAK_SECOND_CAL_REFERENCE_VPP_MV/measuredVpp;
}

void fft_peak_apply_secondary_calibration(fft_peak_result_t *signal)
{
    uint8_t harmonicCount;
    uint8_t i;

    if(signal==NULL)
    {
        return;
    }

    signal->fundamental_amp*=fft_peak_get_secondary_correction(
            signal->fundamental_freq);

    harmonicCount=signal->harmonic_num;
    if(harmonicCount>FFT_PEAK_MAX_COUNT)
    {
        harmonicCount=FFT_PEAK_MAX_COUNT;
    }

    for(i=0u;i<harmonicCount;i++)
    {
        signal->harmonic_amp[i]*=fft_peak_get_secondary_correction(
                signal->harmonic_freq[i]);
    }
}

/*************************************************
函数名：fft_peak_find_max_bin
功能：寻找基波峰值（忽略直流）
*************************************************/
uint16_t fft_peak_find_max_bin(uint16_t *fft)
{
    uint16_t i;
    uint16_t maxIndex=FFT_PEAK_IGNORED_BIN_COUNT;
    uint16_t maxValue=fft[FFT_PEAK_IGNORED_BIN_COUNT];

    for(i=FFT_PEAK_IGNORED_BIN_COUNT+1u;i<FFT_PEAK_POINT_COUNT/2;i++)
    {
        if(fft[i]>maxValue)
        {
            maxValue=fft[i];
            maxIndex=i;
        }
    }

    return maxIndex;
}

/*************************************************
函数名：fft_peak_interpolate
功能：最大峰值与相邻次大值比例插值
*************************************************/
void fft_peak_interpolate(uint16_t *fft,
                            uint16_t peakIndex,
                            fft_peak_info_t *peak)
{
    uint16_t maxValue;
    uint16_t leftValue;
    uint16_t rightValue=0u;
    uint16_t secondValue;
    float ratio;
    float fractionalOffset;

    if(peakIndex==0)
        peakIndex=1;

    if(peakIndex>=FFT_PEAK_POINT_COUNT/2)
        peakIndex=FFT_PEAK_POINT_COUNT/2-1;

    maxValue=fft[peakIndex];
    leftValue=fft[peakIndex-1u];

    if(peakIndex<(FFT_PEAK_POINT_COUNT/2-1u))
    {
        rightValue=fft[peakIndex+1u];
    }

    if(rightValue>leftValue)
    {
        secondValue=rightValue;
        ratio=(float)secondValue/((float)maxValue+(float)secondValue);
    }
    else if(leftValue>rightValue)
    {
        secondValue=leftValue;
        ratio=-(float)secondValue/((float)maxValue+(float)secondValue);
    }
    else
    {
        ratio=0.0f;
    }

    fractionalOffset=fabsf(ratio);

    peak->bin=peakIndex;

    peak->delta=ratio;

    peak->freq=((float)peakIndex+ratio)
                *FFT_PEAK_FREQUENCY_RESOLUTION_HZ;

    peak->raw_amp=fft_peak_calculate_raw_amplitude(fft,
                                             peakIndex,
                                             fractionalOffset);

    peak->amp=fft_peak_calibrate_amplitude(peak->raw_amp,
                                         peak->freq);
}

static uint8_t fft_peak_is_cfar_local_maximum(uint16_t *fft,
                                   uint16_t peakIndex,
                                   uint32_t *mergedAmplitude)
{
    uint32_t referenceSum=0u;
    uint32_t cellAmplitude;
    uint32_t thresholdSum;
    uint16_t referenceCount=0u;
    uint16_t offset;

    cellAmplitude=fft_peak_get_merged_amplitude(fft,peakIndex);
    if(cellAmplitude==0u)
    {
        return 0u;
    }

    /* 当前点必须是三点合并幅度谱上的局部最大值。 */
    if(peakIndex>FFT_PEAK_IGNORED_BIN_COUNT)
    {
        if(fft_peak_get_merged_amplitude(fft,peakIndex-1u)>=cellAmplitude)
        {
            return 0u;
        }
    }

    if(peakIndex<((FFT_PEAK_POINT_COUNT/2)-1u))
    {
        if(fft_peak_get_merged_amplitude(fft,peakIndex+1u)>cellAmplitude)
        {
            return 0u;
        }
    }

    /* 左右各16个参考单元，中间各留3个保护单元。 */
    for(offset=FFT_PEAK_CFAR_GUARD_CELLS+1u;
        offset<=FFT_PEAK_CFAR_GUARD_CELLS+FFT_PEAK_CFAR_REFERENCE_CELLS;
        offset++)
    {
        if(peakIndex>=(FFT_PEAK_IGNORED_BIN_COUNT+offset))
        {
            referenceSum+=fft_peak_get_merged_amplitude(fft,peakIndex-offset);
            referenceCount++;
        }

        if((uint32_t)peakIndex+(uint32_t)offset<(FFT_PEAK_POINT_COUNT/2))
        {
            referenceSum+=fft_peak_get_merged_amplitude(fft,peakIndex+offset);
            referenceCount++;
        }
    }

    if(referenceCount==0u)
    {
        return 0u;
    }

    /*
     * cell > 3*(referenceSum/referenceCount)。
     * 交叉相乘避免除法；常量3在优化构建中会转为移位和加法。
     */
    thresholdSum=referenceSum*FFT_PEAK_CFAR_THRESHOLD_MULTIPLIER;
    if((cellAmplitude*(uint32_t)referenceCount)<=thresholdSum)
    {
        return 0u;
    }

    *mergedAmplitude=cellAmplitude;
    return 1u;
}

static void fft_peak_save_detected(fft_peak_info_t *detectedPeaks,
                                 uint8_t *peakCount,
                                 const fft_peak_info_t *newPeak)
{
    uint8_t weakestIndex;
    uint8_t i;

    /* Filter by calibrated physical peak voltage, not raw FFT magnitude. */
    if(newPeak->amp<FFT_PEAK_MIN_AMPLITUDE_MV)
    {
        return;
    }

    if(newPeak->freq>FFT_PEAK_MAX_FREQUENCY_HZ)
    {
        return;
    }

    if(*peakCount<FFT_PEAK_MAX_COUNT)
    {
        detectedPeaks[*peakCount]=*newPeak;
        (*peakCount)++;
        return;
    }

    /*
     * detectedPeaks[0]是从低频向高频扫描得到的第一个有效峰，
     * 始终保留作为基波候选；其余位置保留原始FFT幅值最强的峰。
     */
    weakestIndex=1u;
    for(i=2u;i<FFT_PEAK_MAX_COUNT;i++)
    {
        if(detectedPeaks[i].raw_amp<detectedPeaks[weakestIndex].raw_amp)
        {
            weakestIndex=i;
        }
    }

    if(newPeak->raw_amp>detectedPeaks[weakestIndex].raw_amp)
    {
        detectedPeaks[weakestIndex]=*newPeak;
    }
}

static void fft_peak_sort_by_frequency(fft_peak_info_t *detectedPeaks,
                                     uint8_t peakCount)
{
    uint8_t i;
    uint8_t j;
    fft_peak_info_t current;

    for(i=1u;i<peakCount;i++)
    {
        current=detectedPeaks[i];
        j=i;

        while((j>0u) &&
              (detectedPeaks[j-1u].freq>current.freq))
        {
            detectedPeaks[j]=detectedPeaks[j-1u];
            j--;
        }

        detectedPeaks[j]=current;
    }
}

/*************************************************
函数名：fft_peak_find
功能：扫描整个正频率范围，寻找所有有效局部峰
*************************************************/
static void fft_peak_find_internal(uint16_t *detectionFft,
                          uint16_t *amplitudeFft,
                          fft_peak_result_t *signal)
{
    fft_peak_info_t detectedPeaks[FFT_PEAK_MAX_COUNT];
    fft_peak_info_t peak;
    uint16_t scanBin=FFT_PEAK_IGNORED_BIN_COUNT;
    uint16_t candidateBin;
    uint16_t pendingBin=0u;
    uint16_t amplitudeBin;
    uint32_t mergedAmplitude;
    uint32_t pendingAmplitude=0u;
    uint8_t pendingValid=0u;
    uint8_t peakCount=0u;
    uint8_t i;

    while(scanBin<(FFT_PEAK_POINT_COUNT/2))
    {
        if(fft_peak_is_cfar_local_maximum(detectionFft,scanBin,&mergedAmplitude)!=0u)
        {
            candidateBin=fft_peak_refine_bin(detectionFft,scanBin);

            if(pendingValid==0u)
            {
                pendingBin=candidateBin;
                pendingAmplitude=mergedAmplitude;
                pendingValid=1u;
            }
            else if(((uint32_t)candidateBin-(uint32_t)pendingBin)
                    <FFT_PEAK_MIN_DISTANCE_BINS)
            {
                /* 距离不足19个bin时，只保留三点合并幅度较强的峰。 */
                if(mergedAmplitude>pendingAmplitude)
                {
                    pendingBin=candidateBin;
                    pendingAmplitude=mergedAmplitude;
                }
            }
            else
            {
                amplitudeBin=fft_peak_refine_bin(amplitudeFft,pendingBin);
                fft_peak_interpolate(amplitudeFft,amplitudeBin,&peak);
                fft_peak_save_detected(detectedPeaks,&peakCount,&peak);
                pendingBin=candidateBin;
                pendingAmplitude=mergedAmplitude;
            }
        }

        scanBin++;
    }

    if(pendingValid!=0u)
    {
        amplitudeBin=fft_peak_refine_bin(amplitudeFft,pendingBin);
        fft_peak_interpolate(amplitudeFft,amplitudeBin,&peak);
        fft_peak_save_detected(detectedPeaks,&peakCount,&peak);
    }

    if(peakCount==0u)
    {
        signal->harmonic_num=0u;
        return;
    }

    fft_peak_sort_by_frequency(detectedPeaks,peakCount);

    /* 最低频有效峰作为基波。 */
    signal->fundamental_freq=detectedPeaks[0].freq;
    signal->fundamental_amp=detectedPeaks[0].amp;
    signal->fundamental_raw_amp=detectedPeaks[0].raw_amp;

    for(i=0u;i<peakCount;i++)
    {
        signal->harmonic_freq[i]=detectedPeaks[i].freq;
        signal->harmonic_amp[i]=detectedPeaks[i].amp;
        signal->harmonic_raw_amp[i]=detectedPeaks[i].raw_amp;
    }

    signal->harmonic_num=peakCount;
}

void fft_peak_find(uint16_t *fft,
                       fft_peak_result_t *signal)
{
    fft_peak_find_internal(fft,fft,signal);
}

/*************************************************
函数名：fft_peak_calculate_rms
功能：计算信号RMS
说明：
    每个谱线保存的是峰值
    RMS=√(Σ(A2/2))
*************************************************/
float fft_peak_calculate_rms(fft_peak_result_t *signal)
{
    float sum=0.0f;

    uint8_t i;

    for(i=0;i<signal->harmonic_num;i++)
    {
        sum+=signal->harmonic_amp[i]
            *signal->harmonic_amp[i];
    }

    return sqrtf(sum/2.0f);
}

/*************************************************
函数名：fft_peak_calculate_vpp
功能：计算峰峰值
*************************************************/
static float fft_peak_calculate_vpp_oversampled(fft_peak_result_t *signal,
                                    uint32_t oversampleFactor)
{
    float phaseSin[FFT_PEAK_MAX_COUNT];
    float phaseCos[FFT_PEAK_MAX_COUNT];
    float stepSin[FFT_PEAK_MAX_COUNT];
    float stepCos[FFT_PEAK_MAX_COUNT];
    float minimum=0.0f;
    float maximum=0.0f;
    uint8_t componentCount=signal->harmonic_num;
    uint8_t i;
    uint32_t sample;
    uint32_t totalSamples;
    float synthesisSampleRate;

    if(oversampleFactor==0u)
    {
        oversampleFactor=1u;
    }

    totalSamples=(uint32_t)FFT_PEAK_POINT_COUNT*oversampleFactor;
    synthesisSampleRate=FFT_PEAK_SAMPLE_RATE_HZ*(float)oversampleFactor;

    if(componentCount>FFT_PEAK_MAX_COUNT)
    {
        componentCount=FFT_PEAK_MAX_COUNT;
    }

    if(componentCount==0u)
    {
        return 0.0f;
    }

    /* All components use phi=0. Precompute one-sample phase increments. */
    for(i=0u;i<componentCount;i++)
    {
        float phaseStep=2.0f*3.14159265358979323846f
                *signal->harmonic_freq[i]/synthesisSampleRate;

        phaseSin[i]=0.0f;
        phaseCos[i]=1.0f;
        stepSin[i]=sinf(phaseStep);
        stepCos[i]=cosf(phaseStep);
    }

    /* Reconstruct x(t) over the same acquisition window at higher resolution. */
    for(sample=0u;sample<totalSamples;sample++)
    {
        float value=0.0f;

        for(i=0u;i<componentCount;i++)
        {
            float nextSin;
            float nextCos;

            value+=signal->harmonic_amp[i]*phaseSin[i];

            nextSin=phaseSin[i]*stepCos[i]+phaseCos[i]*stepSin[i];
            nextCos=phaseCos[i]*stepCos[i]-phaseSin[i]*stepSin[i];
            phaseSin[i]=nextSin;
            phaseCos[i]=nextCos;
        }

        /* Keep the recursive oscillator on the unit circle. */
        if((sample&0xFFu)==0xFFu)
        {
            for(i=0u;i<componentCount;i++)
            {
                float magnitudeSquared=phaseSin[i]*phaseSin[i]
                        +phaseCos[i]*phaseCos[i];
                float correction=0.5f*(3.0f-magnitudeSquared);

                phaseSin[i]*=correction;
                phaseCos[i]*=correction;
            }
        }

        if(value<minimum)
        {
            minimum=value;
        }
        if(value>maximum)
        {
            maximum=value;
        }
    }

    return maximum-minimum;
}

float fft_peak_calculate_vpp(fft_peak_result_t *signal)
{
    return fft_peak_calculate_vpp_oversampled(signal,FFT_PEAK_VPP_OVERSAMPLE_FACTOR);
}

/*************************************************
函数名：fft_peak_calculate_thd
功能：计算THD
*************************************************/
float fft_peak_calculate_thd(fft_peak_result_t *signal)
{
    float sum=0.0f;

    uint8_t i;

    if(signal->harmonic_num<=1)
        return 0.0f;

    for(i=1;i<signal->harmonic_num;i++)
    {
        sum+=signal->harmonic_amp[i]
            *signal->harmonic_amp[i];
    }

    if(signal->fundamental_amp<1e-6f)
        return 0.0f;

    return
        sqrtf(sum)
        /
        signal->fundamental_amp
        *
        100.0f;
}

/*************************************************
函数名：fft_peak_process
功能：FFT总处理
*************************************************/
void fft_peak_process(uint16_t *fft,
                 fft_peak_result_t *signal)
{
    uint16_t *processedFft;
    uint16_t i;

    /* 处理前覆盖直流及其附近的前18个频点。 */
    for(i=0u;i<FFT_PEAK_PRESET_BIN_COUNT;i++)
    {
        fft[i]=FFT_PEAK_PRESET_BIN_VALUE;
    }

    processedFft=fft_peak_update_average(fft);

    signal->fundamental_freq=0.0f;

    signal->fundamental_amp=0.0f;

    signal->fundamental_raw_amp=0.0f;

    signal->harmonic_num=0;

    signal->rms=0.0f;

    signal->vpp=0.0f;

    signal->thd=0.0f;

    for(i=0;i<FFT_PEAK_MAX_COUNT;i++)
    {
        signal->harmonic_freq[i]=0.0f;

        signal->harmonic_amp[i]=0.0f;

        signal->harmonic_raw_amp[i]=0.0f;
    }

    /* EMA只用于稳定找峰；幅值使用当前帧，避免信源调节后的滞后。 */
    fft_peak_find_internal(processedFft,fft,signal);

    signal->rms=fft_peak_calculate_rms(signal);

    /* Per-frame Vpp is only diagnostic; the averaged result uses 16x accuracy. */
    signal->vpp=fft_peak_calculate_vpp_oversampled(signal,1u);

    signal->thd=fft_peak_calculate_thd(signal);
}
