# Lab2：OpenMP 矩阵转置

将行优先的稠密方阵 A 转置到行优先方阵 B：

```text
A: N × N，uint32_t，行优先
B: N × N，uint32_t，行优先
B[j,i] = A[i,j]
```

各实现提供完整源码，程序直接执行矩阵生成、转置和计时。

## 源码结构

```text
Lab2/
  include/
    matrix.hpp                 方阵和输入生成接口
    transpose.hpp              转置内核和线程统计
    benchmark.hpp              运行参数
    support.hpp                计时、随机数和 CSV 工具
    validation.hpp             命令行值与范围检查
    verification.hpp           参考结果核对接口
  src/
    main.cpp                   命令行参数
    matrix.cpp                 输入矩阵生成
    transpose_serial.cpp       串行转置
    transpose_parallel.cpp     按行并行转置
    transpose_tiled.cpp        分块并行转置
    benchmark.cpp              预热、测量和 CSV 输出
    verification.cpp           逐元素或逐桶参考比较
  cli_utils.py                参数与构建目录检查
  run.py                       顺序测量和汇总
```

## 环境准备

进入 Lab2，后续命令都在这个文件夹内执行：

```bash
cd Lab2
```

Windows 使用 WSL2 的 Ubuntu 终端安装依赖：

```bash
sudo apt update
sudo apt install -y g++ cmake python3 python3-venv
```

macOS 安装依赖并选择支持 OpenMP 的编译器：

```bash
brew install cmake python llvm libomp
export CXX="$(brew --prefix llvm)/bin/clang++"
export OpenMP_ROOT="$(brew --prefix libomp)"
```

安装本实验的 Python 依赖：

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
```

## 第一次运行：快速验证

先运行这组小配置，确认环境、程序和输出均正常：

```bash
python3 run.py --n 256 --tile 16 32 --repeat 1 --rounds 1 \
  --verify --output results/quick
```

终端会显示 `Lab2 i/11` 的配置进度，并出现 `VERIFY PASS: 65536 elements`。PASS 后的误差或说明因实验而异。运行完成后，检查 `results/quick/environment.json`、`raw.csv`、`summary.csv` 和比较图表。

`--verify` 在计时之外逐元素或逐桶比较参考结果。错误会显示 `VERIFY FAIL`、第一个错误位置与期望值，并以非零退出码停止。浮点乘法使用 `1e-10 + 1e-9 × |参考值|` 的误差阈值；转置、计数和整数 GEMM 精确比较。**小配置只确认功能，不能用于正式性能结论。**

## 完整测量

确认快速验证通过后，再运行完整默认扫描：

```bash
python3 run.py
```


完整默认扫描会启动 255 次独立程序；最大一组输入和输出矩阵合计约 512 MiB。先完成快速验证，完整扫描可能需要数分钟。默认扫描 N=2048、3072、4096、6144、8192 五组方阵。每组运行串行、按行并行，以及 tile=16、32、64、128 的分块转置，扫描 1、2、4 线程。编译产物保存在本实验的 `build/`，运行数据保存在本实验的 `results/`。

## 设置运行参数

以下参数用于 `run.py`：

| 参数 | 默认值 | 含义 |
|---|---|---|
| `--n` | 2048 3072 4096 6144 8192 | 方阵边长列表，可指定一个或多个值 |
| `--tile` | 16 32 64 128 | 转置分块边长列表，可指定一个或多个值 |
| `--threads` | 4 | 指定线程数，同时运行 1 线程；指定值不小于 2 时也运行 2 线程，重复值合并 |
| `--repeat` | 7 | 每次独立执行的测量次数 |
| `--rounds` | 3 | 测量批次数 |
| `--seed` | 2026 | 输入矩阵的随机种子 |
| `--output` | `results/` | 输出目录 |
| `--verify` | 关闭 | 在计时之外逐元素或逐桶核对结果 |
| `--build-dir` | `build/` | 选择构建目录；相对路径从本 Lab 目录解析 |

指定全部主要参数：

```bash
python3 run.py --n 2048 3072 4096 6144 8192 --tile 16 32 64 128 \
  --threads 4 --repeat 7 --rounds 3 --seed 2026
