/*
 * 连续读取的主机状态机回归。编译通过-I传入实际驱动的Core/Src与Core/Inc。
 * 本文件包含实际ms5837.c形成单翻译单元，只有冷启动复位测试夹具时清内部状态，
 * 运行测试均从公开StartDMA进入完整复位、PROM/CRC、基准采样流程。
 * HAL桩模拟DMA完成回调，记录真实命令和时间，不自行推进驱动状态。
 * 断言针对可观察的需求：20ms转换等待、未消费样本不覆盖、消费后继续、
 * 正周期等待仍存在、故障冻结不自动循环。没有串口或真实硬件访问。
 */
#include "hal_stub.h"
#include "ms5837.c"
#include <assert.h>
#include <stdio.h>

typedef struct {uint8_t command; uint32_t tick;} CommandRecord;
typedef enum {PENDING_NONE = 0, PENDING_TX, PENDING_RX} PendingTransfer;

static uint32_t test_tick;
static DMA_HandleTypeDef fake_tx_dma;
static DMA_HandleTypeDef fake_rx_dma;
static I2C_HandleTypeDef fake_i2c;
static PendingTransfer pending_transfer;
static uint8_t *pending_buffer;
static uint16_t pending_length;
static uint8_t pending_command;
static uint8_t last_command;
static uint8_t last_conversion;
static uint32_t conversion_completed_tick;
static CommandRecord conversion_log[128];
static uint32_t conversion_count;
static uint32_t adc_command_count;
static uint32_t transfer_count;
static uint32_t pressure_read_count;
static uint16_t fake_prom[7];
static uint8_t inject_zero_adc;
static uint8_t hold_dma_completion;

uint32_t HAL_GetTick(void) {return test_tick;}
HAL_I2C_StateTypeDef HAL_I2C_GetState(I2C_HandleTypeDef *handle) {return handle->state;}
uint32_t HAL_I2C_GetError(I2C_HandleTypeDef *handle) {return handle->error;}
uint32_t TestHal_GetFlag(I2C_HandleTypeDef *handle, uint32_t flag)
{
    assert(flag == I2C_FLAG_BUSY);
    return (handle->state == HAL_I2C_STATE_BUSY) ? 1U : RESET;
}

