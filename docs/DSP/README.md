# DSP 专题

本目录收纳 `App/dsp/` 模块相关说明。当前覆盖 CMSIS-DSP V1.10 集成、FFT、FIR/IIR 滤波和系数导入流程。

## 文档清单

- [MATLAB滤波器设计与导出.md](MATLAB滤波器设计与导出.md)
  - 在 MATLAB 中设计 FIR / IIR Biquad，并把系数粘贴进工程的标准流程。
- [实时流水线设计.md](实时流水线设计.md)
  - ADC → 滤波 → FFT 实时流水线的数据流、状态机、CLI 接口与性能预算。
- [FFT加窗与系数模板.md](FFT加窗与系数模板.md)
  - FFT 前置加窗模块（Hann/Hamming/Blackman 公式 + Kaiser MATLAB 模板）、coherent gain 幅度补偿、`pipeline window` CLI 用法。

## 模块当前状态

- 路径：`App/dsp/`
  - `dsp.h` / `dsp.c`：FFT、FIR、Biquad 的薄包装 API + 自测函数
  - `dsp_filters.h` / `dsp_filters.c`：所有滤波器系数的集中存放点
  - `dsp_window.h` / `dsp_window.c`：FFT 前置加窗（公式型 + MATLAB 模板）
  - `dsp_pipeline.h` / `dsp_pipeline.c`：ADC→滤波→加窗→FFT 实时链路
- CLI：`fft selftest` / `filter selftest` / `pipeline window <name>`（详见根 `App/cli/cli.c`）
- 内置示例滤波器：
  - FIR：5 点滑动平均（`dsp_fir_lp_ma5_*`）
  - Biquad：2 阶 Butterworth low-pass，fc=2 kHz @ Fs=8 kHz（`dsp_biquad_lp_butter2_2khz_*`）
  - FIR / Biquad 还各保留一个 pass-through 模板，等待 MATLAB 系数粘贴
- 链接选项：`-u _printf_float`（newlib-nano 启用 `%f` 浮点格式化）
