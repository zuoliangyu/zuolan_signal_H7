# MATLAB 滤波器设计与导出到工程

本文档约束滤波器系数从 MATLAB 流到工程的统一流程。所有 FIR / IIR 系数最终都落在 `App/dsp/dsp_filters.c`。

## 0. 总原则

- **MATLAB 是唯一权威设计源**。工程里不手写系数，避免丢失设计上下文。
- 每个滤波器在 `dsp_filters.h` 必须配套写明：
  - 设计参数（Fs、fc、阶数、窗函数、滤波器族）
  - 一段最小可复现的 MATLAB 脚本（注释里贴）
- 系数数组使用 `const float32_t`，常驻 Flash。
- **state 缓冲不放在 `dsp_filters.c`**，由调用方按用途分配（不同 block_size 不同长度）。

## 1. FIR 流程

### 1.1 在 MATLAB 中设计

最常用 `fir1`（窗函数法）或 `firpm`（Parks-McClellan 等纹波法）。例：

```matlab
Fs = 8000;
N  = 64;                     % 阶数（系数个数 = N+1 = 65）
fc = 1500;                   % 截止频率
h  = fir1(N, fc/(Fs/2));     % 默认 Hamming 窗
fvtool(h, 1, 'Fs', Fs);      % 可视化检查
```

### 1.2 导出系数

在 MATLAB 命令窗运行：

```matlab
fprintf('%.10ff,\n', h);
```

得到一长串 `0.xxxxxxxxxxf,` 文本，整段复制。

### 1.3 粘贴进工程

打开 `App/dsp/dsp_filters.h`，找到 FIR 模板段，调整 `NUM_TAPS`：

```c
#define DSP_FIR_TEMPLATE_NUM_TAPS  65U   // = N + 1
```

打开 `App/dsp/dsp_filters.c`，把粘贴出来的数字塞进 `dsp_fir_template_coeffs[]`：

```c
const float32_t dsp_fir_template_coeffs[DSP_FIR_TEMPLATE_NUM_TAPS] = {
    /* === 粘贴 MATLAB fir1 输出 === */
    0.0012345678f, 0.0034567890f, ... ,
    /* ============================= */
};
```

### 1.4 调用

```c
#include "dsp.h"
#include "dsp_filters.h"

#define BLOCK 256U
static float32_t s_state[DSP_FIR_TEMPLATE_NUM_TAPS + BLOCK - 1U];
static dsp_fir_f32_t s_fir;

void my_init(void) {
    DSP_FIR_F32_Init(&s_fir,
                     DSP_FIR_TEMPLATE_NUM_TAPS,
                     dsp_fir_template_coeffs,
                     s_state, BLOCK);
}

void my_process(const float *in, float *out) {
    DSP_FIR_F32_Process(&s_fir, in, out, BLOCK);
}
```

> state 必须**至少** `num_taps + block_size - 1` 个 `float32_t`，开少一个就内存越界。
> 多个滤波器实例**不能共享 state**，每个 instance 一个独立 state buffer。

## 2. IIR Biquad 流程

CMSIS-DSP 推荐用级联二阶段（SOS, Second-Order Sections）实现 IIR，数值更稳。MATLAB 里设计高阶 IIR 后必须 `tf2sos` 拆段。

### 2.1 在 MATLAB 中设计

```matlab
Fs = 8000;
fc = 2000;
[b, a]   = butter(2, fc/(Fs/2));   % 2 阶 Butterworth low-pass
[sos, g] = tf2sos(b, a);           % sos 是 N×6 矩阵，g 是总增益
```

可换：
- `butter` → `cheby1`（Chebyshev I）/ `cheby2` / `ellip`（椭圆滤波器）
- `'low'` → `'high'` / `'bandpass'` / `'stop'`

### 2.2 转成 CMSIS 格式

CMSIS 的 Biquad DF2T 要求**每段 5 个系数** `{b0, b1, b2, a1, a2}`：

- 取 `sos` 的列 1-3 作为 `b0, b1, b2`
- 取 `sos` 的列 5-6，**取负号**作为 CMSIS 的 `a1, a2`（CMSIS 用加号约定）
- 总增益 `g` 乘进**第一段**的 `b0, b1, b2`