HAL_StatusTypeDef HAL_I2C_Master_Transmit_DMA(I2C_HandleTypeDef *handle,
    uint16_t address, uint8_t *data, uint16_t length)
{
    assert(handle == &fake_i2c && address == MS5837_ADDR);
    assert(data != NULL && length == 1U);
    assert(pending_transfer == PENDING_NONE);
    assert(handle->state == HAL_I2C_STATE_READY);
    pending_transfer = PENDING_TX;
    pending_buffer = data;
    pending_length = length;
    pending_command = *data;
    if (*data == MS5837_D1_CMD || *data == MS5837_D2_CMD)
    {
        assert(conversion_count < sizeof(conversion_log) / sizeof(conversion_log[0]));
        conversion_log[conversion_count].command = *data;
        conversion_log[conversion_count].tick = test_tick;
        ++conversion_count;
    }
    if (*data == 0x00U)
    {
        /* 需求断言放在实际ADC命令入口：任何模式都不得提前读取ADC。 */
        assert(last_conversion == MS5837_D1_CMD || last_conversion == MS5837_D2_CMD);
        assert((uint32_t)(test_tick - conversion_completed_tick) >= 20U);
        ++adc_command_count;
    }
    handle->state = HAL_I2C_STATE_BUSY;
    ++transfer_count;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive_DMA(I2C_HandleTypeDef *handle,
    uint16_t address, uint8_t *data, uint16_t length)
{
    assert(handle == &fake_i2c && address == MS5837_ADDR);
    assert(data != NULL && (length == 2U || length == 3U));
    assert(pending_transfer == PENDING_NONE);
    assert(handle->state == HAL_I2C_STATE_READY);
    pending_transfer = PENDING_RX;
    pending_buffer = data;
    pending_length = length;
    handle->state = HAL_I2C_STATE_BUSY;
    ++transfer_count;
    return HAL_OK;
}

/* 模拟一次DMA真正完成，先释放HAL BUSY，再只通过公开回调通知驱动。 */
static void CompletePendingDma(void)
{
    if (hold_dma_completion != 0U || pending_transfer == PENDING_NONE) return;
    if (pending_transfer == PENDING_TX)
    {
        assert(pending_length == 1U && *pending_buffer == pending_command);
        last_command = pending_command;
        if (last_command == MS5837_D1_CMD || last_command == MS5837_D2_CMD)
        {
            last_conversion = last_command;
            conversion_completed_tick = test_tick;
        }
        pending_transfer = PENDING_NONE;
        fake_i2c.state = HAL_I2C_STATE_READY;
        MS5837_OnTxComplete(&fake_i2c);
    }
    else
    {
        if (pending_length == 2U)
        {
            assert(last_command >= 0xA0U && last_command <= 0xACU);
            uint32_t index = (last_command - 0xA0U) / 2U;
            pending_buffer[0] = (uint8_t)(fake_prom[index] >> 8);
            pending_buffer[1] = (uint8_t)fake_prom[index];
        }
        else
        {
            assert(last_command == 0x00U);
            uint32_t adc = (uint32_t)fake_prom[5] * 256U;
            if (last_conversion == MS5837_D1_CMD)
                adc = 9000000U + ++pressure_read_count * 1000U;
            if (inject_zero_adc != 0U) adc = 0U;
            pending_buffer[0] = (uint8_t)(adc >> 16);
            pending_buffer[1] = (uint8_t)(adc >> 8);
            pending_buffer[2] = (uint8_t)adc;
        }
        pending_transfer = PENDING_NONE;
        fake_i2c.state = HAL_I2C_STATE_READY;
        MS5837_OnRxComplete(&fake_i2c);
    }
}

/* 同一个毫秒内模拟多轮主循环；没有时间推进就不能绕过转换或周期等待。 */
static void PumpAtCurrentTick(void)
{
    for (uint32_t iteration = 0U; iteration < 64U; ++iteration)
    {
        MS5837_ProcessDMA();
        CompletePendingDma();
    }
}

/* 测试夹具模拟MCU冷启动。生产驱动没有新增复位/重启接口。 */
static void ResetFixture(void)
{
    memset(&ms5837, 0, sizeof(ms5837));
    memset(&fake_i2c, 0, sizeof(fake_i2c));
    memset(conversion_log, 0, sizeof(conversion_log));
    fake_i2c.hdmatx = &fake_tx_dma;
    fake_i2c.hdmarx = &fake_rx_dma;
    test_tick = 0U;
    pending_transfer = PENDING_NONE;
    pending_buffer = NULL;
    pending_length = 0U;
    pending_command = 0U;
    last_command = 0U;
    last_conversion = 0U;
    conversion_completed_tick = 0U;
    conversion_count = 0U;
    adc_command_count = 0U;
    transfer_count = 0U;
    pressure_read_count = 0U;
    inject_zero_adc = 0U;
    hold_dma_completion = 0U;
    const uint16_t coefficients[7] = {0U,40127U,36924U,23317U,23282U,33464U,28312U};
    memcpy(fake_prom, coefficients, sizeof(fake_prom));
    fake_prom[0] |= (uint16_t)MS5837_CalculateCRC(fake_prom) << 12;
}

/* 这里只观察样本是否发布，不清sample_ready；消费行为专门在各测试断言。 */
static void WaitForPublishedSample(void)
{
    uint32_t began = test_tick;
    while (ms5837.sample_ready == 0U && (uint32_t)(test_tick - began) < 4000U)
    {
        PumpAtCurrentTick();
        assert(MS5837_GetState() != MS5837_ERROR);
        if (ms5837.sample_ready != 0U) break;
        ++test_tick;
    }
    assert(ms5837.sample_ready != 0U);
    assert(MS5837_IsReady() == 1U);
}

static void TestZeroIntervalAndStartupValidation(void)
{
    ResetFixture();
    assert(MS5837_StartDMA(NULL, 2U, 0U) == HAL_ERROR);
    assert(MS5837_StartDMA(&fake_i2c, 0U, 0U) == HAL_ERROR);
    I2C_HandleTypeDef missing_dma = fake_i2c;
    missing_dma.hdmatx = NULL;
    assert(MS5837_StartDMA(&missing_dma, 2U, 0U) == HAL_ERROR);
    assert(MS5837_GetState() == MS5837_STOPPED);
    assert(transfer_count == 0U);
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_OK);
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_BUSY);
    assert(transfer_count == 0U); /* 接受启动不等于在Start入口阻塞读取。 */
    WaitForPublishedSample(); /* 必须实际走完复位/PROM/CRC/两轮基准后才发布。 */
    assert(pressure_read_count == 3U);
    assert(conversion_count == 6U);
}

