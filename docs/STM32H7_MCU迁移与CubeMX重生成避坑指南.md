# STM32H7 MCU 迁移与 CubeMX 重新生成避坑指南

本文档记录工程从 `STM32H743VGT6` 迁移到 `STM32H743ZIT6` 的完整过程，提炼成下次再做类似迁移（V→Z→I→X，或同家族换型号、换板）时的可复用流程。

## 1. 前置认知

H7 同家族跨型号迁移的成本理论上**很低**：

- 同一颗 die（H743 全系）共用：HAL 驱动、CMSIS、启动文件 `startup_stm32h743xx.s`、宏 `STM32H743xx`
- 真正变化的只有：引脚集合（V⊂Z⊂I⊂X）、Flash 容量、封装

但在工具链层面有几个**容易被低估的坑**：

- `CubeMX` 切 MCU 的方式不同，破坏程度差很多
- `CubeMX` 重新生成代码会重置部分系统配置
- 自定义链接脚本片段（如 `.dma_buffer`）和自定义链接选项（如 `-u _printf_float`）会被覆盖
- 板差异（LED 引脚、串口路由）会让看起来"代码完全一样"的工程在新板上完全不亮

## 2. 推荐操作流程（最稳）

### 2.1 提交干净状态 + 打 tag

迁移前先确保工作树干净，然后打一个回滚锚：

```bash
git tag pre-zit6-migration
git branch backup/pre-zit6
```

任何后续步骤出问题，都可以 `git reset --hard pre-zit6-migration` 一键回滚。

> 这步在本次迁移中**实际救命了**——CubeMX 误操作把 `App/`、`.vscode/`、`CMakeLists.txt` 全擦了，靠 tag 全部恢复。

### 2.2 在 CubeMX 内切换 MCU 的两种方式

**正确方式（保留全部配置）**：

- 打开现有 `.ioc`
- `Pinout` 下拉菜单 → **`List Pinout Compatible MCUs`**（列出引脚兼容 MCU）
- 在列表中选目标型号 → 双击/确认
- 按提示完成引脚迁移（V→Z 是子集，全部 Keep）

**错误方式（重置全部配置）** —— 不要走：

- `File → New Project` 新建工程后选新 MCU
- 这会用**默认配置**起一个新 `.ioc`，丢失 PLL、MPU、I-Cache、外设参数等所有手工设置
- 即使保留了引脚定义，主时钟会被重置成 64 MHz HSI、`PLLState=NONE`、ADC 时钟源换成 PLL2 等

如果不慎走了"错误方式"，本文第 4 节给出补救路径。

### 2.3 引脚迁移确认

CubeMX 弹"Pin Migration"对话框时：

- V/Z/I/X 是引脚超集关系，原工程引脚在新封装上**全部存在**，可全部 Keep
- Z 引出的额外引脚（PB/PD/PE/PF/PG/PH 等）此时不要急着用，按需后续再加

### 2.4 时钟配置核对

进 `Clock Configuration` 标签：

- 看时钟树是否全绿。理论上同 die 切换时钟约束不变，PLL 配置应直接套用
- 如果弹 `Resolve Clock Issues` 对话框，**点 No**，先手动看清楚再决定

H743 上 480 MHz 跑通需要：

- `Pinout & Configuration → System Core → RCC → Power Parameters`：
  - `SupplySource = PWR_LDO_SUPPLY`
  - `Power Regulator Voltage Scale = Power Regulator Voltage Scale 0`（VOS0，否则 480 MHz 超频报红）
- `Clock Configuration` 时钟树：
  - HSI 源 → PLLM `/4` → PLLN `×60` → PLLP `/2` = 480 MHz
  - **System Clock Mux 选 PLLCLK**（容易停留在 HSI 默认值）
  - HPRE `/2` → HCLK 240 MHz（H7 规范要求 AHB ≤ 240 MHz）
  - APB1/2/3/4 全部 `/2` → 各 120 MHz
- ADC Clock Mux 选 **PER_CK**（外设公共时钟），不是 PLL2P
- Peripheral Common Clock 的 CKPER 源选 HSI

### 2.5 关键 .ioc 字段核对

CubeMX 切完 MCU 后，确认 `.ioc` 文本里以下 key 都存在：

```
RCC.SYSCLKSource=RCC_SYSCLKSOURCE_PLLCLK
RCC.ADCCLockSelection=RCC_ADCCLKSOURCE_CLKP
CORTEX_M7.CPU_ICache=Enabled
CORTEX_M7.IPParameters=default_mode_Activation,CPU_ICache
CORTEX_M7.default_mode_Activation=1
```

这五个 key 决定了主时钟、ADC 时钟、I-Cache 是否被代码生成器写进 `main.c` / `adc.c`。任何一个缺失，对应模块就会回到默认（往往是不工作的状态）。

