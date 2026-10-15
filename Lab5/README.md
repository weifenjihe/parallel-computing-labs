# Lab5：SPM 与脉动阵列矩阵乘法仿真

使用 Verilator 运行 RTL，计算稠密矩阵乘积：

```text
A: M × K，int16
B: K × N，int16
C: M × N，int32
C = A × B
```

软件 DRAM 保存行优先矩阵，读 DMA 将 A、B 分块加载到 SPM。阵列将 A 向右、B 向下逐 PE 传递，各 PE 保留输出部分和；一个 K 分块完成后，部分和累加到 C 的 SPM。遍历 K 维后，写 DMA 将 C 分块写回软件 DRAM。

## 源码结构

```text
Lab5/
  rtl/
    pe.sv                     int16 乘法、int32 累加与相邻 PE 寄存器
    systolic_array.sv         二维 PE 阵列和有效信号传递
    scratchpad.sv            分 bank 的 A、B 双缓冲和 C 存储
    read_dma.sv              A、B 分块突发读取
    write_dma.sv             C 分块突发写回
    matrix_accelerator.sv    分块循环、阵列控制和周期统计
  include/
    config.hpp               运行参数
    matrix.hpp               软件存储和矩阵布局
    dram.hpp                 DRAM 事务和握手接口
    simulator.hpp            仿真接口和统计
    support.hpp              随机数和宿主机计时
    validation.hpp           命令行值与范围检查
    verification.hpp         参考结果核对接口
  src/
    main.cpp                 命令行参数
    matrix.cpp               矩阵生成和软件存储访问
    dram.cpp                 延迟、逐拍供数和 ready/valid 握手
    simulator.cpp            时钟、DRAM 与 DUT 的逐周期驱动
    benchmark.cpp            重复运行和 CSV 输出
    verification.cpp         软件整数矩阵参考比较
  build.py                   本实验的 Verilator 构建
  cli_utils.py               参数与构建目录检查
  run.py                     顺序运行和汇总
  plot_waveform.py           无额外查看器的 VCD 波形绘图
```

## 环境准备

进入 Lab5，后续命令都在这个文件夹内执行：

```bash
cd Lab5
```

Windows 使用 WSL2 的 Ubuntu 终端安装依赖：

```bash
sudo apt update
sudo apt install -y g++ make python3 python3-venv verilator
```

macOS 安装依赖：

```bash
brew install python verilator
```

安装本实验的 Python 依赖：

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

## 构建

```bash
python3 build.py --array 8
```

## 第一次运行：快速验证

先运行这组小配置，确认环境、程序和输出均正常：

```bash
python3 run.py --m 32 --array 8 --tile 16 --verify --output results/quick
```

终端会显示 `Lab5 i/2` 的配置进度，并出现 `VERIFY PASS: 1024 elements`。PASS 后的误差或说明因实验而异。运行完成后，检查 `results/quick/environment.json`、`raw.csv`、`summary.csv` 和比较图表。

`--verify` 在计时之外逐元素或逐桶比较参考结果。错误会显示 `VERIFY FAIL`、第一个错误位置与期望值，并以非零退出码停止。浮点乘法使用 `1e-10 + 1e-9 × |参考值|` 的误差阈值；转置、计数和整数 GEMM 精确比较。**小配置只确认功能，不能用于正式性能结论。**

## 完整测量

确认快速验证通过后，再运行完整默认扫描：

```bash
python3 build.py
python3 run.py
```


默认构建 4×4、8×8、16×16 阵列。运行 128×128、256×256、512×512 方阵，分块尺寸取 16、32、64，分别使用单缓冲和双缓冲，共 54 个配置。

编译产物保存在本实验的 `build/`，数据保存在本实验的 `results/`。修改 RTL 或 C++ 后重新运行 `build.py`，先通过快速验证，再运行正式测量。构建脚本按源码、编译器、Verilator 版本和硬件参数缓存各个模型。

## 设置构建参数

| 参数 | 默认值 | 含义 |
|---|---|---|
| `--array` | 4 8 16 | 脉动阵列边长，可选 4、8、16，可指定一个或多个 |
| `--spm-tile` | 64 | SPM 可保存的最大 M、N 分块尺寸，为阵列边长的整数倍 |
| `--spm-k` | 128 | SPM 可保存的最大 K 分块尺寸 |
| `--jobs` | 2 | 单个模型的编译并发数 |

A、B 各有两个物理缓冲区，C 有一个缓冲区；计算读取时，A 按行、B 按列分 bank，C 按列分 bank。A 每个 64 位存储字保存四个 int16 元素。

只构建 8×8 阵列：

```bash
python3 build.py --array 8 --spm-tile 64 --spm-k 128
```

## 设置运行参数

以下参数用于 `run.py`。运行时 `--spm-tile`、`--spm-k` 与已构建模型一致；M、N 分块不超过 `spm-tile`，K 分块不超过 `spm-k`。