static void TestBothConversionsKeepTwentyMsWait(void)
{
    ResetFixture();
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_OK);
    while (conversion_count == 0U && test_tick < 3000U)
    {
        PumpAtCurrentTick();
        if (conversion_count == 0U) ++test_tick;
    }
    assert(conversion_count == 1U);
    assert(conversion_log[0].command == MS5837_D1_CMD);
    assert(test_tick >= 1020U); /* 保留复位20ms和启动稳定1000ms。 */
    uint32_t d1_started = test_tick;
    test_tick = d1_started + 19U;
    PumpAtCurrentTick();
    assert(adc_command_count == 0U);
    test_tick = d1_started + 20U;
    PumpAtCurrentTick();
    assert(adc_command_count == 1U);
    assert(conversion_count == 2U);
    assert(conversion_log[1].command == MS5837_D2_CMD);
    uint32_t d2_started = test_tick;
    test_tick = d2_started + 19U;
    PumpAtCurrentTick();
    assert(adc_command_count == 1U);
    test_tick = d2_started + 20U;
    PumpAtCurrentTick();
    assert(adc_command_count == 2U);
    assert(MS5837_IsReady() == 0U); /* 两轮基准尚未全部完成。 */
    uint32_t baseline_conversions = conversion_count;
    test_tick += 19U;
    PumpAtCurrentTick();
    assert(conversion_count == baseline_conversions);
    ++test_tick;
    PumpAtCurrentTick();
    assert(conversion_count == baseline_conversions + 1U); /* 保留基准间隔20ms。 */
}

static void TestBackpressureAndImmediateContinuousRestart(void)
{
    ResetFixture();
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_OK);
    WaitForPublishedSample();
    MS5837_Sample expected = ms5837.sample;
    uint32_t previous_conversions = conversion_count;
    uint32_t previous_transfers = transfer_count;
    assert(MS5837_TakeSample(NULL) == 0U);
    test_tick += 2000U;
    PumpAtCurrentTick();
    assert(conversion_count == previous_conversions);
    assert(transfer_count == previous_transfers);
    assert(ms5837.sample.d1 == expected.d1 && ms5837.sample.d2 == expected.d2);
    assert(ms5837.sample.depth_cm_x100 == expected.depth_cm_x100);
    MS5837_Sample sample;
    assert(MS5837_TakeSample(&sample) == 1U);
    assert(sample.d1 == expected.d1 && sample.d2 == expected.d2);
    assert(sample.reference_x100 == expected.reference_x100);
    assert(sample.depth_cm_x100 == expected.depth_cm_x100);
    assert(MS5837_TakeSample(&sample) == 0U);
    uint32_t consumed_tick = test_tick;
    PumpAtCurrentTick();
    assert(conversion_count == previous_conversions + 1U);
    assert(conversion_log[conversion_count - 1U].command == MS5837_D1_CMD);
    assert(conversion_log[conversion_count - 1U].tick == consumed_tick);
    WaitForPublishedSample();
    assert(MS5837_TakeSample(&sample) == 1U);
    assert(sample.d1 != expected.d1); /* 下一轮发布新值，未消费时没有提前覆写。 */
    /* 快速连续边界：第二份样本没有额外持有，距其D1启动只有两段转换时间。
     * 若误保留1000ms周期，长背压后的第一次重启可能通过，但这里必定失败。
     */
    uint32_t fast_previous_conversions = conversion_count;
    uint32_t second_round_start = conversion_log[fast_previous_conversions - 2U].tick;
    assert((uint32_t)(test_tick - second_round_start) < 1000U);
    uint32_t fast_consumed_tick = test_tick;
    PumpAtCurrentTick();
    assert(conversion_count == fast_previous_conversions + 1U);
    assert(conversion_log[conversion_count - 1U].command == MS5837_D1_CMD);
    assert(conversion_log[conversion_count - 1U].tick == fast_consumed_tick);
}