```matlab
sos_cmsis      = sos(:, [1 2 3 5 6]);
sos_cmsis(:, 4:5) = -sos_cmsis(:, 4:5);    % a1/a2 取负
sos_cmsis(1, 1:3) = sos_cmsis(1, 1:3) * g; % 增益放第一段

fprintf('%.10ff, %.10ff, %.10ff, %.10ff, %.10ff,\n', sos_cmsis');
```

### 2.3 粘贴进工程

打开 `App/dsp/dsp_filters.h`：

```c
#define DSP_BIQUAD_TEMPLATE_NUM_STAGES  N   // sos 行数
```

打开 `App/dsp/dsp_filters.c`：

```c
const float32_t dsp_biquad_template_coeffs[5U * DSP_BIQUAD_TEMPLATE_NUM_STAGES] = {
    /* === 粘贴 sos_cmsis 输出 === */
    /* stage 0 */ b0, b1, b2, a1, a2,
    /* stage 1 */ b0, b1, b2, a1, a2,
    /* ... */
    /* ========================== */
};
```

### 2.4 调用

```c
static float32_t s_state[2U * DSP_BIQUAD_TEMPLATE_NUM_STAGES];  // 每段 2 个 state
static dsp_biquad_f32_t s_biq;

void my_init(void) {
    DSP_Biquad_F32_Init(&s_biq,
                        DSP_BIQUAD_TEMPLATE_NUM_STAGES,
                        dsp_biquad_template_coeffs,
                        s_state);
}

void my_process(const float *in, float *out, uint32_t n) {
    DSP_Biquad_F32_Process(&s_biq, in, out, n);
}
```

## 3. 命名约定

| 类别 | 命名格式 | 示例 |
|---|---|---|
| FIR | `DSP_FIR_<TYPE>_<DESC>_NUM_TAPS` `dsp_fir_<type>_<desc>_coeffs` | `DSP_FIR_LP_MA5_*` |
| Biquad | `DSP_BIQUAD_<TYPE>_<DESC>_NUM_STAGES` `dsp_biquad_<type>_<desc>_coeffs` | `DSP_BIQUAD_LP_BUTTER2_2KHZ_*` |

`<TYPE>`：`LP`（low-pass）/ `HP`（high-pass）/ `BP`（band-pass）/ `BS`（band-stop）/ `NOTCH`
`<DESC>`：滤波器族 + 显著参数，如 `MA5` / `BUTTER2_2KHZ` / `CHEBY4_FC500`

## 4. 验证步骤

每次新加滤波器都做这两步：

1. **MATLAB 仿真**：在 MATLAB 里用同一段输入信号过这个滤波器，记录幅频响应（`fvtool` 或 `freqz`）。
2. **板级 selftest**：写一个小 selftest（仿照 `DSP_Filter_SelfTest`），输入 1 kHz / 1024 点正弦，对比：
   - 滤波后 RMS 增益是否接近 MATLAB 仿真值（误差 < 1%）
   - FFT 峰值 bin 是否还在 128（未在阻带内）
   - FFT 峰值幅度变化是否符合幅频响应预期

## 5. 常见坑

| 坑 | 现象 | 排查 |
|---|---|---|
| Biquad `a1/a2` 没取负 | 输出发散到 NaN/Inf | 检查 CMSIS 系数行的第 4-5 个元素是否是 MATLAB 值的负数 |
| FIR state 缓冲长度不够 | 输出有 spike / 内存被踩 | `num_taps + block_size - 1` 是下限，不是上限可任选 |
| 多 instance 共享 state | 串扰、输出错乱 | 每个滤波器实例独立分配 state |
| 系数数组没 const | 占 RAM，浪费 | 一定写 `const float32_t` |
| 用 `arm_biquad_cascade_df1_*` | DF1 在浮点下数值不如 DF2T | 浮点统一用 DF2T (`arm_biquad_cascade_df2T_f32`) |
| 在 MATLAB 里直接对 `[b,a]` 不拆 SOS | 高阶 IIR 数值不稳 | 阶数 ≥ 3 必须 `tf2sos`（2 阶单段可不拆） |

## 6. 参考

- 工程内自测代码：`App/dsp/dsp.c::DSP_Filter_SelfTest`
- 内置示例：`App/dsp/dsp_filters.c` 里的 `dsp_fir_lp_ma5_*`、`dsp_biquad_lp_butter2_2khz_*`
- ARM 官方文档：CMSIS-DSP `arm_fir_f32` / `arm_biquad_cascade_df2T_f32` 函数页