如果 `.ioc` 缺其中某项，**优先在 CubeMX UI 内补**（`System Core → CORTEX_M7` 勾选 I-Cache、`Clock Configuration` 改 mux 等），让 CubeMX 自己写进 `.ioc`。

### 2.6 生成代码

- `Project Manager → Project` 标签确认：`Toolchain/IDE = CMake`，`Project Name = <你的工程名>`
- `Project Manager → Code Generator` 标签：**不要勾** `Delete previously generated files when not re-generated`，否则会误删 `App/`
- `Alt+K` 或 `Project → Generate Code`

### 2.7 生成后验证（重点）

CubeMX 生成完，在 git 里看 diff：

```bash
git status
git diff Core/Src/main.c
```

至少核对以下几点都还在 `main.c` 里：

- `MPU_Config()` 函数定义 + 调用
- `SCB_EnableICache()` 调用
- `PeriphCommonClock_Config()` 函数定义 + 调用
- `PLLState = RCC_PLL_ON`、`PLLM = 4`、`PLLN = 60`
- `SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK`
- USER CODE BEGIN 段里的 `*_APP_Init()`、`Scheduler_AddTask`、`Scheduler_Run` 等

如有缺失，说明 `.ioc` 的某些 key 仍然不齐，回 2.5 节继续补。

## 3. 必须手工处理的非 .ioc 内容

`.ioc` 的"管辖范围"不包括以下内容，CubeMX 重新生成时会用模板默认值覆盖。每次 regen 后都要重新检查：

### 3.1 链接脚本里的自定义段

如果工程使用了 `__attribute__((section(".dma_buffer")))` 把 DMA 缓冲区放到 D2 SRAM，新生成的 `.ld` 文件**不会**自动包含这个段。需要手工补回：

```ld
  /* DMA buffers must live in DMA-accessible RAM (D2 SRAM at 0x30000000),
     not DTCM, since GPDMA1 cannot reach DTCM. App code marks buffers with
     __attribute__((section(".dma_buffer"))). 32-byte aligned for cache lines. */
  .dma_buffer (NOLOAD) :
  {
    . = ALIGN(32);
    *(.dma_buffer)
    *(.dma_buffer*)
    . = ALIGN(32);
  } >RAM_D2
```

插入位置：`.bss` 段之后，`._user_heap_stack` 段之前。

详细背景见 [STM32H7_DMA缓冲区与链接脚本说明.md](STM32H7_DMA缓冲区与链接脚本说明.md)。

### 3.2 链接脚本路径在 toolchain cmake 里

新生成的 `.ld` 文件名通常带封装代号，例如：

- `STM32H743XG_FLASH.ld` (H743**G**, 1 MB Flash)
- `STM32H743XX_FLASH.ld` (H743 通用，2 MB Flash 区版本)
- `STM32H743ZITX_FLASH.ld` (一些 CubeMX 版本会带 ZITX 后缀)

`cmake/gcc-arm-none-eabi.cmake` 和 `cmake/starm-clang.cmake` 里的 `-T` 参数要跟着改：

```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32H743XX_FLASH.ld\"")
```

### 3.3 newlib-nano 的浮点 printf 开关

CubeMX 模板默认的 toolchain cmake **不带** `-u _printf_float`，导致 `printf("%f")` 静默失败（不崩溃，直接吞掉 `%f` 之后的输出）。需要在两个 toolchain 文件里都加：

```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map -Wl,--gc-sections -u _printf_float")
```

代价约 +2 KB Flash，在 2 MB 的 ZIT6/XIH6 上无感。

### 3.4 调试器配置里的 device 字段

`.vscode/launch.json` 和 `.vscode/tasks.json` 里 cortex-debug 的 `"device"` 字段需要跟着 MCU 改：

- VGT6 → `"STM32H743VGTx"`
- ZIT6 → `"STM32H743ZITx"`
- XIH6 → `"STM32H743XIHx"`

device 值用于让调试器加载正确的 SVD（外设寄存器视图）。烧录本身不依赖，但调试时寄存器名会错乱。

### 3.5 build cache 必须删

旧 `build/` 目录里 CMake cache 记着旧 MCU 的 toolchain 路径、旧 `.ld` 文件名、旧的编译产物：

```
rm -rf build/
```

或在 VS Code 命令面板：`CMake: Delete Cache and Reconfigure`。

## 4. 误用"新建工程"流程的补救

如果在 CubeMX 里**误用了"新建工程"** 而不是切换 MCU，导致 `.ioc` 配置被重置 + `App/` 等用户文件被擦：

### 4.1 立即停止后续操作

不要再在 CubeMX 内做任何点击或 Generate Code，避免覆盖更多文件。