| 参数 | 默认值 | 含义 |
|---|---|---|
| `--m` | 128 256 512 | A、C 的行数，可指定一个或多个 |
| `--k` | 每组与 M 相同 | A 的列数、B 的行数 |
| `--n` | 每组与 M 相同 | B、C 的列数 |
| `--array` | 4 8 16 | 使用的阵列边长，可选 4、8、16，可指定一个或多个 |
| `--tile` | 16 32 64 | M、K、N 的分块尺寸，可指定一个或多个 |
| `--tile-m`、`--tile-k`、`--tile-n` | 未设置 | 分别覆盖 M、K、N 分块尺寸 |
| `--buffering` | single double | 单缓冲或双缓冲，可指定一个或两个 |
| `--latency` | 12 | 读请求首拍延迟及写响应延迟，单位周期 |
| `--beat-cycles` | 1 | 相邻数据拍的最小间隔，单位周期 |
| `--stall-percent` | 6 | 地址握手、首个数据拍产生停顿的百分比 |
| `--seed` | 2026 | 矩阵与握手停顿的随机种子 |
| `--repeat` | 1 | 每个配置的仿真次数 |
| `--spm-tile`、`--spm-k` | 64、128 | 已构建模型的 SPM 参数 |
| `--output` | results/ | 数据输出目录 |
| `--verify` | 关闭 | 在计时之外逐元素或逐桶核对结果 |
| `--max-cycles` | 100000000 | 每次仿真的工作周期上限，超限明确报错 |
| `--save-output` | 关闭 | 将每个配置的 C 保存为行优先 int32 二进制文件 |
| `--trace` | 未设置 | 保存首个执行配置的 VCD 波形 |
| `--trace-start` | 0 | 波形记录起始周期，包含复位周期 |
| `--trace-cycles` | 4096 | 波形记录的周期数 |

单缓冲按加载、计算顺序使用输入 SPM。双缓冲在计算当前 K 分块时，将下一 K 分块加载到另一个缓冲区；切换后继续累加同一个 C 分块。

默认 A、B 的元素为 −3 到 3 的整数。矩阵内容由全局元素索引和 seed 确定，各阵列、分块和缓冲配置使用相同的矩阵。

## 参数范围与错误提示

M、K、N、分块、repeat、beat-cycles、trace-cycles、max-cycles 必须为正整数；latency、trace-start 可为 0。M/N 分块不超过 `spm-tile`，K 分块不超过 `spm-k`；`spm-tile` 必须是所选阵列边长的整数倍。`stall-percent` 为 0～99，`seed` 为 0～4294967295。

程序与脚本会拒绝未知参数、缺失参数值和非法配置，并给出明确原因。可执行文件和 `run.py` 均支持 `--help`。

超过周期上限时先检查矩阵、分块和握手参数；确认任务确实更大后再提高 `--max-cycles`。模型不会在非法容量下继续计算或静默使用其他缓冲方式。

## 单独运行一个配置

```bash
python3 run.py --m 256 --k 384 --n 320 --array 8 --tile 32 \
  --buffering double --output results/rectangular
```

分别设置三维分块尺寸：

```bash
python3 run.py --m 512 --array 8 --tile-m 32 --tile-k 128 --tile-n 64 \
  --buffering double --output results/tile_32_128_64
```

直接运行已构建的模型，通过 CSV 获取单次运行数据：

```bash
mkdir -p results/single
./build/a8_t64_k128/sim --m 256 --k 256 --n 256 \
  --tile-m 32 --tile-k 32 --tile-n 32 --buffering double \
  --latency 12 --beat-cycles 1 --stall-percent 6 --seed 2026 \
  > results/single/double_buffer.csv
```

## 分组运行

改变软件 DRAM 的首拍与写响应延迟：

```bash
for latency in 12 24 48; do
  python3 run.py --latency "$latency" --output "results/latency_$latency"
done
```

改变数据拍间隔：

```bash
for interval in 1 2 4; do
  python3 run.py --beat-cycles "$interval" --output "results/beat_$interval"
done
```

保存 C 和波形：

```bash
python3 run.py --m 256 --array 8 --tile 32 --buffering double \
  --save-output --trace results/waveform.vcd --trace-start 0 --trace-cycles 4096 \
  --output results/waveform
```

波形查看步骤与状态编码见网站的“分块、双缓冲与周期”教程。也可以使用本实验自带的波形绘图脚本，先生成一张无需额外查看器的 PNG：

```bash
python3 plot_waveform.py --input results/waveform.vcd --output results/waveform.png
```

在交互式查看器中打开 VCD 时，添加 `TOP.matrix_accelerator` 下的信号；信号包括 `state`、`current_buffer`、`buffer_ready`、`step_q`、`compute_active`、`ar_valid/ready`、`r_valid/ready`、`aw_valid/ready`、`w_valid/ready` 和 `mac_count`。

