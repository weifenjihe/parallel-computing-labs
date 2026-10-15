# SPM 与脉动阵列

这一页先认识各个模块，再沿着一个数据块追踪它的路径。


<figure class="hardware-flow" aria-label="软件DRAM通过读DMA加载A和B到SPM，经阵列计算的部分和累加到C的SPM，再由写DMA回写同一软件DRAM">
  <div class="hardware-flow-grid">
    <div class="hardware-node flow-dram"><b>软件 DRAM</b><span>完整 A、B、C</span></div>
    <span class="flow-arrow flow-read-1" aria-hidden="true">→</span>
    <div class="hardware-node flow-read"><b>读 DMA</b><span>加载输入分块</span></div>
    <span class="flow-arrow flow-read-2" aria-hidden="true">→</span>
    <div class="hardware-node flow-ab"><b>A / B 的 SPM</b><span>buffer 0 / 1</span></div>
    <span class="flow-arrow flow-read-3" aria-hidden="true">→</span>
    <div class="hardware-node flow-array"><b>P × P 阵列</b><span>A 向右 · B 向下</span><span>PE 保存部分和</span></div>
    <span class="flow-arrow flow-write-1" aria-hidden="true">←</span>
    <div class="hardware-node flow-write"><b>写 DMA</b><span>回写输出分块</span></div>
    <span class="flow-arrow flow-write-2" aria-hidden="true">←</span>
    <div class="hardware-node flow-c"><b>C 的 SPM</b><span>跨 K 分块累加</span></div>
    <span class="flow-arrow flow-write-3" aria-hidden="true">←</span>
  </div>
  <figcaption>沿上方路径加载与计算，沿下方路径回写。控制器安排加载、缓冲切换、部分和累加与下一块计算。</figcaption>
</figure>

## 1. 软件 DRAM：保存完整矩阵

软件 DRAM 是 C++ 中的一片存储，保存行优先的 A、B 和 C。RTL 发出读写请求后，软件模型经过设定延迟，按拍提供或接收数据。

本实验接口每拍传输 64 位，读请求给出起始地址和拍数。返回的数据保持到接收方完成握手，不能在对方尚未接收时提前丢掉。

`src/dram.cpp` 实现延迟、数据拍间隔和握手停顿。供数组织参考 [ZirconSim](https://github.com/MAdrid1011/ZirconSim) 中软件存储逐周期驱动 RTL 的方法。

!!! note "这里模拟哪些存储行为？"
    模型关注请求延迟、逐拍数据传输和 ready/valid 握手。它用于本实验的供数观察，没有建模完整 DDR 存储控制器的 bank、行缓冲和命令时序。

## 2. ready 与 valid 怎样配合？

一方用 `valid` 表示当前数据或请求有效，另一方用 `ready` 表示自己能够接收。在有效时钟沿上，两者同时为 1 才完成一次传输。

```text
valid=1，ready=1 → 本拍被接收
valid=1，ready=0 → 保持有效数据，继续等待
valid=0          → 本拍没有可接收的有效数据
```

这让发送方和接收方可以独立停顿。查看 `src/simulator.cpp`，找到驱动信号、采样握手、推进时钟和更新事务的顺序。

## 3. DMA：按请求搬运分块

DMA（Direct Memory Access）在这里负责搬运，而不执行矩阵乘加：

- `rtl/read_dma.sv` 从软件 DRAM 读取 A、B 分块，写入输入 SPM。
- `rtl/write_dma.sv` 从 C 的 SPM 取出输出分块，写回软件 DRAM。

完整矩阵可能包含许多分块，DMA 需要根据全局行列范围生成地址，并处理最后不足一块的区域。

## 4. SPM：明确控制的片上工作空间

SPM（Scratchpad Memory）保存当前计算需要的数据。与由硬件自动决定换入换出的缓存不同，这里由 DMA 和控制器明确安排加载什么、何时使用、何时覆盖。

本实验中 A、B 各有两个物理缓冲区，C 有一个缓冲区。SPM 分 bank 提供阵列需要的并行访问：A 按行、B 按列、C 按列组织。

使用单缓冲模式时只按单缓冲时序使用输入；双缓冲模式会轮流使用两个输入缓冲区。两种模式使用相同的物理 SPM 容量，可以把观察重点放在调度差别上。

打开 `rtl/scratchpad.sv`，分别找到 A、B、C 的存储和读写端口。

## 5. 一个 PE 做什么？

一个处理单元收到 A 元素 a 和 B 元素 b，在两者都有效时执行：

```text
sum ← sum + a × b
```

同时把 A 传到右边，把 B 传到下边。PE 保存自己的部分和；这个分工称为 **output-stationary（输出驻留）**，表示当前输出的累加值留在负责它的 PE 中。

在 `rtl/pe.sv` 中查看输入有效信号、邻接寄存器和累加器，再在 `rtl/systolic_array.sv` 中找到 PE 之间的连接。

## 6. 数据为什么要错开进入？

对于阵列位置 `(r,c)`，它需要把同一 k 的 `A[r,k]` 与 `B[k,c]` 配成一对。

A 从左边进入第 r 行，再向右传播 c 步；B 从上边进入第 c 列，再向下传播 r 步。边缘供数对第 r 行的 A 延迟 r 步、对第 c 列的 B 延迟 c 步，这样同一 k 的两者可以在相同的 `k+r+c` 步到达对应 PE。

这也意味着阵列不是第一拍就全部开始工作：需要填充，最后还要让已有数据继续传播并排空。

??? info "阵列越大，每拍就一定做满所有乘加吗？"
    只有 A、B 都有效时，PE 才执行有效乘加。填充、排空、边界不完整的微块，以及等待输入都会影响利用率。因此要把有效乘加次数和周期一起看。

## 7. 从一块输入走到一块输出

在 `rtl/matrix_accelerator.sv` 中寻找这些阶段：

1. 为当前输出块清零 C 的 SPM，并准备 A、B 的输入块。
2. 将输入送入阵列，按 P×P 输出微块执行计算。
3. 将本次 K 分块的部分和累加到 C 的 SPM。
4. 继续下一个 K 分块，直到当前输出块的 K 维全部完成。
5. 将 C 输出块写回软件 DRAM，转到下一输出块。

把控制状态与 DMA、SPM 和 PE 的作用对应起来，比一开始逐行阅读整个控制器更容易。

[继续：分块、双缓冲与周期](tiling.md){ .md-button .md-button--primary }
