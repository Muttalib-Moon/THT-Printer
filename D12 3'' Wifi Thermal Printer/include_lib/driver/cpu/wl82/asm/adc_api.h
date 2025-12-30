#ifndef __ADC_API_H__
#define __ADC_API_H__

/**
 * \name AD channel define
 * \{
 */
#define AD_CH_PA07    (0x0)
#define AD_CH_PA08    (0x1)
#define AD_CH_PA10    (0x2)
#define AD_CH_PB01    (0x3)
#define AD_CH_PB06    (0x4)
#define AD_CH_PB07    (0x5)
#define AD_CH_PC00    (0x6)
#define AD_CH_PC01    (0x7)
#define AD_CH_PC09    (0x8)
#define AD_CH_PC10    (0x9)
#define AD_CH_PH00    (0xa)
#define AD_CH_PH03    (0xb)
#define AD_CH_DM      (0xc)
#define AD_CH_DP      (0xd)
#define AD_CH_RTC     (0xe)
#define AD_CH_PMU     (0xf)
#define AD_CH_SYSPLL  (0xf)
#define AD_CH_AUDIO   (0xf)
#define AD_CH_WIFI    (0xf)
#define AD_CH_CTMU    (0xf)


#define ADC_PMU_CH_VBG       (0x0<<16)
#define ADC_PMU_CH_VDC14     (0x1<<16)
#define ADC_PMU_CH_SYSVDD    (0x2<<16)
#define ADC_PMU_CH_VTEMP     (0x3<<16)
#define ADC_PMU_CH_PROGF     (0x4<<16)
#define ADC_PMU_CH_VBAT      (0x5<<16)     //1/4 vbat
#define ADC_PMU_CH_LDO5V     (0x6<<16)     //1/4 LDO 5V
#define ADC_PMU_CH_WVDD      (0x7<<16)

#define AD_CH_LDOREF    AD_CH_PMU_VBG

#define AD_CH_PMU_VBG   (AD_CH_PMU | ADC_PMU_CH_VBG)
#define AD_CH_VDC14     (AD_CH_PMU | ADC_PMU_CH_VDC14)
#define AD_CH_SYSVDD    (AD_CH_PMU | ADC_PMU_CH_SYSVDD)
#define AD_CH_VTEMP     (AD_CH_PMU | ADC_PMU_CH_VTEMP)
#define AD_CH_VBAT      (AD_CH_PMU | ADC_PMU_CH_VBAT)
#define AD_CH_LDO5V     (AD_CH_PMU | ADC_PMU_CH_LDO5V)
#define AD_CH_WVDD      (AD_CH_PMU | ADC_PMU_CH_WVDD)


#define AD_AUDIO_VCM     ((BIT(0))<<16)
#define AD_AUDIO_VOUTL   ((BIT(1))<<16)
#define AD_AUDIO_VOUTR   ((BIT(2))<<16)
#define AD_AUDIO_DACVDD  ((BIT(3))<<16)


#define AD_CH_VCM        (AD_CH_AUDIO | AD_AUDIO_VCM)
#define AD_CH_VOUTL      (AD_CH_AUDIO | AD_AUDIO_VOUTL)
#define AD_CH_VOUTR      (AD_CH_AUDIO | AD_AUDIO_VOUTR)
#define AD_CH_DACVDD     (AD_CH_AUDIO | AD_AUDIO_DACVDD)

#define ADC_MAX_CH  10
/* \} name */

/**
 * @brief adc_init, adc初始�?
*/
void adc_init(void);

/**
 * @brief adc_pmu_detect_en, 使能adc通道
 * @param ch : ADC通道�?, AD_CH_PA07
*/
 void adc_pmu_detect_en(u32 ch);

/**
 * @brief adc_vdc14_save, 存储vdc14的�??
*/
void adc_pmu_ch_select(u32 ch);

void adc_vdc14_save(void);

/**
 * @brief adc_vdc14_restore, 再次存储vdc14的�?�，避免被中途修�?
*/
void adc_vdc14_restore(void);

/**
 * @brief adc_get_value, 获取adc通道测得的数�?(3.3*数�??/1024)即得对应的电压�?�采用等待的方式采�?�，直到转换结束，才出来，即死等
 * @param ch : ADC通道�?, AD_CH_PA07
 * @return 当前通道的AD�?
*/
u32 adc_get_value(u32 ch);

/**
 * @brief adc_add_sample_ch, 添加adc测试通道
 * @param ch : ADC通道�?
 * @return 当前通道�?
*/
u32 adc_add_sample_ch(u32 ch);

/**
 * @brief adc_remove_sample_ch, 移除adc测试通道
 * @param ch : ADC通道�?
 * @return 当前通道�?
*/
u32 adc_remove_sample_ch(u32 ch);

/**
 * @brief adc_get_voltage, 换算电压的公式函�?, 获取adc通道电压值，如果测得与实际不符，则需留意芯片是否trim过，trim值是否正确�??
 * @param ch : ADC通道�?
 * @return 当前通道的电压�?�，单位mv
*/
u32 adc_get_voltage(u32 ch);

#endif
