# RF-only FreeRTOS 调用链

本文描述 `freertos` 目录中 RF-only 固件的运行边界与调用顺序。该工程使用 WCH 闭源归档中的 RFRole/RFIP 实现收发，但**不启动 BLE 协议栈，也不链接 TMOS 调度器**。

## 1. 构建与裁剪链

```text
rf_basic/LIB/libCH58xBLE.a
        │
        ├─ CMake: ar x 到 <build>/rf_vendor_objects/
        ├─ 排除 tmos.o / tmos_hw.o / tmos_snv.o
        ├─ 排除 ble_task_scheduler.o / ble_task_process.o
        └─ 剩余 RFRole、RFIP 和共同射频底层对象直接参与链接
                   │
                   ├─ --gc-sections 删除未引用的 BLE/GAP/GATT section
                   └─ src/tmos_shim.c 补齐 RF 对象所需的无状态 TMOS ABI
```

`tmos_shim.c` 只提供 `tmos_memcpy`、`tmos_memset`、`tmos_memcmp`、`tmos_strlen`，以及 RF 底层共享的 `ble[0x3c]` 与 `fnGetClockCBs` 控制块。它不提供事件队列、软件定时器、消息池、`TMOS_SystemProcess` 或 BLE 调度入口。

`freertos/src/rf_main.c:11:RF_TEST_MODE` 控制：`1` 是 TX，`0` 是 RX。

```bash
cmake -S freertos -B build-rf
cmake --build build-rf
```

## 2. 启动与任务链

```text
复位
  └─ Startup_CH585_FreeRTOS.S
       └─ main()                                      [src/rf_main.c]
            ├─ HSECFG_Capacitance()
            ├─ SetSysClock()
            ├─ UART0 初始化（DEBUG 时）
            ├─ xTaskCreate(rf_task, priority=13)
            └─ vTaskStartScheduler()
                 ├─ SysTick_Handler() → xTaskIncrementTick()
                 └─ rf_task()
```

`rf_task()` 是一次性初始化任务。完成射频配置、启动统计定时器并启用 TX 的 TMR0 后，它调用 `vTaskDelete(NULL)`；后续周期工作由中断、RFRole 回调和 FreeRTOS 软件定时器承担。

## 3. 射频初始化链

```text
rf_task()
  ├─ RFRole_BasicInit(&config)
  │    ├─ 保存 rf_callback 与 processMask
  │    ├─ RFIP_RegInit()
  │    └─ 初始化 RF/IP、BB、LLE、RFEND 共用寄存器状态
  ├─ RFRole_SetParam(&rf_params)
  │    └─ 设置 Access Address、CRC 初值、2M PHY、间隔与稳定时间
  ├─ 初始化 tx_params / rx_params / DMA 缓冲区
  └─ 按 RF_TEST_MODE 分支
       ├─ TX: 配置并启用 TMR0
       └─ RX: rf_rx_start() → RFIP_SetRx(&rx_params)
```

`rfRoleConfig_t` 只订阅 `RX`、`RX_CRCERR`、`TX_FINISH` 和 `TIMEOUT` 状态。BLEB/BLEL IRQ、BLE 内存堆、SNV、RTC/TMR3 BLE 时钟及 BLE 协议初始化均不在此调用链中。

## 4. TX、RX 与统计链

### TX（`RF_TEST_MODE==TX`）

```text
TMR0 周期到达
  └─ TMR0_IRQHandler()
       ├─ 清除 TMR0_3_IT_CYC_END 标志
       ├─ 填充 tx_buf: 0x55、长度、递增序号、上次 RSSI
       └─ rf_tx_start(tx_buf)
            ├─ RFIP_SetTxStart()
            └─ RFIP_SetTxParm(&tx_params)
                 └─ RFRole/RFIP 完成实际发送
                      └─ rf_callback(RF_STATE_TX_FINISH, id)
                           └─ tx_count++
```

### RX（`RF_TEST_MODE==RX`）

```text
rf_task() → rf_rx_start() → RFIP_SetRx(&rx_params)
  └─ 收到有效帧
       └─ rf_callback(RF_STATE_RX, id)
            ├─ RFIP_ReadRssi()
            ├─ 更新 rssi、rssi_average、rx_count
            └─ rf_rx_start()             // 重新进入接收

  └─ CRC 错误
       └─ rf_callback(RF_STATE_RX_CRCERR, id) → rf_rx_start()
```

### 1 秒统计

```text
FreeRTOS Timer Service Task
  └─ stats_timer()，每 1000 ms
       ├─ taskENTER_CRITICAL()
       ├─ 快照并清零 tx_count / rx_count / rssi_average
       ├─ taskEXIT_CRITICAL()
       └─ printf("TX\\tRX\\tRSSI\\n")
```

临界区避免 `stats_timer()` 与 RF 回调/`TMR0_IRQHandler()` 在读取或清零统计变量时竞争；射频 ISR 与回调中不使用会阻塞的 FreeRTOS API。

## 5. 中断和时基职责

| 来源 | 入口 | 职责 | 与 TMOS/BLE 的关系 |
| --- | --- | --- | --- |
| SysTick | `SysTick_Handler` | FreeRTOS 500 Hz Tick、抢占调度 | 仅 FreeRTOS |
| TMR0 | `TMR0_IRQHandler` | TX 周期触发 | 仅 RF 示例 |
| RFRole/RFIP | 闭源 RF 路径 → `rf_callback` | RX、CRC 错误、发送完成状态 | 不经过 TMOS 应用调度 |
| 软件定时器 | Timer Service Task → `stats_timer` | 每秒打印统计 | 仅 FreeRTOS |

## 6. 禁止与审计项

不得重新引入以下内容，否则会恢复 TMOS/BLE 调度依赖：

- `CH58x_BLEInit()`、`BLE_LibInit()`；
- `TMOS_SystemProcess()`、`TMOS_ProcessEventRegister()`；
- `tmos_set_event()`、`tmos_start_*()`、`TMOS_Timer*()`；
- `ble_task_scheduler.S`、`ble_task_process.o`；
- BLEL/BLEB IRQ 使能、BLE SNV 或 BLE RTC/TMR3 时钟初始化。

建议每次构建后审计：

```bash
riscv-none-elf-nm -g build-rf/RF_Basic_FreeRTOS.elf | \
  rg 'TMOS_SystemProcess|TMOS_ProcessEventRegister|TMOS_Timer|BLE_LibInit'
```

该命令无输出即表明禁止符号未进入最终映像。
