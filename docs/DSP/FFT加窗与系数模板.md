# FFT 加窗与窗系数模板

本文档说明 `App/dsp/dsp_window.*` 模块、它在 `dsp_pipeline` 中的位置、窗函数对幅度的影响、以及如何按 `dsp_filters` 同款套路把 MATLAB 设计的窗系数导入工程。

## 1. 为什么要加窗

实时流水线把 ADC 样本切成 1024 点一帧做 RFFT。直接对截断的方块做 FFT 等同于乘了一个矩形窗，频域里就是 `sinc` 卷积，会出现：

- 强主瓣旁边大量旁瓣（"频谱泄漏"）
- 邻近频率被旁瓣裙边掩盖，无法分辨
- 非整周期分量幅度被高估或低估

加窗就是把"硬截断"换成"软衰减"，旁瓣电平显著下降，代价是主瓣变宽（频率分辨率略有损失）。常见窗的折中：

| 窗 | 主瓣宽度 (bins) | 第一旁瓣 | coherent gain (sum/N) | 用途 |
|---|---|---|---|---|
| 矩形 (NONE) | 2 | -13 dB | 1.00 | 整周期信号 / 瞬态 |
| Hann | 4 | -32 dB | 0.50 | 通用首选 |
| Hamming | 4 | -43 dB | 0.54 | 通用，旁瓣稍优 |
| Blackman | 6 | -58 dB | 0.42 | 旁瓣要求严的频谱估计 |
| Kaiser β=8.6 | ~6 | -84 dB | ~0.40 | 自定义旁瓣电平时用 |

## 2. 模块结构

跟 `dsp_filters` 平行：

- `App/dsp/dsp_window.h`：枚举 + API 声明 + Kaiser 模板长度
- `App/dsp/dsp_window.c`：
  - `DSP_Window_Generate(type, dst, N)`：生成窗系数。Hann/Hamming/Blackman 按公式现算（N 可变）；Kaiser 从静态模板拷贝（N 必须等于 1024）
  - `DSP_Window_CoherentGain(w, N)`：算 `sum(w)/N`，用于幅度补偿
  - `dsp_window_kaiser_template[1024]`：MATLAB 模板系数，默认占位是全 1（pass-through）

## 3. 在 pipeline 中的接入点

`dsp_pipeline.c` 内每帧顺序：

```
ADC block (uint16) -> float32
  -> (可选) 粗去 DC （减去 ADC_MIDSCALE）
  -> 滤波器（FIR / Biquad / NONE）
  -> 累积到 fft_size
  -> (一次性) 按帧均值精去 DC
  -> (一次性) arm_mult_f32(buf, window, buf, N)   ← 这里加窗
  -> RFFT magnitude
  -> 找峰 + 用 cgain 反算幅度
```

`SetFftLen` 和 `SetWindowByName` 都会触发 `pipeline_reload_window`，重新填一份窗系数到 `s_window_buf`，并刷新 `s_pl.window_cgain`。

## 4. CLI 用法

```
pipeline window ?              # 列出可选窗，标记当前
pipeline window hann
pipeline window hamming
pipeline window blackman
pipeline window kaiser         # 仅 fft=1024 时有效，否则自动回退到 none 并打印警告
pipeline window none
pipeline window test           # 自动跑全部窗，出对比表 + PASS/FAIL（见下）
pipeline status                # 输出里包含 window=<name> cgain=<f>
```

### `pipeline window test` 自动测试

依次跑 `none / hann / hamming / blackman` 各一次同步 oneshot，单帧日志被静默，
最终打印一张对比表：

```
[WINDOW-TEST] running 4 windows x 1 oneshot each ...

  window     cgain  peak_bin  peak_freq      mag           amp           amp/none
  ---------  -----  --------  -------------  ------------  ------------  --------
  none       1.000         4    999.46 Hz     10150446         10150446    1.000
  hann       0.500         4    999.46 Hz      5075000          10150000    1.000
  hamming    0.540         4    999.46 Hz      5481000          10150000    1.000
  blackman   0.420         4    999.46 Hz      4263000          10150000    1.000
  verdict: PASS  (peak_bin within +/-2 of base=4, amp within +/-15%)
```

