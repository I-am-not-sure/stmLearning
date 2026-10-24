#include "soft_i2c.h"

typedef struct
{
    SoftI2C_HandleTypeDef *bus;
    uint32_t start_cycles;
    uint32_t timeout_cycles;
} SoftI2C_Transfer;

static HAL_StatusTypeDef EnableGPIOClock(GPIO_TypeDef *port)
{
    if (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }
    else if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); }
    else if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); }
    else if (port == GPIOF) { __HAL_RCC_GPIOF_CLK_ENABLE(); }
    else if (port == GPIOG) { __HAL_RCC_GPIOG_CLK_ENABLE(); }
    else if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
    else if (port == GPIOI) { __HAL_RCC_GPIOI_CLK_ENABLE(); }
    else { return HAL_ERROR; }
    return HAL_OK;
}

static void DelayHalfPeriod(const SoftI2C_HandleTypeDef *bus)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = bus->half_period_us * bus->cycles_per_us;
    /* Unsigned subtraction also handles the cycle counter wrapping. */
    while ((uint32_t)(DWT->CYCCNT - start) < cycles) { __NOP(); }
}

static void WriteSCL(const SoftI2C_HandleTypeDef *bus, GPIO_PinState state)
{
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, state);
}

static void WriteSDA(const SoftI2C_HandleTypeDef *bus, GPIO_PinState state)
{
    /* SET releases an open-drain output; it does not actively drive high. */
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, state);
}

static GPIO_PinState ReadSDA(const SoftI2C_HandleTypeDef *bus)
{
    return HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin);
}

static SoftI2C_Transfer MakeTransfer(SoftI2C_HandleTypeDef *bus, uint32_t timeout_ms)
{
    uint64_t cycles = (uint64_t)timeout_ms * 1000U * bus->cycles_per_us;
    SoftI2C_Transfer transfer = {
        .bus = bus,
        .start_cycles = DWT->CYCCNT,
        .timeout_cycles = (cycles > 0x7FFFFFFFULL) ? 0x7FFFFFFFU : (uint32_t)cycles
    };
    return transfer;
}

static HAL_StatusTypeDef RaiseSCL(SoftI2C_Transfer *transfer)
{
    SoftI2C_HandleTypeDef *bus = transfer->bus;
    WriteSCL(bus, GPIO_PIN_SET);
    /* Wait for the physical line, allowing a target to stretch the clock. */
    while (HAL_GPIO_ReadPin(bus->scl_port, bus->scl_pin) == GPIO_PIN_RESET)
    {
        if ((uint32_t)(DWT->CYCCNT - transfer->start_cycles) >= transfer->timeout_cycles)
        {
            return HAL_TIMEOUT;
        }
    }
    if ((uint32_t)(DWT->CYCCNT - transfer->start_cycles) >= transfer->timeout_cycles)
    {
        return HAL_TIMEOUT;
    }
    return HAL_OK;
}

static HAL_StatusTypeDef Stop(SoftI2C_Transfer *transfer)
{
    SoftI2C_HandleTypeDef *bus = transfer->bus;
    WriteSCL(bus, GPIO_PIN_RESET);
    WriteSDA(bus, GPIO_PIN_RESET);
    DelayHalfPeriod(bus);
    HAL_StatusTypeDef status = RaiseSCL(transfer);
    if (status == HAL_OK) { DelayHalfPeriod(bus); }
    /* Always release SDA, including the timeout path. */
    WriteSDA(bus, GPIO_PIN_SET);
    DelayHalfPeriod(bus);
    return status;
}

static HAL_StatusTypeDef Start(SoftI2C_Transfer *transfer)
{
    SoftI2C_HandleTypeDef *bus = transfer->bus;
    WriteSDA(bus, GPIO_PIN_SET);
    HAL_StatusTypeDef status = RaiseSCL(transfer);
    if (status != HAL_OK) { return status; }
    DelayHalfPeriod(bus);
    if (ReadSDA(bus) == GPIO_PIN_RESET) { return HAL_BUSY; }
    WriteSDA(bus, GPIO_PIN_RESET);
    DelayHalfPeriod(bus);
    WriteSCL(bus, GPIO_PIN_RESET);
    return HAL_OK;
}