static void TestPositiveIntervalStillWaits(void)
{
    ResetFixture();
    assert(MS5837_StartDMA(&fake_i2c, 2U, 1000U) == HAL_OK);
    WaitForPublishedSample();
    uint32_t previous_conversions = conversion_count;
    assert(previous_conversions >= 2U);
    uint32_t last_round_start = conversion_log[previous_conversions - 2U].tick;
    assert(conversion_log[previous_conversions - 2U].command == MS5837_D1_CMD);
    MS5837_Sample sample;
    assert(MS5837_TakeSample(&sample) == 1U);
    PumpAtCurrentTick();
    assert(conversion_count == previous_conversions);
    test_tick = last_round_start + 999U;
    PumpAtCurrentTick();
    assert(conversion_count == previous_conversions);
    test_tick = last_round_start + 1000U;
    PumpAtCurrentTick();
    assert(conversion_count == previous_conversions + 1U);
    assert(conversion_log[conversion_count - 1U].tick == test_tick);
}

/* 故障冻结的公开行为共用断言：等待再久也不发新传输、不自重标定或重启。 */
static void AssertFrozenFault(MS5837_Fault expected_cause)
{
    assert(MS5837_GetState() == MS5837_ERROR);
    assert(MS5837_IsReady() == 0U);
    MS5837_Error before;
    assert(MS5837_GetError(&before) == 1U && before.cause == expected_cause);
    uint32_t transfers_before = transfer_count;
    uint32_t conversions_before = conversion_count;
    for (uint32_t iteration = 0U; iteration < 5U; ++iteration)
    {
        test_tick += 1000U;
        PumpAtCurrentTick();
        assert(MS5837_GetState() == MS5837_ERROR);
    }
    assert(transfer_count == transfers_before && conversion_count == conversions_before);
    MS5837_Error after;
    assert(MS5837_GetError(&after) == 1U);
    assert(after.cause == before.cause && after.state == before.state);
    assert(after.status == before.status && after.hal_error == before.hal_error);
    MS5837_Sample sample;
    assert(MS5837_TakeSample(&sample) == 0U);
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_BUSY);
}

static void TestI2cFaultDoesNotRestart(void)
{
    ResetFixture();
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_OK);
    WaitForPublishedSample();
    fake_i2c.error = 0x04U;
    MS5837_OnI2CError(&fake_i2c);
    MS5837_ProcessDMA();
    AssertFrozenFault(MS5837_FAULT_I2C);
}

static void TestZeroAdcDoesNotRestart(void)
{
    ResetFixture();
    inject_zero_adc = 1U;
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_OK);
    while (MS5837_GetState() != MS5837_ERROR && test_tick < 3000U)
    {
        PumpAtCurrentTick();
        ++test_tick;
    }
    AssertFrozenFault(MS5837_FAULT_ADC_ZERO);
}

static void TestDmaTimeoutDoesNotRestart(void)
{
    ResetFixture();
    hold_dma_completion = 1U;
    assert(MS5837_StartDMA(&fake_i2c, 2U, 0U) == HAL_OK);
    PumpAtCurrentTick();
    assert(transfer_count == 1U && pending_transfer == PENDING_TX);
    test_tick += 100U;
    MS5837_ProcessDMA();
    AssertFrozenFault(MS5837_FAULT_TIMEOUT);
}

int main(void)
{
    TestZeroIntervalAndStartupValidation();
    TestBothConversionsKeepTwentyMsWait();
    TestBackpressureAndImmediateContinuousRestart();
    TestPositiveIntervalStillWaits();
    TestI2cFaultDoesNotRestart();
    TestZeroAdcDoesNotRestart();
    TestDmaTimeoutDoesNotRestart();
    puts("ms5837_continuous: 7 test groups passed");
    return 0;
}
