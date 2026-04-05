# 02 CubeMX 页面逐项解释

这篇按你截图里的页面顺序解释。

## 1. Speculation default mode Settings

### 1.1 这个选项在管什么

这是 Cortex-M7 的一种预取/推测访问行为控制项。它不是 `Cache` 本身，但会影响 CPU 在访问代码和数据时的执行效率。

### 1.2 你该怎么理解

你可以粗略理解为：

- CPU 会“预判”后续可能用到的内容
- 这样可以少等几个周期
- 大多数正常工程里保持默认开启即可

### 1.3 什么时候不建议你碰它

在你还没有遇到非常具体的外部存储器、总线访问、异常时序问题之前，不要把排查精力放在这个选项上。

### 1.4 当前建议

- 保持 `Enabled`

## 2. Cortex Interface Settings

这里主要就是：

- `CPU ICache`
- `CPU DCache`

### 2.1 CPU ICache

#### 它是什么

- 指令缓存
- 提高取指效率

#### 开启后影响

- 程序执行速度一般会更好
- 对普通逻辑代码收益较明显
- 通常不会直接制造 DMA 数据错乱

#### 对你的建议

- 建议启用

### 2.2 CPU DCache

#### 它是什么

- 数据缓存
- 提高数据访问效率

#### 开启后影响

优点：

- 算法和数组访问可能明显加速

代价：

- 你必须开始严肃处理 `CPU / DMA` 一致性

#### 如果不开，会怎样

- 性能少一部分
- 但系统行为更直观
- 更适合基础工程起步阶段

#### 对你的建议

- 先不要急着开
- 等你准备接入 DMA 数据链路时，再连同 MPU 一起规划

## 3. Cortex Memory Protection Unit Control Settings

### 3.1 MPU Control Mode

这个选项决定了 MPU 启用后的总体控制方式。

你当前工程生成出来的是：

```c
HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
```

这个模式可以理解为：

- MPU 已启用
- 对没有被显式 Region 覆盖的空间，特权访问使用默认内存映射属性

它的好处是：

- 不至于一上来就把整个地址空间都配置得极其复杂
- 允许你先为少量关键区域设置属性

### 3.2 为什么 CubeMX 常给这个模式

因为它适合作为模板工程起点：

- 既有基本保护
- 又不至于难以启动

## 4. Cortex Memory Protection Unit Region x Settings

### 4.1 Region 到底是什么

每个 Region 就是一段地址空间规则。

一个 Region 会定义：

- 基地址
- 大小
- 能否访问
- 能否执行
- 是否缓存
- 是否写缓冲
- 是否共享

### 4.2 你截图里的 Region 0

你现在看到的是典型的 CubeMX 默认模板：

- `Base Address = 0x0`
- `Size = 4GB`
- `NO ACCESS`
- `Instruction Access = DISABLE`
- 再通过 `SubRegionDisable` 留出默认工作窗口

它的本意不是“帮你完成最终内存规划”，而是“给你一个基础保护框架”。

### 4.3 后面你真正会主动配置哪些 Region

当工程进入 DMA/RTOS 阶段，通常会开始认真规划这些 Region：

1. DMA 接收缓冲区
   - 一般倾向 non-cacheable
2. DMA 发送缓冲区
   - 视策略决定 non-cacheable 或 clean cache
3. 普通算法数据区
   - 倾向 cacheable
4. 某些不应执行的 RAM 区
   - `XN = Disable instruction access`

## 5. MPU 里的几个子项怎么理解

### 5.1 MPU Region Base Address

区域起始地址。

### 5.2 MPU Region Size

区域大小，必须按 MPU 支持的固定粒度选。

不是任意大小。

### 5.3 MPU SubRegion Disable

一个大 Region 可以进一步切成 8 个子区，通过位掩码屏蔽部分子区。

这个选项在 CubeMX 默认模板里经常用来做“粗范围保护 + 局部放开”。

### 5.4 MPU Access Permission

控制读写权限。

常见工程理解：

- 完全禁止访问
- 特权可读写
- 全访问
- 只读

### 5.5 MPU Instruction Access

决定这块区域能不能执行指令。

典型用法：

- RAM 中普通数据区禁止执行
- 防止跑飞

### 5.6 MPU Shareability Permission

表示这块区域是否被多个主体共享。

但你要记住：

- `Shareable` 不是“自动解决一致性”
- 它只是属性的一部分
- 真正的一致性还要靠缓存策略和维护动作

### 5.7 MPU Cacheable Permission

是否允许缓存。

这是以后给 DMA buffer 做非缓存区时最关键的开关之一。

### 5.8 MPU Bufferable Permission

是否允许写缓冲。

它也影响访问特性，但在你当前阶段不用过早抠到寄存器级别，先和 `Cacheable`、`Shareable` 一起作为“内存属性组”来理解。

## 6. 页面上看不出来，但你必须记住的现实

CubeMX 页面只是“配置入口”，真正的系统行为取决于：

- 生成代码
- 内存映射
- 链接脚本
- 变量放置位置
- 是否用了 DMA
- 是否用了 RTOS

所以你不能只看 CubeMX 页面本身判断“系统一定安全”或“性能一定最好”。