static HAL_StatusTypeDef WriteByte(SoftI2C_Transfer *transfer, uint8_t value)
{
    SoftI2C_HandleTypeDef *bus = transfer->bus;
    HAL_StatusTypeDef status;
    for (uint8_t bit = 0; bit < 8U; bit++)
    {
        WriteSDA(bus, (value & 0x80U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        DelayHalfPeriod(bus);
        status = RaiseSCL(transfer);
        if (status != HAL_OK) { return status; }
        DelayHalfPeriod(bus);
        WriteSCL(bus, GPIO_PIN_RESET);
        value <<= 1;
    }

    /* Ninth clock: release SDA and sample the target's ACK. */
    WriteSDA(bus, GPIO_PIN_SET);
    DelayHalfPeriod(bus);
    status = RaiseSCL(transfer);
    if (status != HAL_OK) { return status; }
    DelayHalfPeriod(bus);
    GPIO_PinState ack = ReadSDA(bus);
    WriteSCL(bus, GPIO_PIN_RESET);
    return (ack == GPIO_PIN_RESET) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef ReadByte(
    SoftI2C_Transfer *transfer, uint8_t *value, uint8_t send_ack)
{
    SoftI2C_HandleTypeDef *bus = transfer->bus;
    uint8_t result = 0;
    HAL_StatusTypeDef status;
    /* SDA remains an open-drain output, released while the target drives it. */
    WriteSDA(bus, GPIO_PIN_SET);
    for (uint8_t bit = 0; bit < 8U; bit++)
    {
        DelayHalfPeriod(bus);
        status = RaiseSCL(transfer);
        if (status != HAL_OK) { return status; }
        DelayHalfPeriod(bus);
        result = (uint8_t)((result << 1) | (ReadSDA(bus) == GPIO_PIN_SET));
        WriteSCL(bus, GPIO_PIN_RESET);
    }

    /* ACK intermediate bytes; NACK the final byte before STOP. */
    WriteSDA(bus, send_ack ? GPIO_PIN_RESET : GPIO_PIN_SET);
    DelayHalfPeriod(bus);
    status = RaiseSCL(transfer);
    if (status != HAL_OK) { return status; }
    DelayHalfPeriod(bus);
    WriteSCL(bus, GPIO_PIN_RESET);
    WriteSDA(bus, GPIO_PIN_SET);
    *value = result;
    return HAL_OK;
}

static uint8_t ValidTransfer(
    const SoftI2C_HandleTypeDef *bus, uint16_t address,
    const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    return bus != NULL && bus->initialized != 0U &&
           address <= 0x7FU && data != NULL && size != 0U && timeout_ms != 0U;
}

HAL_StatusTypeDef SoftI2C_Init(SoftI2C_HandleTypeDef *bus)
{
    if (bus == NULL) { return HAL_ERROR; }
    bus->initialized = 0;
    if (bus->scl_pin == 0U || (bus->scl_pin & (bus->scl_pin - 1U)) != 0U ||
        bus->sda_pin == 0U || (bus->sda_pin & (bus->sda_pin - 1U)) != 0U ||
        (bus->scl_port == bus->sda_port && bus->scl_pin == bus->sda_pin))
    {
        return HAL_ERROR;
    }
    if (EnableGPIOClock(bus->scl_port) != HAL_OK ||
        EnableGPIOClock(bus->sda_port) != HAL_OK) { return HAL_ERROR; }
    if (bus->half_period_us == 0U) { bus->half_period_us = SOFT_I2C_DEFAULT_HALF_PERIOD_US; }
    if (bus->half_period_us > 1000U) { return HAL_ERROR; }
    bus->cycles_per_us = SystemCoreClock / 1000000U;
    if (bus->cycles_per_us == 0U) { return HAL_ERROR; }

    /* Enable the Cortex-M4 cycle counter; keep an existing counter's value. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
#ifdef DWT_CTRL_NOCYCCNT_Msk
    if ((DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) != 0U) { return HAL_ERROR; }
#endif
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    uint32_t before = DWT->CYCCNT;
    for (volatile uint32_t i = 0; i < 32U; i++) { __NOP(); }
    if (DWT->CYCCNT == before) { return HAL_ERROR; }

    /* Preload released levels before switching the GPIOs to open drain. */
    WriteSCL(bus, GPIO_PIN_SET);
    WriteSDA(bus, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pin = bus->scl_pin;
    HAL_GPIO_Init(bus->scl_port, &gpio);
    gpio.Pin = bus->sda_pin;
    HAL_GPIO_Init(bus->sda_port, &gpio);

    SoftI2C_Transfer transfer = MakeTransfer(bus, 100U);
    HAL_StatusTypeDef status = RaiseSCL(&transfer);
    if (status != HAL_OK) { return status; }
    DelayHalfPeriod(bus);
    /* If reset interrupted a read, up to nine clocks let the target release SDA. */
    for (uint8_t i = 0; i < 9U && ReadSDA(bus) == GPIO_PIN_RESET; i++)
    {
        WriteSCL(bus, GPIO_PIN_RESET);
        DelayHalfPeriod(bus);
        status = RaiseSCL(&transfer);
        if (status != HAL_OK) { WriteSDA(bus, GPIO_PIN_SET); return status; }
        DelayHalfPeriod(bus);
    }
    status = Stop(&transfer);
    if (status != HAL_OK) { return status; }
    if (ReadSDA(bus) == GPIO_PIN_RESET) { return HAL_BUSY; }
    bus->initialized = 1;
    return HAL_OK;
}

HAL_StatusTypeDef SoftI2C_Master_Transmit(
    SoftI2C_HandleTypeDef *bus, uint16_t address,
    const uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    if (!ValidTransfer(bus, address, data, size, timeout_ms)) { return HAL_ERROR; }
    SoftI2C_Transfer transfer = MakeTransfer(bus, timeout_ms);
    HAL_StatusTypeDef status = Start(&transfer);
    if (status == HAL_OK) { status = WriteByte(&transfer, (uint8_t)(address << 1)); }
    for (uint16_t i = 0; i < size && status == HAL_OK; i++)
    {
        status = WriteByte(&transfer, data[i]);
    }
    HAL_StatusTypeDef stop_status = Stop(&transfer);
    return (status == HAL_OK) ? stop_status : status;
}

HAL_StatusTypeDef SoftI2C_Master_Receive(
    SoftI2C_HandleTypeDef *bus, uint16_t address,
    uint8_t *data, uint16_t size, uint32_t timeout_ms)
{
    if (!ValidTransfer(bus, address, data, size, timeout_ms)) { return HAL_ERROR; }
    SoftI2C_Transfer transfer = MakeTransfer(bus, timeout_ms);
    HAL_StatusTypeDef status = Start(&transfer);
    if (status == HAL_OK) { status = WriteByte(&transfer, (uint8_t)((address << 1) | 1U)); }
    for (uint16_t i = 0; i < size && status == HAL_OK; i++)
    {
        status = ReadByte(&transfer, &data[i], (uint8_t)(i + 1U < size));
    }
    HAL_StatusTypeDef stop_status = Stop(&transfer);
    return (status == HAL_OK) ? stop_status : status;
}
