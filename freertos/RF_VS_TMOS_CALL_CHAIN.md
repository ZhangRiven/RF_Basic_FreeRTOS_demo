# RF-only FreeRTOS 与原 TMOS rf_basic 调用链对比

本文对比原始工程 [`rf_basic`](../rf_basic) 与移植工程 [`freertos`](.) 的实际调用关系。重点是区分两种含义不同的“TMOS”：

- 原工程使用 **TMOS 调度器** 执行应用事件、定时器和 BLE 协议栈工作；
- RF-only 工程仍复用闭源归档内的 RFRole/RFIP 代码，但没有链接 TMOS 调度对象，也没有调用 TMOS 调度 API。

## 1. 总体差异

```text
原 rf_basic
═══════════
main
 ├─ CH58x_BLEInit() → BLE_LibInit() → TMOS_Init()
 ├─ HAL_Init()       → TMOS_ProcessEventRegister(HAL_ProcessEvent)
 ├─ RFRole_Init()    → TMOS_ProcessEventRegister(RFRole_ProcessEvent)
 └─ while (1) TMOS_SystemProcess()
       ├─ 取出到期 timer / pending event
       ├─ 调用 HAL_ProcessEvent()
       ├─ 调用 RFRole_ProcessEvent()
       └─ 驱动 BLE 主机/控制器相关任务

RF-only FreeRTOS
═══════════════
main
 ├─ 创建 rf_task
 └─ vTaskStartScheduler()
       ├─ SysTick → FreeRTOS 内核调度
       ├─ rf_task（仅负责一次性 RF 初始化）
       ├─ Timer Service Task → stats_timer()
       ├─ TMR0 IRQ → rf_tx_start()                 [TX]
       └─ RFRole/RFIP 回调 → rf_callback()         [TX/RX 状态]
```

核心变化是：**TMOS 的事件位图和轮询主循环被 FreeRTOS 的任务、软件定时器和硬件 ISR 替代。**

## 2. 启动链对照

| 阶段 | TMOS rf_basic | RF-only FreeRTOS | 影响 |
| --- | --- | --- | --- |
| 时钟 | `HSECFG_Capacitance()` → `SetSysClock()` | 相同 | 系统主时钟保持一致。 |
| BLE/RF 库初始化 | `CH58x_BLEInit()` → `BLE_LibInit(&cfg)` | 无 | 不创建 BLE 内存堆、SNV、MAC、BLE 时钟或协议栈任务。 |
| HAL 初始化 | `HAL_Init()` 注册 HAL TMOS 任务并配置 RTC/TMR3 | 无 BLE HAL 初始化 | 不建立 TMOS RTC 时钟或 BLE 校准循环。 |
| RF 初始化 | `RFRole_Init()` 在裸机上下文中运行 | `rf_task()` 在 FreeRTOS 任务上下文中运行 | RF 参数及 RFRole API 保持，但不登记 TMOS task ID。 |
| 主调度 | `Main_Circulation()` 无限调用 `TMOS_SystemProcess()` | `vTaskStartScheduler()` 不返回 | 调度所有权由 TMOS 切换为 FreeRTOS。 |

原始主链可概括为：

```text
RF_main.c: main()
  → CH58x_BLEInit()
      → SysTick 配置后关闭 SysTick IRQ
      → BLE_LibInit()
      → TMOS_Init()
  → HAL_Init()
  → RFRole_Init()
  → Main_Circulation()
      → TMOS_SystemProcess() forever
```

移植后主链：

```text
rf_main.c: main()
  → HSECFG_Capacitance() / SetSysClock()
  → xTaskCreate(rf_task)
  → vTaskStartScheduler()
      → FreeRTOS SysTick 和 SWI 上下文切换机制开始运行
      → rf_task()
```

## 3. RF 初始化链对照

两套工程保留的射频配置在逻辑上等价：发射功率 4 dBm、Access Address `0x71762345`、CRC 初值 `0x555555`、2M PHY、TX DMA 缓冲区 64 字节、RX DMA 缓冲区 264 字节。

```text
原 RFRole_Init()                         新 rf_task()
────────────────                         ──────────────
TMOS_ProcessEventRegister(...)           无 task ID
        │                                         │
RFRole_BasicInit(&conf)                  RFRole_BasicInit(&config)
        │                                         │
RFRole_SetParam(&gParm)                  RFRole_SetParam(&rf_params)
        │                                         │
准备 gTxParam / gRxParam                 准备 tx_params / rx_params
        │                                         │
BLEB/BLEL IRQ enable                     不使能 BLEB/BLEL IRQ
        │                                         │
TMOS event / timer 注册                  按 TX 或 RX 直接启动 RF
```

原工程的 `rfTaskID` 的唯一用途，是让 `RFRole_ProcessEvent()` 通过 TMOS 接收三类应用事件：消息、启动 RX、每秒统计。移植后没有等价的 task ID；这些动作分别直接执行或映射为 FreeRTOS 设施。

## 4. 原 TMOS 事件到 FreeRTOS 的映射

