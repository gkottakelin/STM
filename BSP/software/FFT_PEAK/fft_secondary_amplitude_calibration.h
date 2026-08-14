#ifndef BSP_SOFTWARE_FFT_PEAK_SECONDARY_CALIBRATION_H
#define BSP_SOFTWARE_FFT_PEAK_SECONDARY_CALIBRATION_H

#include <stdint.h>

#define FFT_PEAK_SECOND_CAL_MIN_FREQ_HZ       10000.0f
#define FFT_PEAK_SECOND_CAL_MAX_FREQ_HZ      500000.0f
#define FFT_PEAK_SECOND_CAL_STEP_HZ            5000.0f
#define FFT_PEAK_SECOND_CAL_POINT_COUNT             99u
#define FFT_PEAK_SECOND_CAL_REFERENCE_VPP_MV      250.0f

/*
 * Measured signal amplitude for a 250 mVpp input after the first
 * calibration stage.
 * Source: 新建 Microsoft Excel 工作表.xlsx, Sheet1!A2:B100.
 */
static const uint8_t
fft_peak_second_calibration_measured_vpp[FFT_PEAK_SECOND_CAL_POINT_COUNT] =
{
    255, 253, 255, 254, 255, 253, 254, 253, 254, 253, 255, 254, 254, 253, 254, 254,
    254, 254, 254, 248, 248, 248, 248, 248, 248, 248, 249, 249, 249, 249, 250, 249,
    250, 249, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250, 250,
    250, 250, 250
};

#endif