### 4.2 用 git restore 拉回被误删文件

```bash
git restore App/ .vscode/ .gitignore .clangd .settings/ CMakeLists.txt Core/Src/main.c Core/Src/adc.c Core/Inc/usart.h
```

不要 restore 旧的 `.ld` 文件——新 `.ld` 才是正确的。

### 4.3 手补 .ioc 关键 key

CubeMX "新建" 流程产生的 `.ioc` 通常缺第 2.5 节列的 5 个 key。可以直接在文本编辑器里按字母顺序插入。本次迁移的实际操作记录可参考 git 历史 commit `a92a2d7` 的 `.ioc` 部分。

### 4.4 链接脚本和 toolchain 选项按第 3 节处理

回到 2.7 节做生成后验证。

## 5. 板差异确认（最容易被忽略）

代码全对的情况下板子还是不亮、串口不通，往往是因为新板的 LED 引脚 / 串口路由和旧板不同。

### 5.1 LED 引脚

不同厂家最小系统板差异很大：

| 板厂 / 型号 | LED1 引脚 | 极性 |
|---|---|---|
| 例程板 (LQFP144 ZIT6) | PG7 | 低有效 |
| 野火 H743 mini | PB0/PB1 | 通常低有效 |
| 正点原子 H743 战神 | PB0/PB1/PB14 | 低有效 |
| 部分杂牌"裸板" | 无板载 LED，PC13 是按键 | - |

**确认方法**：找板厂提供的最小例程，看其 `LED_Init` 或 GPIO 配置。或直接读板上原理图。

修改时优先在 `.ioc` 里改 GPIO 标签，让 CubeMX 生成正确的 `LED1_Pin` / `LED1_GPIO_Port` 宏。`led.c` 不需要改。

### 5.2 串口

板载 USB-UART 通常通过：

- USART1 (PA9/PA10) —— 大多数 H7 最小系统板
- USART3 (PD8/PD9 或 PB10/PB11) —— 某些较老板型
- 不存在板载 USB-UART —— 需自行外接 CP2102/CH340/FT232

**确认方法**：板子插 USB 后看设备管理器是否出现 COM 口。出现的话可能是 USART1 也可能是 USART3，多试几个波特率/USART。

### 5.3 时钟源

最小系统板通常都是 HSI 64 MHz 或外接 25 MHz HSE。如果你工程用 HSE，要确认板上 HSE 晶振存在并接对引脚（PH0/PH1）。

## 6. 验证清单

迁移完成、第一次烧录前，把以下勾完：

- [ ] `.ioc` 里 MCU 字段（CPN/Name/Package/UserName/DeviceId）全部指向新型号
- [ ] `.ioc` 里 5 个关键 key 都在（见 2.5 节）
- [ ] `STM32H743XX_FLASH.ld`（或对应 .ld）存在，FLASH 大小匹配新型号
- [ ] `.ld` 里有 `.dma_buffer (NOLOAD) ... >RAM_D2` 段
- [ ] `cmake/gcc-arm-none-eabi.cmake` 和 `cmake/starm-clang.cmake` 的 `-T` 路径指向新 `.ld`
- [ ] 这两个 cmake 文件都有 `-u _printf_float`
- [ ] `.vscode/launch.json` 和 `tasks.json` 的 device 字段更新
- [ ] `Core/Src/main.c` USER CODE 块里的 `*_APP_Init` / `Scheduler_*` 调用完整
- [ ] `main.c` 里 PLL/MPU/I-Cache 初始化代码完整
- [ ] `build/` 目录已删
- [ ] `git diff` 检查所有修改在意料之中

## 7. 烧录后排查顺序

如果板子上电后什么都没反应：

1. **LED 不亮**：先核对引脚（第 5.1 节）。引脚对了再看代码。
2. **串口无输出**：
   - 板子端：用万用表 / 示波器测 TX 引脚有没有信号
   - 电脑端：换波特率、换串口工具、换 USB 线
   - 代码端：确认 DMA 缓冲区在 `.dma_buffer` 段（即 D2 SRAM），不在 DTCM
3. **`printf("%f")` 不输出值**：第 3.3 节，加 `-u _printf_float`
4. **HardFault**：通常是 SystemClock_Config 报错（VOS 不对、PLL 锁不住）或 MPU 配置错误

## 8. 历史记录

- 2026-04-25：从 STM32H743VGT6（LQFP100 / 1 MB Flash）迁移到 STM32H743ZIT6（LQFP144 / 2 MB Flash）
- 主要驱动：板载 LED 在 PG7（V 系列没有 PG）、为后续接双路 AD9226 + AD9764 留够 IO
- 实际遇到的所有坑都已在本文档第 3、4、5 节归档
