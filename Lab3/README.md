# Lab3：OpenMP 直方图

对整数输入统计直方图：

```text
input: N 个整数，每个值属于 [0, bins)
counts: bins 个 uint64_t 计数
counts[input[i]] += 1
```

各实现提供完整源码，程序直接执行输入生成、计数和计时。

## 源码结构

```text
Lab3/
  include/
    input.hpp                  输入和计数类型
    histogram.hpp              计数内核、工作空间和线程统计
    benchmark.hpp              运行参数
    support.hpp                计时、随机数和 CSV 工具
    validation.hpp             命令行值与范围检查
    verification.hpp           参考结果核对接口
  src/
    main.cpp                   命令行参数
    input.cpp                  输入生成
    workspace.cpp              分段锁和线程私有计数空间
    histogram_serial.cpp       串行计数
    histogram_critical.cpp     全局临界区
    histogram_atomic.cpp       原子加
    histogram_locks.cpp        分段锁
    histogram_private.cpp      线程私有计数和合并
    benchmark.cpp              预热、测量和 CSV 输出
    verification.cpp           逐元素或逐桶参考比较
  cli_utils.py                参数与构建目录检查
  run.py                       顺序测量和汇总
```

## 环境准备

进入 Lab3，后续命令都在这个文件夹内执行：

```bash
cd Lab3
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
python3 run.py --n 10000 --stripes 1 16 --repeat 1 --rounds 1 \
  --verify --output results/quick
```

终端会显示 `Lab3 i/34` 的配置进度，并出现 `VERIFY PASS: 256 buckets`。PASS 后的误差或说明因实验而异。运行完成后，检查 `results/quick/environment.json`、`raw.csv`、`summary.csv` 和比较图表。

`--verify` 在计时之外逐元素或逐桶比较参考结果。错误会显示 `VERIFY FAIL`、第一个错误位置与期望值，并以非零退出码停止。浮点乘法使用 `1e-10 + 1e-9 × |参考值|` 的误差阈值；转置、计数和整数 GEMM 精确比较。**小配置只确认功能，不能用于正式性能结论。**

## 完整测量

确认快速验证通过后，再运行完整默认扫描：

```bash
python3 run.py
```


完整默认扫描会启动 414 次独立程序。临界区和热点锁更新会串行化，完整扫描可能超过十分钟；首次操作请用上面的快速验证。默认扫描 100 万、500 万、1000 万个输入，运行 uniform 和 hotspot 两种分布。每组输入运行串行、全局临界区、原子加、1/4/16/64 把分段锁和线程私有计数，扫描 1、2、4 线程。编译产物保存在本实验的 `build/`，运行数据保存在本实验的 `results/`。

## 设置运行参数

以下参数用于 `run.py`：

| 参数 | 默认值 | 含义 |
|---|---|---|
| `--n` | 1000000 5000000 10000000 | 输入元素数列表 |
| `--bins` | 256 | bucket 数 |
| `--stripes` | 1 4 16 64 | 分段锁数量列表 |
| `--distribution` | uniform hotspot | 输入分布列表 |
| `--hot-percent` | 90 | hotspot 分布中直接映射到 bucket 0 的比例，单位百分比 |
| `--threads` | 4 | 指定线程数，同时运行 1 线程；指定值不小于 2 时也运行 2 线程，重复值合并 |
| `--repeat` | 7 | 每次独立执行的测量次数 |
| `--rounds` | 3 | 测量批次数 |
| `--seed` | 2026 | 输入随机种子 |
| `--output` | `results/` | 输出目录 |
| `--verify` | 关闭 | 在计时之外逐元素或逐桶核对结果 |
| `--build-dir` | `build/` | 选择构建目录；相对路径从本 Lab 目录解析 |

指定全部主要参数：

```bash
python3 run.py --n 1000000 5000000 10000000 --bins 256 --stripes 1 4 16 64 \
  --distribution uniform hotspot --hot-percent 90 --threads 4 \
  --repeat 7 --rounds 3 --seed 2026
```

修改 C++ 源码后，先执行 `cmake --build build --parallel 4` 并重新运行快速验证，再进行正式测量。

## 参数范围与错误提示

N、bins、stripes、threads、repeat、rounds 必须为正整数；锁数可以超过桶数。`hot-percent` 为 0～100，`seed` 为 0～4294967295。

程序与脚本会拒绝未知参数、缺失参数值和非法配置，并给出明确原因。可执行文件和 `run.py` 均支持 `--help`。

切换到新构建目录时，构建和测量必须选择同一个目录，例如：

```bash
cmake -S . -B build-llvm -DCMAKE_BUILD_TYPE=Release
cmake --build build-llvm --parallel 4
python3 run.py --build-dir build-llvm --help
```

实际测量时，把 `--build-dir build-llvm` 加到上面的快速验证命令或自己的参数组合中。`environment.json` 会记录实际构建目录与编译器。

## 输入分布

- `uniform`：输入分散到全部 bucket。
- `hotspot`：按 `hot-percent` 指定的比例映射到 bucket 0，其余输入分散到全部 bucket。

hotspot 剩余输入也可能映射到 bucket 0，因此实际 bucket 0 占比略高于设置值。相同 N、bins、distribution、hot-percent 和 seed 下，各实现使用相同输入。

## 单独运行一个实现

直接运行可执行文件，通过 `--kernel`、`--distribution` 和 `--stripes` 选择实现、分布和锁数：