| 原事件/机制 | 原始调用链 | FreeRTOS 对应链 | 说明 |
| --- | --- | --- | --- |
| `RF_START_RX_EVENT` | `tmos_set_event(rfTaskID, ...)` → `TMOS_SystemProcess()` → `RFRole_ProcessEvent()` → `rf_rx_start()` | `rf_task()` → `rf_rx_start()` | RX 初始化不必延迟到调度轮询。 |
| `RF_TEST_TX_EVENT` | `tmos_start_reload_task(..., 1000 ms)` → `RFRole_ProcessEvent()` | Timer Service Task → `stats_timer()` | 每秒打印并清零 TX/RX/RSSI 统计。 |
| `RF_TEST_RX_EVENT` | 同上 | 同上 | TX/RX 共用同一统计软件定时器。 |
| `SYS_EVENT_MSG` | `tmos_msg_receive()` → `tmos_msg_deallocate()` | 无 | RF-only 应用不创建 TMOS 消息，故整条消息队列链移除。 |
| `HAL_KEY_EVENT` | 100 ms TMOS 定时器 → `HAL_KeyPoll()` → 重启 timer | 当前 RF-only 目标未启用 | 若后续需要，应使用 FreeRTOS 软件定时器或周期任务。 |
| `LED_BLINK_EVENT` | TMOS 事件 → `HalLedUpdate()` | 当前 RF-only 目标未启用 | 同上。 |
| `HAL_REG_INIT_EVENT` | TMOS 定时器 → `BLE_RegInit()`/32K 校准 | 移除 | 该链服务于 BLE/RTC 校准，RF-only 不启动它。 |

## 5. TX 路径差异

TX 数据格式和发包周期保持原例程含义：`0x55`、长度、递增序号和上一次 RSSI；TMR0 周期为 `GetSysClock() / 8000`。

```text
原工程
TMR0_IRQHandler()
  → 填充 TxBuf
  → rf_tx_start(TxBuf)
       → RFIP_SetTxStart()
       → RFIP_SetTxParm(&gTxParam)
  → RF_ProcessCallBack(RF_STATE_TX_FINISH)
       → gTxCount++

RF-only 工程
TMR0_IRQHandler()
  → 填充 tx_buf
  → rf_tx_start(tx_buf)
       → RFIP_SetTxStart()
       → RFIP_SetTxParm(&tx_params)
  → rf_callback(RF_STATE_TX_FINISH)
       → tx_count++
```

因此发包触发仍是硬件 TMR0 ISR，**不是** FreeRTOS 软件定时器；这样不改变原例程的硬件周期与中断时序。区别只在于每秒统计从 TMOS reload timer 改为了 FreeRTOS software timer。

## 6. RX 路径差异

```text
原工程（RX 模式）
RFRole_Init()
  → tmos_set_event(RF_START_RX_EVENT)
  → TMOS_SystemProcess()
  → RFRole_ProcessEvent()
  → rf_rx_start() → RFIP_SetRx()
       ↓
RF_ProcessCallBack(RF_STATE_RX)
  → RFIP_ReadRssi()
  → 更新 gRssi / gRssiAverage / gRxCount
  → rf_rx_start() 重新接收

RF-only 工程（RF_TEST_MODE=0）
rf_task()
  → rf_rx_start() → RFIP_SetRx()
       ↓
rf_callback(RF_STATE_RX)
  → RFIP_ReadRssi()
  → 更新 rssi / rssi_average / rx_count
  → rf_rx_start() 重新接收
```

RX 回调中的核心射频操作未迁移至任务队列，仍在 RFRole 通知路径中立即重新启动接收。这与原例程保持相同的低延迟策略；回调中不得加入会阻塞或长时间执行的 FreeRTOS API。

## 7. 时基、IRQ 与上下文对照

| 功能 | TMOS rf_basic | RF-only FreeRTOS |
| --- | --- | --- |
| 内核调度时基 | RTC/TMR3 通过 `TMOS_TimerInit()` 供 TMOS timer 使用 | SysTick 以 `configTICK_RATE_HZ`（当前 500 Hz）供 FreeRTOS 使用 |
| 应用 1 秒统计 | TMOS reload task | FreeRTOS software timer |
| 周期 TX | TMR0 IRQ | TMR0 IRQ，不变 |
| BLEB/BLEL | `RFRole_Init()` 中显式使能 | 不在应用代码中使能 |
| BLE 内部上下文 | `ble_task_scheduler.S`、ECALL/LLE 机制 | 不参与最终链接 |
| 普通 IRQ 入口 | 原 startup 直接向量 | FreeRTOS startup 的统一中断入口和独立 ISR 栈 |

FreeRTOS 工程中 `TMR0_IRQHandler()` 位于 `.highcode`，由启动代码的统一中断入口调度；不要为它额外添加原裸机例程的 `__INTERRUPT` 属性。

## 8. 闭源库复用边界

`libCH58xBLE.a` 是 RF 与 BLE 共用的归档，不链接整个归档会丢失 RFRole 所在对象；完整链接则会拉入 TMOS 调度器。工程在配置阶段解包该归档，并显式排除：

```text
tmos.o
tmos_hw.o
tmos_snv.o
ble_task_scheduler.o
ble_task_process.o
```

剩余对象可提供 `RFRole_BasicInit`、`RFRole_SetParam`、`RFIP_SetTxStart`、`RFIP_SetTxParm`、`RFIP_SetRx` 与所需 RF/IP/BB/LLE/RFEND 底层代码。链接器的 `--gc-sections` 再剔除这些对象中没有调用到的 BLE 协议 section。

因为 RF 代码共享了少量原本定义在 `tmos.o` 中的全局控制块，`tmos_shim.c` 提供最小二进制兼容定义。它不是 TMOS 实现，且没有控制流进入 TMOS。

## 9. 验证两种工程的边界

对 RF-only ELF 执行：

```bash
riscv-none-elf-nm -g build-rf/RF_Basic_FreeRTOS.elf | \
  rg 'TMOS_SystemProcess|TMOS_ProcessEventRegister|TMOS_Timer|tmos_(set_event|start|stop|msg_)|BLE_LibInit'
```

预期结果是**无输出**。若出现任何符号，通常表示 CMake 的排除列表被修改，或新的调用间接引用了被排除的 TMOS/BLE 对象；应先恢复 RF-only 边界，再进行链接。
