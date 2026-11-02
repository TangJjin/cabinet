/*
 * 文件用途：CM4 采集：RS485 Modbus RTU 温湿度轮询、水浸消抖，以及 OpenAMP 参数接收和采样发布。
 * 编译归属：CM4 应用。
 */
#include "bsp_sensor.h"
#include "bsp_board.h"
#include "bsp_ipc.h"
#include "FreeRTOS.h"
#include "task.h"

static UART_HandleTypeDef uart;
static DMA_HandleTypeDef dma;
static TaskHandle_t sensor_task_handle;
static uint8_t rx[32]; /* CM4 D2 SRAM, accessible to DMA2, no data cache. */
static volatile uint8_t rx_error;
static CabinetControl control;
static CabinetSample sample;
static uint32_t sensor_beat, water_beat;

static void receive(const CabinetMessage *m)
{
    if (m->kind != 2 || !cabinet_config_valid(&m->payload.control.config))
        return;
    taskENTER_CRITICAL();
    if (control.config.simulated != m->payload.control.config.simulated ||
        control.config.sensor_address != m->payload.control.config.sensor_address)
    {
        sample.online = 0;
        sample.failures = 0;
    }
    control = m->payload.control;
    taskEXIT_CRITICAL();
}

/* CM4 独占 USART6、DMA2 Stream0 和 RS485 方向脚；当前传感器参数为 9600/8N1。 */
static void hardware_init(void)
{
    GPIO_InitTypeDef g = {0};
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    g.Pin = GPIO_PIN_9 | GPIO_PIN_14;
    g.Mode = GPIO_MODE_AF_PP;
    g.Alternate = GPIO_AF7_USART6;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOG, &g);
    HAL_GPIO_WritePin(CAB_RS485_DE_PORT, CAB_RS485_DE_PIN, GPIO_PIN_RESET);
    g.Pin = CAB_RS485_DE_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CAB_RS485_DE_PORT, &g);
    g.Pin = CAB_WATER_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(CAB_WATER_PORT, &g);
    uart.Instance = USART6;
    uart.Init.BaudRate = 9600;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX;
    uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart.Init.OverSampling = UART_OVERSAMPLING_16;
    uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    if (HAL_UART_Init(&uart) != HAL_OK)
        bsp_fatal();
    dma.Instance = DMA2_Stream0;
    dma.Init.Request = DMA_REQUEST_USART6_RX;
    dma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    dma.Init.PeriphInc = DMA_PINC_DISABLE;
    dma.Init.MemInc = DMA_MINC_ENABLE;
    dma.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    dma.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    dma.Init.Mode = DMA_CIRCULAR;
    dma.Init.Priority = DMA_PRIORITY_HIGH;
    dma.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&dma) != HAL_OK)
        bsp_fatal();
    __HAL_LINKDMA(&uart, hdmarx, dma);
    HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
    HAL_NVIC_SetPriority(USART6_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
}

void DMA2_Stream0_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&dma);
}

void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&uart);
}

/* UART/DMA 中断只通知采集任务检查接收进度，实际报文校验留在任务中完成。 */
static void wake_sensor(void)
{
    BaseType_t wake = pdFALSE;
    if (sensor_task_handle)
        vTaskNotifyGiveFromISR(sensor_task_handle, &wake);
    portYIELD_FROM_ISR(wake);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *h, uint16_t count)
{
    if (h != &uart)
        return;
    if (count == sizeof(rx))
        rx_error = 1; /* Overlong frame, never accept a wrapped frame. */
    wake_sensor();
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *h)
{
    if (h == &uart)
    {
        rx_error = 1;
        wake_sensor();
    }
}

/* 一次 RTU 事务：组包、切发送、等待最后一位发完、启动 DMA 并切回接收，再判断帧结束和解析。 */
static int poll_sensor(uint8_t address, int16_t *temperature, uint16_t *humidity)
{
    uint8_t request[8];
    uint32_t start, last_change;
    unsigned count = 0, old_count = 0;
    cabinet_rtu_request(address, request);
    HAL_UART_AbortReceive(&uart);
    (void)ulTaskNotifyTake(pdTRUE, 0);
    rx_error = 0;
    HAL_GPIO_WritePin(CAB_RS485_DE_PORT, CAB_RS485_DE_PIN, GPIO_PIN_SET);
    if (HAL_UART_Transmit(&uart, request, sizeof(request), 30) != HAL_OK)
    {
        HAL_GPIO_WritePin(CAB_RS485_DE_PORT, CAB_RS485_DE_PIN, GPIO_PIN_RESET);
        return -1;
    }
    /* HAL transmit waits for TC: the last stop bit has left the pin. */
    if (HAL_UARTEx_ReceiveToIdle_DMA(&uart, rx, sizeof(rx)) != HAL_OK)
    {
        HAL_GPIO_WritePin(CAB_RS485_DE_PORT, CAB_RS485_DE_PIN, GPIO_PIN_RESET);
        return -1;
    }
    __HAL_DMA_DISABLE_IT(&dma, DMA_IT_HT);
    HAL_GPIO_WritePin(CAB_RS485_DE_PORT, CAB_RS485_DE_PIN, GPIO_PIN_RESET);
    start = last_change = HAL_GetTick();
    /* 最多等待约 250 ms；按 DMA 写入数量判断新数据，至少 5 ms 无增长后结束当前收帧。 */
    while (HAL_GetTick() - start < 250 && !rx_error)
    {
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(2));
        count = sizeof(rx) - __HAL_DMA_GET_COUNTER(&dma);
        if (count != old_count)
        {
            old_count = count;
            last_change = HAL_GetTick();
        }
        /* 5 ms silence exceeds 3.5 characters at 9600, 8N1. */
        if (count && HAL_GetTick() - last_change >= 5)
            break;
    }
    HAL_UART_AbortReceive(&uart);
    __DMB();
    return rx_error ? -1 : cabinet_rtu_decode(address, rx, count, temperature, humidity);
}