## 软件 DRAM 接口

供数方式参考 [ZirconSim](https://github.com/MAdrid1011/ZirconSim)。软件存储按字节寻址，接口每拍传输 64 位；读请求给出起始地址和拍数，响应数据保持到握手完成。写请求、写数据和写响应分别握手，写数据使用字节 strobe。

每个周期先驱动存储接口，采样本周期握手，再推进 RTL 时钟并更新软件事务。`latency` 设置读请求到首拍的延迟，以及最后一个写数据拍到写响应的延迟；`beat-cycles` 设置逐拍间隔。随机停顿按 seed 重现。读数据从请求地址开始逐拍增加 8 字节，DMA 对尾部元素进行有效范围处理。

## 怎样完成一组观察

每次固定输入与其他条件，只改变表中的一项。先核对正确性，再读取数据解释变化：

| 固定条件 | 改变哪一项 | 从哪里读数据 | 要回答的问题 |
|---|---|---|---|
| 矩阵、阵列、三维分块和 DRAM 参数固定 | single → double | cycles、input_wait_cycles、overlap_read_beats | 下一 K 块是否能在当前块计算时准备好？ |
| 矩阵、阵列、缓冲和 DRAM 参数固定 | tile=16 → 32 → 64 | read_bytes、read_requests、cycles | 增大分块减少了哪些重复加载与控制开销？ |

为每组比较保留完整命令、环境文件、原始记录和汇总表；用一张对比表记录参数、耗时或周期，再写出“观察到什么、实现中哪一步能解释它”。性能趋势取决于机器与输入，不要求某个版本一定更快。

## 输出文件

| 文件 | 内容 |
|---|---|
| `raw.csv` | 每次仿真的周期与数据量；`record` 区分 `summary`、`trial` |
| `summary.csv` | 各矩阵、阵列、分块和缓冲配置的汇总 |
| `implementation_comparison.csv` | 各配置相对当前阵列基准分块、缓冲方式的周期加速比 |
| `array_scaling.csv` | 相同矩阵、分块和缓冲方式下，相对最小阵列的周期加速比 |
| `dram_traffic.csv` | 每次矩阵乘法的 DRAM 读写总量 |
| `buffering_comparison.csv` | 相同矩阵、阵列和分块下，双缓冲相对单缓冲的周期加速比 |
| `environment.json` | 矩阵、硬件、DRAM、构建环境和测量设置 |
| `outputs/*.bin` | 使用 `--save-output` 时保存的行优先 int32 结果 |


终端的 `i/total` 统计独立配置启动次数，每次启动还包含自己的预热与重复测量。`raw.csv` 在每个配置完成后保存，`summary.csv` 和最终图表在全部配置完成后生成。按 Ctrl+C 中断会保留已完成的原始记录；部分数据不能视为完整扫描结果。再次运行请换一个 `--output` 目录；脚本会重新测量，不自动续跑。

默认基准为相同阵列与矩阵下的单缓冲、16×16×16 分块。修改运行列表后，基准使用请求列表中的第一种缓冲方式与最小分块。`--repeat` 大于 1 时选取周期数中位数对应的记录；矩阵内容不变，第 t 次仿真的握手种子为 `seed + t × 7919`。

| 字段 | 含义 |
|---|---|
| `cycles` | 从 start 后的首个工作周期到 DUT done 的周期数，包含最后写响应和控制器完成周期 |
| `compute_cycles` | 阵列推进周期数，包含流水线填充和排空 |
| `input_wait_cycles` | 等待当前输入 SPM 准备完成的周期数 |
| `output_cycles` | C 写 DMA 处于工作状态的周期数 |
| `overlap_cycles` | 阵列推进与输入 DMA 同时工作的周期数 |
| `overlap_read_beats` | 阵列推进期间接收的 DRAM 数据拍数 |
| `macs` | PE 实际执行的有效乘加次数 |
| `macs_per_cycle` | `macs / cycles` |
| `pe_utilization` | `macs / (cycles × array²)` |
| `compute_utilization` | `macs / (compute_cycles × array²)` |
| `read_requests`、`write_requests` | DRAM 读、写突发请求数 |
| `read_bytes`、`write_bytes` | DRAM 实际读、写字节数；读尾拍包含填充，写入按 strobe 统计 |
| `spm_bytes` | 两组 A、B 缓冲和一组 C 存储的总字节数 |
| `speedup` | 基准配置周期数 / 当前配置周期数 |
| `baseline_buffering`、`baseline_tile_m/k/n` | 加速比使用的基准配置 |
| `host_seconds` | 宿主机驱动仿真的耗时，单位秒 |
| `checksum` | C 的全部元素之和 |

复位和输入矩阵生成不计入 `cycles`。DMA、SPM 初始化、阵列计算、流水线填充排空和结果回写计入 `cycles`。
