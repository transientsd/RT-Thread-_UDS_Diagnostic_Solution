# 基于 RT-Thread 的车载 CAN 诊断与 IAP 固件升级系统 (V2.0)

![Language](https://img.shields.io/badge/Language-C99-blue.svg)
![MCU](https://img.shields.io/badge/MCU-STM32F103C8T6-007EC6.svg)
![RTOS](https://img.shields.io/badge/RTOS-RT--Thread-2B993A.svg)
![Protocol](https://img.shields.io/badge/Protocol-ISO--15765--2%20%7C%20ISO--14229-orange.svg)
![License](https://img.shields.io/badge/License-Apache%202.0-green.svg)

本项目是一个遵循汽车电子分层解耦思想设计的 **轻量级车规 CAN 诊断终端与安全 IAP 固件升级系统**。项目使用rtthread架构，从零手写实现了 **ISO 15765-2 (ISO-TP) 网络层传输协议**、**ISO 14229 (UDS) 诊断服务层**，并开发了脱离 OS 依赖的 **W25Q64 存储芯片复杂设备驱动 (CDD)**。

在 V2 版本中，系统打通了 **裸机 Bootloader + RT-Thread App 双工程 Flash 分区架构**，实现了从总线诊断、掉电持久化、升级状态握手到内核级向量重定位跳转的完整技术闭环。

---

## 目录

- [系统架构设计](#系统架构设计)
- [物理内存与存储空间规划](#物理内存与存储空间规划)
- [核心功能与技术亮点](#核心功能与技术亮点)
- [UDS 诊断服务支持清单](#uds-诊断服务支持清单)
- [关键测试与运行日志](#关键测试与运行日志)
- [工程构建与烧录指引](#工程构建与烧录指引)

---

## 系统架构设计

系统严格遵循单向依赖与分层解耦原则：

```text
+-----------------------------------------------------------------------+
|                        Application Layer (UDS)                        |
|       0x10 (会话控制)  |  0x11 (复位)  |  0x22 (读DID)  |  0x2E (写DID)   |
+-----------------------------------------------------------------------+
                                   ▲
                                   │ 结构化诊断报文
                                   ▼
+-----------------------------------------------------------------------+
|                       Network Layer (ISO-TP)                          |
|         ISO 15765-2 传输协议状态机 (SF / FF / CF / FC)                  |
|         - 多帧接收防越界机制                                             |
|         - SN 连续帧序列号校验与丢包止损                                  |
+-----------------------------------------------------------------------+
                                   ▲
                                   │ 环形缓冲区 (RingBuffer) + 信号量同步唤醒
                                   ▼
+-----------------------------------------------------------------------+
|                        Driver / CDD Layer                             |
|  drv_can.c: CAN 500kbps 中断驱动 + 临界区并发保护                      |
|  cdd_w25q64.c: 去 OS 化的外部 Flash 复杂驱动 (跨页拆包 / BSY等待)        |
|  bsp_internal_flash.c: 内部 Flash 硬件页擦除与半字编程 (IAP 核心)       |
+-----------------------------------------------------------------------+
                                   ▲
                                   │ 物理外设交互 (CAN / SPI / Internal Flash)
                                   ▼
+-----------------------------------------------------------------------+
|                      Hardware Layer (STM32F103)                       |
|         Cortex-M3 内核 | TJA1050 收发器 | W25Q64 SPI Flash (8MB)       |
+-----------------------------------------------------------------------+
```

---

## 物理内存与存储空间规划

系统将 64KB 内部 Flash 与 8MB 外部 Flash 划分为明确的职能区域，实现代码与数据的物理隔离：

### 1. STM32F103 内部 Flash 分区（单页大小 1KB）

| 区域 | 地址范围 | 大小 | 运行载体 | 职能描述 |
| :--- | :--- | :--- | :--- | :--- |
| **Bootloader 分区** | `0x08000000 ~ 0x08003FFF` | 16 KB | 纯裸机 (Bare-Metal) | 上电入口、升级标志检测、内部 Flash 擦写与内核跳转 |
| **Application 分区** | `0x08004000 ~ 0x0800FFFF` | 48 KB | RT-Thread OS | 跑主业务、CAN 协议栈、UDS 诊断与持久化逻辑 |

### 2. 外部 SPI Flash (W25Q64) 分区（扇区大小 4KB）

| 区域 | 物理地址范围 | 大小 | 用途 |
| :--- | :--- | :--- | :--- |
| **升级标志扇区** | `0x000000 ~ 0x000FFF` | 4 KB | 存储升级握手标志（`0xA5`：待升级；`0xFF`：正常态） |
| **标定持久化区** | `0x001000 ~ 0x001FFF` | 4 KB | 专供 UDS 0x2E/0x22 读写掉电持久化标定参数（如 DID `0xF199`） |
| **固件暂存区** | `0x002000 ~ 0x01FFFF` | 120 KB | 暂存待更新固件二进制镜像，防变砖保护隔离区 |

---

## 核心功能与技术亮点

### 1. 去 OS 化的存储 CDD 架构（100% 跨工程复用）

- **规避框架臃肿**：放弃占用大量内存的通用文件系统与 OS 设备抽象框架，采用纯 C 语言编写 `cdd_w25q64.c`。
- **物理缺陷防御（跨页自动拆分）**：针对 w25q64 SPI Flash 单次页编程（256字节）跨页发生“地址回卷覆盖”的硬件特性，在 CDD 内部自研了动态边界步长算法：`StepSize = min(256 - (Addr & 0xFF), RemainingLength)`。对上层业务完全透明，支持任意起始地址、任意大块数据的无缝安全写入。
- **双端复用**：同一套 CDD 驱动源码，零修改地同时编译进 RT-Thread App 工程与 16KB 裸机 Bootloader 工程。

### 2. 车规级传输层（ISO-TP）鲁棒性设计

- **多帧收发支持**：手写单帧 (SF)、首帧 (FF)、连续帧 (CF) 的组包解析与流控帧 (FC) 的主动下发。
- **内存防溢出防御**：在状态机处理 CF 帧时，精准计算 `remaining` 剩余长度，严格比对 SN（1~15 轮转），一旦发现序号错乱或超出最大接收长度，立即丢弃当前报文并复位状态机，防止内存越界。
- **底层时序排雷**：在开发 SPI 驱动时，排查出标准库/HAL 库在发送完 1 Byte 后，SPI 总线 `BSY` 标志位尚未清零，导致下发下一帧时发生数据错位（读回 0xFF）的致命 Bug，通过在驱动层增加硬性轮询等待，实现稳定通信。

### 3. 内核级安全跳转与 IAP 环回自测机制

- **洁净跳转交接体系**：Bootloader 向 App 移交 CPU 控制权前，严格执行四步操作：
  1. 调用 `__disable_irq()` 关闭全局中断；
  2. 彻底清零并关闭内核 SysTick 滴答定时器，防止跳入 App 瞬间发生未初始化中断导致的 HardFault；
  3. 通过内联汇编 `MSR MSP, r0` 将 CPU 主堆栈指针强置为 App 栈顶地址（`0x08004000` 处的内容）；
  4. App 启动首行通过 `SCB->VTOR = 0x08004000` 将中断向量表基址迁移至自身偏移区，完成向 `Reset_Handler` 的平滑过渡。
- **自产自销环回自测验证**：无需复杂上位机脚本，App 将自身头部的 1KB 镜像数据暂存至 W25Q64 并置位升级标志；Bootloader 捕获标志后，真实执行对内部 Flash Page 16 (`0x08004000`) 的物理页擦除，随后从 W25Q64 读取数据完整烧写入内部 Flash，成功恢复并拉起 RT-Thread App，100% 验证底层擦写驱动与跳转闭环。

---

## UDS 诊断服务支持清单

| SID | 服务名称 | 子功能 / 标定参数 | 响应逻辑与特性 |
| :--- | :--- | :--- | :--- |
| `0x10` | 诊断会话控制 (DSC) | 01-Default / 02-Programming / 03-Extended | 支持会话状态机转移，返回 P2 (50ms) 与 P2* (5000ms) 定时器参数 |
| `0x11` | ECU 复位服务 (ER) | 具备编程会话前置条件防御，支持通过软复位衔接固件升级 |
| `0x22` | 按标识符读取数据 (RDBI) | 支持 DID 表路由 + 动态 DID `0xF199` | 静态 DID（版本、模拟量）走查表；`0xF199` 动态直接穿透读取外部 W25Q64 |
| `0x2E` | 按标识符写入数据 (WDBI) | 专用持久化 DID `0xF199` | 严格校验报文长度与 DID，调用 CDD 写入 W25Q64，实现参数掉电持久化 |
| `0x7F` | 负响应管理 (NRC) | 支持标准车规 NRC 码 | 覆盖 `0x11`、`0x12`、`0x13`、`0x22`、`0x31` 等标准错误码防御 |

---

## 关键测试与运行日志

### 1. UDS 0x2E / 0x22 标定数据掉电持久化测试

```text
>>> 上位机写入标定参数 (0x2E, DID: 0xF199, 8字节载荷)
[TX]: 0x2E 0xF1 0x99 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88
[RX]: 0x6E 0xF1 0x99 (写入外部 Flash 成功，正响应)

>>> 开发板物理断电，重新上电复位开机...

>>> 上位机读取标定参数 (0x22, DID: 0xF199)
[TX]: 0x22 0xF1 0x99
[RX]: 0x62 0xF1 0x99 0x11 0x22 0x33 0x44 0x55 0x66 0x77 0x88 (数据完全吻合，持久化生效)
```

### 2. Bootloader 固件搬运与起死回生测试串口日志

```text
[BOOT] ========================================================
[BOOT] System Power-on. Checking External SPI Flash...
[BOOT] Upgrade Flag 0xA5 Detected in Sector 0!
[BOOT] Step 1: Unlocking Internal Flash Controller...
[BOOT] Step 2: Erasing Page 16 (0x08004000 ~ 0x080043FF)...
[BOOT] Verify: 0x08004000 readback = 0xFFFFFFFF (Page successfully cleared!)
[BOOT] Step 3: Copying 1024 Bytes from W25Q64 [0x002000] -> Internal Flash...
[BOOT] Programming completed. Restored MSP = 0x200008F0
[BOOT] Step 4: Clearing Upgrade Flag in External Flash...
[BOOT] Step 5: De-initializing SysTick & Disabling IRQ...
[BOOT] Clean jump to Application at 0x08004000!
[BOOT] ========================================================

 \ | /
- RT -     Thread Operating System
 / | \     4.0.4 build Oct 2026
 2006 - 2026 Copyright by RT-Thread team
[APP] Hardware Initialized Successfully.
[APP] CAN Driver @ 500kbps Ready.
[APP] ISO-TP Network Layer & UDS Engine Started.
msh >
```

---

## 工程构建与烧录指引

项目由两个独立的工程构成，烧录时注意**必须使用扇区擦除（Sector Erase）**，防止相互覆盖：

### 1. Bootloader 工程 (Keil MDK)

- **目标单片机**：STM32F103C8
- **链接配置**：IROM1 Start: `0x08000000`, Size: `0x4000` (16 KB)
- **烧录配置**：`Options for Target -> Utilities -> Flash Download` 必须勾选 `Erase Sectors`。

### 2. Application 工程 (RT-Thread Studio / GCC)

- **链接脚本修改 (`link.lds`)**：

```ld
MEMORY
{
    ROM (rx) : ORIGIN = 0x08004000, LENGTH = 48k
    RAM (rw) : ORIGIN = 0x20000000, LENGTH = 20k
}
```

- **烧录配置**：在调试配置中设置删除选项为 **【擦除扇区】**。

### 3. 硬件连接参考

| STM32F103C8T6 引脚 | 外设模块引脚 | 说明 |
| :--- | :--- | :--- |
| PA11 / PA12 | TJA1050 (CAN_RX / CAN_TX) | CAN 总线通信引脚 (500 kbps) |
| PA5 / PA6 / PA7 | W25Q64 (SCK / MISO / MOSI) | 硬件 SPI1 接口 (Mode 0, 稳定传输) |
| PA4 | W25Q64 (CS) | 软件片选 GPIO |
| 3V3 / GND | TJA1050 & W25Q64 | 系统供电与共地 |