/* 每秒取一次控制快照；模拟模式直接生成值，现场模式才访问 RS485。连续三个失败周期判离线。 */
static void sensor_task(void *arg)
{
    TickType_t next = xTaskGetTickCount();
    (void)arg;
    for (;;)
    {
        CabinetControl c;
        int16_t temperature = 0;
        uint16_t humidity = 0;
        int result;
        taskENTER_CRITICAL();
        c = control;
        taskEXIT_CRITICAL();
        if (c.config.simulated)
        {
            temperature = c.sim_temperature;
            humidity = c.sim_humidity;
            result = c.sim_offline ? -1 : 0;
        }
        else
            result = poll_sensor((uint8_t)c.config.sensor_address, &temperature, &humidity);
        taskENTER_CRITICAL();
        /* Discard an in-flight reading if its acquisition source changed. */
        if (c.config.simulated != control.config.simulated ||
            c.config.sensor_address != control.config.sensor_address)
        {
            sensor_beat = HAL_GetTick();
            taskEXIT_CRITICAL();
            vTaskDelayUntil(&next, pdMS_TO_TICKS(1000));
            continue;
        }
        sample.simulated = (uint8_t)c.config.simulated;
        if (!result)
        {
            sample.temperature = temperature;
            sample.humidity = humidity;
            sample.failures = 0;
            sample.online = 1;
            sample.last_good_ms = HAL_GetTick();
        }
        else
        {
            if (sample.failures < 255)
                ++sample.failures;
            if (sample.failures >= 3)
                sample.online = 0;
        }
        sensor_beat = HAL_GetTick();
        taskEXIT_CRITICAL();
        vTaskDelayUntil(&next, pdMS_TO_TICKS(1000));
    }
}

/* 水浸独立于 Modbus：每 20 ms 取样，候选电平稳定 200 ms 后才更新 sample.water。 */
static void water_task(void *arg)
{
    uint8_t candidate = 0;
    uint32_t since = 0;
    (void)arg;
    for (;;)
    {
        uint8_t raw;
        taskENTER_CRITICAL();
        raw = control.config.simulated
                  ? control.sim_water
                  : (HAL_GPIO_ReadPin(CAB_WATER_PORT, CAB_WATER_PIN) == GPIO_PIN_RESET);
        if (raw != candidate)
        {
            candidate = raw;
            since = HAL_GetTick();
        }
        if (HAL_GetTick() - since >= 200)
            sample.water = candidate;
        water_beat = HAL_GetTick();
        taskEXIT_CRITICAL();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/* CM4 通信任务每约 5 ms 轮询接收；每约 100 ms 复制采样快照并向 CM7 发布，避免传递本地指针。 */
static void ipc_task(void *arg)
{
    uint32_t last = 0;
    (void)arg;
    if (bsp_ipc_init(receive))
        bsp_fatal();
    for (;;)
    {
        bsp_ipc_poll();
        if (HAL_GetTick() - last >= 100)
        {
            CabinetMessage m = {0};
            uint32_t now = HAL_GetTick();
            last = now;
            m.magic = 0x4341424D;
            m.protocol = CABINET_PROTOCOL;
            m.kind = 1;
            m.size = sizeof(m);
            taskENTER_CRITICAL();
            sample.uptime_ms = now;
            sample.firmware_version = CABINET_FW_VERSION;
            ++sample.sequence;
            sample.tasks_ok = (now - sensor_beat < 2000 && now - water_beat < 500) ? 3 : 0;
            m.payload.sample = sample;
            taskEXIT_CRITICAL();
            (void)bsp_ipc_send(&m);
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

/**
  * @brief Freertos任务创建函数
 */
void bsp_sensor_start(void)
{
    cabinet_config_default(&control.config);
    control.sim_temperature = 2500;
    control.sim_humidity = 5000;
    hardware_init();
    sensor_beat = water_beat = HAL_GetTick();
    if (xTaskCreate(sensor_task, "rs485", 768, 0, 2, &sensor_task_handle) !=
            pdPASS || //每秒采集温湿度，接收、校验、处理超时
        xTaskCreate(water_task, "water", 256, 0, 2, 0) !=
            pdPASS || //约每 20 ms 检查水浸，做 200 ms 消抖
        xTaskCreate(ipc_task, "openamp", 1024, 0, 3, 0) !=
            pdPASS //处理核间消息，每 100 ms 发布采样快照
    )
        bsp_fatal();
}