```

修改 C++ 源码后，先执行 `cmake --build build --parallel 4` 并重新运行快速验证，再进行正式测量。

## 参数范围与错误提示

N、tile、threads、repeat、rounds 必须为正整数；tile 可以大于 N，尾部自动截断。`seed` 为 0～4294967295。

程序与脚本会拒绝未知参数、缺失参数值和非法配置，并给出明确原因。可执行文件和 `run.py` 均支持 `--help`。

切换到新构建目录时，构建和测量必须选择同一个目录，例如：

```bash
cmake -S . -B build-llvm -DCMAKE_BUILD_TYPE=Release
cmake --build build-llvm --parallel 4
python3 run.py --build-dir build-llvm --help
```

实际测量时，把 `--build-dir build-llvm` 加到上面的快速验证命令或自己的参数组合中。`environment.json` 会记录实际构建目录与编译器。

## 输入矩阵

输入和输出均为 N × N 的 `uint32_t` 方阵。输入由 `seed` 生成；相同 N 和 seed 下，各实现使用相同输入。矩阵元素按行存储，分块末尾的行列范围截断到 N。

## 单独运行一个实现

直接运行可执行文件，通过 `--kernel` 选择实现：

| 实现名称 | 参数 | 任务单位 |
|---|---|---|
| Serial | `--kernel serial` | 整个矩阵 |
| Naive | `--kernel naive` | 一行，OpenMP static 分配 |
| Tiled | `--kernel tiled` | tile × tile 区域，OpenMP static 分配 |

以下命令使用相同的矩阵参数，在 Bash 或 zsh 终端中执行：

```bash
transpose_args=(--n 2048 --tile 32 --threads 4 --repeat 7 --seed 2026)

./build/lab2_locality "${transpose_args[@]}" --kernel serial
./build/lab2_locality "${transpose_args[@]}" --kernel naive
./build/lab2_locality "${transpose_args[@]}" --kernel tiled
```

可执行文件向终端输出 CSV。保存到文件时，在命令末尾加上重定向：

```bash
mkdir -p results/single
./build/lab2_locality "${transpose_args[@]}" --kernel tiled > results/single/tiled.csv
```

## 分组运行

指定一组矩阵规模和多个分块边长：

```bash
python3 run.py --n 4096 --tile 16 32 64 128 --output results/n4096
```

指定多组矩阵规模和一个分块边长：

```bash
python3 run.py --n 2048 4096 8192 --tile 64 --output results/tile64
```

## 怎样完成一组观察

每次固定输入与其他条件，只改变表中的一项。先核对正确性，再读取数据解释变化：

| 固定条件 | 改变哪一项 | 从哪里读数据 | 要回答的问题 |
|---|---|---|---|
| N 和线程数固定 | Naive → Tile16 → Tile32 → Tile64 | median_ms、effective_gbps、updates | 为什么写入同样数量的元素，耗时仍不同？ |
| N、内核和 tile 固定 | threads=1、2、4 | thread_speedup 与原始多次时间 | 增加线程的收益是否持续？ |

为每组比较保留完整命令、环境文件、原始记录和汇总表；用一张对比表记录参数、耗时或周期，再写出“观察到什么、实现中哪一步能解释它”。性能趋势取决于机器与输入，不要求某个版本一定更快。

## 输出文件

`run.py` 默认将本次运行的数据保存到本实验的 `results/`，使用 `--output` 可以更改目录。

| 文件 | 内容 |
|---|---|
| `raw.csv` | 逐次耗时和每线程统计；`record` 区分 `summary`、`thread`、`trial` |
| `summary.csv` | 各矩阵规模、实现、分块边长和线程数的汇总数据 |
| `implementation_comparison.csv` | 各实现相对串行转置的加速比数据 |
| `thread_scaling.csv` | 同一实现相对自身单线程的加速比数据 |
| `environment.json` | 矩阵参数、编译器、CPU 和测量设置 |

默认每次独立执行先预热一次，再测量七次并取中位数；进行三个批次后再取中位数。计时包含并行区域启动、循环调度和屏障；矩阵生成、B 清零、输出汇总和 checksum 计算在计时之外。


终端的 `i/total` 统计独立配置启动次数，每次启动还包含自己的预热与重复测量。`raw.csv` 在每个配置完成后保存，`summary.csv` 和最终图表在全部配置完成后生成。按 Ctrl+C 中断会保留已完成的原始记录；部分数据不能视为完整扫描结果。再次运行请换一个 `--output` 目录；脚本会重新测量，不自动续跑。

常用 CSV 字段：

| 字段 | 含义 |
|---|---|
| `kernel`、`n`、`tile`、`threads` | 实现、矩阵边长、分块边长和实际线程数 |
| `variant` | 实现与分块配置的名称，例如 `Naive`、`Tile32` |
| `median_ms` | 汇总耗时，单位毫秒 |
| `speedup` | 串行转置耗时 / 当前方法耗时 |
| `thread_speedup` | 同实现、同 tile 的单线程耗时 / 多线程耗时 |
| `effective_gbps` | `2 × N × N × 4 / elapsed_s / 1e9`，按逻辑读写字节数计算 |
| `row_visits` | 行访问次数，分块实现包含不同列块对行的重复访问 |
| `tile_visits` | 处理的分块数 |
| `updates` | 写入的矩阵元素数 |
| `compute_s`、`wait_s` | 每线程计算和等待耗时，单位秒 |
| `checksum` | 输出矩阵的元素之和 |

如果实际线程数少于请求值，脚本会明确指出差异并停止汇总。检查 `OMP_THREAD_LIMIT` 等设置，或改用允许的 `--threads`；不要把实际两线程的记录当作四线程结果。