| 实现名称 | 参数 | 更新方式 |
|---|---|---|
| Serial | `--kernel serial` | 单线程计数 |
| Critical | `--kernel critical` | 每次更新进入同一个全局临界区 |
| Atomic | `--kernel atomic` | 每个计数使用原子加 |
| Lock16 | `--kernel locks --stripes 16` | bucket 按 `bucket % stripes` 映射到分段锁 |
| Private | `--kernel private` | 每线程独立计数，最后按 bucket 合并 |

以下命令使用相同的输入参数，在 Bash 或 zsh 终端中执行：

```bash
histogram_args=(--n 10000000 --bins 256 --distribution uniform
  --hot-percent 90 --threads 4 --repeat 7 --seed 2026)

./build/lab3_histogram "${histogram_args[@]}" --kernel serial
./build/lab3_histogram "${histogram_args[@]}" --kernel critical
./build/lab3_histogram "${histogram_args[@]}" --kernel atomic
./build/lab3_histogram "${histogram_args[@]}" --kernel locks --stripes 16
./build/lab3_histogram "${histogram_args[@]}" --kernel private
```

将 `--distribution uniform` 替换为 `hotspot`，即可切换输入分布。将 `--stripes 16` 替换为其他锁数，运行对应的分段锁配置。

可执行文件向终端输出 CSV。保存到文件时，在命令末尾加上重定向：

```bash
mkdir -p results/single
./build/lab3_histogram "${histogram_args[@]}" --kernel private > results/single/private.csv
```

## 分组运行

固定输入规模，指定多组锁数：

```bash
python3 run.py --n 10000000 --stripes 1 4 16 64 256 --output results/lock_counts
```

固定锁数，指定多组输入规模：

```bash
python3 run.py --n 1000000 5000000 10000000 --stripes 16 --output results/lock16
```

改变热点比例：

```bash
for hot in 50 90 99; do
  python3 run.py --n 10000000 --distribution hotspot --hot-percent "$hot" \
    --output "results/hot_$hot"
done
```

## 怎样完成一组观察

每次固定输入与其他条件，只改变表中的一项。先核对正确性，再读取数据解释变化：

| 固定条件 | 改变哪一项 | 从哪里读数据 | 要回答的问题 |
|---|---|---|---|
| N、bins、分布和线程数固定 | Critical → Atomic → Locks → Private | median_ms；线程 compute_s、merge_s、wait_s | 哪些共享更新被消除，增加了哪些合并工作？ |
| N、bins、线程数和热点比例固定 | stripes=1、4、16、64 | lock_count_speedup；对照 uniform / hotspot | 为什么更多锁不能分散对桶 0 的竞争？ |

为每组比较保留完整命令、环境文件、原始记录和汇总表；用一张对比表记录参数、耗时或周期，再写出“观察到什么、实现中哪一步能解释它”。性能趋势取决于机器与输入，不要求某个版本一定更快。

## 输出文件

`run.py` 默认将本次运行的数据保存到本实验的 `results/`，使用 `--output` 可以更改目录。

| 文件 | 内容 |
|---|---|
| `raw.csv` | 逐次耗时和每线程统计；`record` 区分 `summary`、`thread`、`trial` |
| `summary.csv` | 各输入规模、分布、实现、锁数和线程数的汇总数据 |
| `implementation_comparison.csv` | 各实现相对匹配输入的串行耗时的加速比数据 |
| `thread_scaling.csv` | 同实现、同锁数相对自身单线程的加速比数据 |
| `lock_count_comparison.csv` | 固定线程数下，各锁数相对本组最小锁数的加速比数据 |
| `environment.json` | 输入参数、编译器、CPU 和测量设置 |

默认每次独立执行先预热一次，再测量七次并取中位数；进行三个批次后再取中位数。计时包含并行区域启动、循环调度、计数更新、线程屏障和私有计数合并；输入生成、表清零、锁初始化、输出汇总和 checksum 计算在计时之外。


终端的 `i/total` 统计独立配置启动次数，每次启动还包含自己的预热与重复测量。`raw.csv` 在每个配置完成后保存，`summary.csv` 和最终图表在全部配置完成后生成。按 Ctrl+C 中断会保留已完成的原始记录；部分数据不能视为完整扫描结果。再次运行请换一个 `--output` 目录；脚本会重新测量，不自动续跑。

常用 CSV 字段：

| 字段 | 含义 |
|---|---|
| `kernel`、`distribution`、`n`、`bins`、`stripes`、`threads` | 实现、分布、规模、bucket 数、锁数和实际线程数 |
| `variant` | 实现与锁数配置的名称，例如 `Critical`、`Lock16` |
| `median_ms` | 汇总耗时，单位毫秒 |
| `speedup` | 匹配输入的串行耗时 / 当前方法耗时 |
| `thread_speedup` | 同实现、同锁数的单线程耗时 / 多线程耗时 |
| `lock_count_speedup` | 同线程数下，本组最小锁数的耗时 / 当前锁数的耗时 |
| `lock_count_baseline` | 锁数对比所使用的基线锁数 |
| `million_items_s` | 每秒处理的百万输入元素数 |
| `updates` | 处理的输入元素数 |
| `merged_bins` | 合并的 bucket 数 |
| `compute_s` | 计数和合并阶段耗时，包含计数阶段的锁等待 |
| `wait_s` | 线程屏障等待耗时 |
| `merge_s` | 私有计数合并阶段耗时，已包含在 `compute_s` 中 |
| `checksum` | 全部 bucket 的计数之和 |

如果实际线程数少于请求值，脚本会明确指出差异并停止汇总。检查 `OMP_THREAD_LIMIT` 等设置，或改用允许的 `--threads`；不要把实际两线程的记录当作四线程结果。