判定逻辑：
- 以 `none` 帧的 `peak_bin` 为基准，其余窗的 `peak_bin` 必须在 `±2` 内（窗主瓣展宽允许）
- 其余窗的 `amp` 与 `none` 的 `amp` 比值必须在 `0.85 ~ 1.15` 之间（cgain 补偿一致性）

任一条件不满足判 `FAIL`。

测试前提：DAC 必须输出已知正弦（推荐 `dac test`，得到 1 kHz / 1V 幅度），
并且板子上把 PA4（DAC1_OUT1）短接到 PA0（ADC1_IN16）。
否则信号源只是噪声，peak 会随窗跳动，必然 FAIL。

实现细节：测试开始前会先跑 2 帧"暖机" oneshot（结果丢弃），
等 DAC 输出和 ADC DMA 队列都稳定下来再开始正式 4 帧测量。
否则若紧接着 `dac test` 后立刻测，首帧会采到 DAC 启动瞬态（实测幅度只有正常值的一半），
导致 baseline 错误，amp 比值偏离 PASS 区间。

每帧打印的格式现在多了 `window` 和 `amp`（用 cgain 补偿过的单频幅度）：

```
[PIPELINE] seq=N filter=... window=hann fft=1024 fs=256000 Hz
           frame_mean=... peak_bin=... peak_freq=... mag=... amp=...
```

`mag` 是 RFFT 后的原始 magnitude（受窗增益影响），`amp` 是 `mag / cgain`，对单频信号近似回到原幅度。

## 5. 用 MATLAB 出 Kaiser 系数（替换占位模板的步骤）

```matlab
N    = 1024;
beta = 8.6;             % 旁瓣 ≈ -84 dB；调大主瓣更宽，调小旁瓣更高
w    = kaiser(N, beta);
sprintf('%.10ff,', w)   % 一次性导出 C 数组初始化串
```

把 sprintf 输出粘贴替换 `dsp_window.c` 里 `dsp_window_kaiser_template[]` 的 `[0 ... (N-1)] = 1.0f` 占位，注释 `MATLAB 等价脚本` 一并更新（保持设计参数可反查）。

> 不替换也能编、能跑 —— 占位是全 1，等同于矩形窗，`pipeline window kaiser` 时 `cgain=1.0`。这是为了让模板在没系数前不破坏 pipeline。

## 6. 自定义窗的扩展方式

如果想加 Chebyshev / Flat-top / 任意自定义窗，按 `dsp_filters_template_*` 同款套路：

1. 在 `dsp_window.h` 里 `dsp_window_type_t` 加新枚举 + 静态系数声明
2. 在 `dsp_window.c` 里
   - 在 `s_window_names[]` 加名字
   - 在 `DSP_Window_Generate` 的 switch 加分支（拷贝系数或运行时算）
   - 加常量数组定义（含 MATLAB 注释）
3. 不需要动 `dsp_pipeline` —— 它只通过 `Generate` + `Name` + `FromName` 三个 API 调用窗模块

## 7. 注意点

- **窗长度必须等于 FFT 长度**。当前实现里 fft 是 256/512/1024 三档；公式型窗每档都能现算，模板型窗（Kaiser）只对应一档。
- **加窗放在精去 DC 之后**。因为窗函数边沿压低了样本数值，先去 DC 再加窗能避免在窗"锥形"位置残留 DC 能量被搬运到低频 bin。
- **cgain 只对单频幅度生效**。如果你做的是宽带 PSD 估计，应该用 `processing gain`（`sum(w^2)/N`）而不是 `coherent gain`，本模块没做这条路径，按需扩展。
- **加窗会让滤波器的瞬态响应更柔和**，但也意味着前后约半个窗长的样本贡献度极低，做"边沿事件检测"时要意识到这一点。
