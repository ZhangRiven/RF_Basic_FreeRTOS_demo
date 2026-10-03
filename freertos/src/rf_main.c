/* FreeRTOS RF-only port of the WCH rf_basic example. */
#include "CH58x_common.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "wchrf.h"

#define MODE_RX 0
#define MODE_TX 1
#ifndef RF_TEST_MODE
#define RF_TEST_MODE MODE_TX
#endif
#define TEST_DATA_LEN 4
#define TEST_FREQUENCY 16
#define RF_DEVICE_PERIOD 8000

static rfRoleParam_t rf_params;
static rfipTx_t tx_params;
static rfipRx_t rx_params;
static __attribute__((aligned(4))) uint8_t tx_buf[64];
static __attribute__((aligned(4))) uint8_t rx_buf[264];
static volatile int8_t rssi;
static volatile int8_t rssi_average;
static volatile uint32_t tx_count;
static volatile uint32_t rx_count;

static void rf_rx_start(void)
{
    rx_params.frequency = TEST_FREQUENCY;
    rx_params.timeOut = 0;
    RFIP_SetRx(&rx_params);
}

static void rf_tx_start(uint8_t *buffer)
{
    RFIP_SetTxStart();
    tx_params.frequency = TEST_FREQUENCY;
    tx_params.txDMA = (uint32_t)buffer;
    RFIP_SetTxParm(&tx_params);
}

__HIGH_CODE
static void rf_callback(rfRole_States_t state, uint8_t id)
{
    (void)id;
    if (state & RF_STATE_RX) {
        rssi = RFIP_ReadRssi();
        rssi_average = rssi_average ? (rssi + rssi_average) / 2 : rssi;
        rx_count++;
#if RF_TEST_MODE == MODE_RX
        rf_rx_start();
#endif
    }
    if ((state & RF_STATE_RX_CRCERR) && RF_TEST_MODE == MODE_RX) rf_rx_start();
    if (state & RF_STATE_TX_FINISH) tx_count++;
}

__HIGH_CODE
void TMR0_IRQHandler(void)
{
    if (TMR0_GetITFlag(TMR0_3_IT_CYC_END)) {
        TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
#if RF_TEST_MODE == MODE_TX
        tx_buf[0] = 0x55;
        tx_buf[1] = TEST_DATA_LEN;
        tx_buf[2]++;
        tx_buf[3] = rssi;
        rf_tx_start(tx_buf);
#endif
    }
}

static void stats_timer(TimerHandle_t timer)
{
    uint32_t tx, rx;
    int8_t avg;
    (void)timer;
    taskENTER_CRITICAL();
    tx = tx_count; rx = rx_count; avg = rssi_average;
    tx_count = 0; rx_count = 0; rssi_average = 0;
    taskEXIT_CRITICAL();
    printf("%lu\t%lu\t%d\n", (unsigned long)tx, (unsigned long)rx, avg);
}

static void rf_task(void *argument)
{
    rfRoleConfig_t config = {0};
    TimerHandle_t stats;
    (void)argument;
    config.TxPower = LL_TX_POWEER_4_DBM;
    config.rfProcessCB = rf_callback;
    config.processMask = RF_STATE_RX | RF_STATE_RX_CRCERR | RF_STATE_TX_FINISH | RF_STATE_TIMEOUT;
    RFRole_BasicInit(&config);
    rf_params.accessAddress = 0x71762345;
    rf_params.crcInit = 0x555555;
    rf_params.properties = LLE_MODE_PHY_2M;
    rf_params.sendInterval = 1999 * 2;
    rf_params.sendTime = 20 * 2;
    RFRole_SetParam(&rf_params);
    tx_params.accessAddress = rf_params.accessAddress;
    tx_params.crcInit = rf_params.crcInit;
    tx_params.properties = rf_params.properties;
    tx_params.sendCount = 1;
    tx_params.txDMA = (uint32_t)tx_buf;
    rx_params.accessAddress = rf_params.accessAddress;
    rx_params.crcInit = rf_params.crcInit;
    rx_params.properties = rf_params.properties;
    rx_params.rxDMA = (uint32_t)rx_buf;
    rx_params.rxMaxLen = TEST_DATA_LEN;
#if RF_TEST_MODE == MODE_RX
    rf_rx_start();
#else
    TMR0_TimerInit(GetSysClock() / RF_DEVICE_PERIOD);
    TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
    PFIC_SetPriority(TMR0_IRQn, 0x80);
    PFIC_EnableIRQ(TMR0_IRQn);
#endif
    stats = xTimerCreate("rfstats", pdMS_TO_TICKS(1000), pdTRUE, NULL, stats_timer);
    configASSERT(stats != NULL);
    xTimerStart(stats, 0);
    vTaskDelete(NULL);
}

int main(void)
{
    HSECFG_Capacitance(HSECap_18p);
    SetSysClock(SYSCLK_FREQ);
#ifdef DEBUG
    GPIOA_SetBits(GPIO_Pin_14);
    GPIOPinRemap(ENABLE, RB_PIN_UART0);
    GPIOA_ModeCfg(GPIO_Pin_15, GPIO_ModeIN_PU);
    GPIOA_ModeCfg(GPIO_Pin_14, GPIO_ModeOut_PP_5mA);
    UART0_DefInit();
    UART0_BaudRateCfg(921600);
#endif
    printf("RF Basic FreeRTOS (no BLE/TMOS scheduler)\n");
    xTaskCreate(rf_task, "rf", 384, NULL, configMAX_PRIORITIES - 2, NULL);
    vTaskStartScheduler();
    for (;;) { }
}
