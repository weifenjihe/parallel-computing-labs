# Lab4：MPI 稀疏矩阵乘法

计算 CSR 稀疏矩阵 A 与行优先稠密矩阵 B 的乘积：

```text
A: M × K，CSR
B: K × N，行优先
C: M × N，行优先
C = A × B
```

每个 MPI 进程保存 A 的连续行段，并计算 C 的对应行段。B 的行按连续区间分配给各进程；根据所选实现，进程接收完整 B 或所需的 B 行。所有实现使用相同的行块、输出列块和 32 列微块计算内核。

## 源码结构

```text
Lab4/
  include/
    config.hpp                运行参数
    matrix.hpp                CSR、稠密矩阵和行块
    distribution.hpp          A、B 的进程划分
    communication.hpp         按需交换计划和缓冲区
    spmm.hpp                  乘法接口和进程统计
    benchmark.hpp             测量接口
    support.hpp               随机数和 CSV 工具
    validation.hpp             命令行值与范围检查
    verification.hpp           参考结果核对接口
  src/
    main.cpp                  MPI 初始化和命令行参数
    matrix.cpp                按全局索引生成本地矩阵
    distribution.cpp          按行数或非零数划分 A
    communication_plan.cpp    B 行去重、请求交换和列索引映射
    spmm_tiled.cpp            行块、列块与 32 列微块
    spmm_full.cpp             串行、完整 B 广播与收集
    spmm_halo.cpp             所需 B 行的阻塞交换
    spmm_overlap.cpp          所需 B 行的非阻塞交换和本地计算
    benchmark.cpp             预热、测量和 CSV 输出
    verification.cpp           逐元素或逐桶参考比较
  cli_utils.py                参数与构建目录检查
  run.py                      顺序测量和汇总
```

## 环境准备

进入 Lab4，后续命令都在这个文件夹内执行：

```bash
cd Lab4
```

Windows 使用 WSL2 的 Ubuntu 终端安装依赖：

```bash
sudo apt update
sudo apt install -y g++ cmake python3 python3-venv openmpi-bin libopenmpi-dev
```

macOS 安装依赖：

```bash
brew install cmake python open-mpi
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
python3 run.py --m 128 --k 256 --n 64 --nnz-per-row 8 \
  --profile uniform banded --repeat 1 --rounds 1 --verify --output results/quick
```

终端会显示 `Lab4 i/28` 的配置进度，并出现 `VERIFY PASS: 8192 distributed elements`。PASS 后的误差或说明因实验而异。运行完成后，检查 `results/quick/environment.json`、`raw.csv`、`summary.csv` 和比较图表。

`--verify` 在计时之外逐元素或逐桶比较参考结果。错误会显示 `VERIFY FAIL`、第一个错误位置与期望值，并以非零退出码停止。浮点乘法使用 `1e-10 + 1e-9 × |参考值|` 的误差阈值；转置、计数和整数 GEMM 精确比较。**小配置只确认功能，不能用于正式性能结论。**

## 完整测量

确认快速验证通过后，再运行完整默认扫描：

```bash
python3 run.py
```


完整默认扫描会启动 504 次 MPI 作业，进程启动与重复计算会让它耗时数分钟。默认 M=4096、8192、16384，K=8192、N=1024，平均每行 32 个非零元素。运行四种输入分布和各个实现，扫描 1、2、4 进程。编译产物保存在本实验的 `build/`，运行数据保存在本实验的 `results/`。

## 设置运行参数

以下参数用于 `run.py`：

| 参数 | 默认值 | 含义 |
|---|---|---|
| `--m` | 4096 8192 16384 | A 和 C 的行数，可指定一个或多个 |
| `--k` | 8192 | A 的列数、B 的行数 |
| `--n` | 1024 | B 和 C 的列数 |
| `--nnz-per-row` | 32 | A 平均每行的非零元素数 |
| `--ranks` | 4 | 指定进程数，同时运行 1 进程；指定值不小于 2 时也运行 2 进程，重复值合并 |
| `--partition` | nnz | A 的行划分方式：`rows` 或 `nnz` |
| `--profile` | uniform skewed blocked banded | 输入分布，可指定一个或多个 |
| `--block-nnz` | 256 | 进程内部行块的目标非零元素数 |
| `--tile` | 128 | 输出列块宽度 |
| `--repeat` | 7 | 每次独立执行的测量次数 |
| `--rounds` | 3 | 测量批次数 |
| `--seed` | 2026 | 输入矩阵的随机种子 |
| `--hostfile` | 未设置 | MPI 主机列表；未设置时在当前电脑运行 |
| `--output` | results/ | 输出目录 |
| `--verify` | 关闭 | 在计时之外逐元素或逐桶核对结果 |
| `--build-dir` | `build/` | 选择构建目录；相对路径从本 Lab 目录解析 |

指定矩阵参数和输入分布：

```bash
python3 run.py --m 4096 8192 16384 --k 8192 --n 1024 \
  --nnz-per-row 32 --ranks 4 --partition nnz \
  --profile uniform skewed blocked banded --tile 128 --block-nnz 256 \
  --repeat 7 --rounds 3 --seed 2026
```

修改 C++ 源码后，先执行 `cmake --build build --parallel 4` 并重新运行快速验证，再进行正式测量。

## 参数范围与错误提示

维度、ranks、tile、block-nnz、repeat、rounds 必须为正整数。`nnz-per-row` 可为 0，但不能超过 K；`seed` 为 0～4294967295。直接运行 Serial 必须使用 1 个进程。K×N 不能超过 MPI 的 int 元素计数范围。

程序与脚本会拒绝未知参数、缺失参数值和非法配置，并给出明确原因。可执行文件和 `run.py` 均支持 `--help`。

切换到新构建目录时，构建和测量必须选择同一个目录，例如：

```bash
cmake -S . -B build-llvm -DCMAKE_BUILD_TYPE=Release
cmake --build build-llvm --parallel 4
python3 run.py --build-dir build-llvm --help
```

实际测量时，把 `--build-dir build-llvm` 加到上面的快速验证命令或自己的参数组合中。`environment.json` 会记录实际构建目录与编译器。

## 输入分布

- `uniform`：各行非零数相同，列位置分散。
- `skewed`：较多非零元素集中在前约 1/8 行。
- `blocked`：每 32 行共享紧凑列窗口。
- `banded`：第 i 行的非零元素位于 `i × K / M` 附近的列窗口，窗口越过矩阵末尾时从列 0 继续。

前三种输入沿用 Lab1 的生成方式。相同 M、K、N、平均非零数和 seed 下，A、B 的元素与 Lab1 相同。四种 profile 都保存 `M × nnz-per-row` 个非零元素。各进程按全局索引生成自己的数据。

`--partition rows` 按行数划分 A；`--partition nnz` 根据累计非零数划分连续行段，单行不拆分。B 始终按行数划分为连续区间。

## 单独运行一个实现

| 实现名称 | 参数 | 数据交换与计算方式 |
|---|---|---|
| Serial | `--kernel serial`，1 进程 | 使用完整 A、B 计算 C |
| Broadcast | `--kernel broadcast` | rank 0 广播完整 B，各进程计算本地 C |
| Allgather | `--kernel allgather` | 各进程提供自己拥有的 B 行，收集完整 B 后计算 |
| Halo | `--kernel halo` | 根据本地 A 的列索引，只交换所需的远程 B 行，再计算 |
| Overlap | `--kernel overlap` | 发布所需 B 行的非阻塞交换，先计算本地 B 对应部分，等待后累加远程部分 |

以下命令使用相同矩阵参数，在 Bash 或 zsh 终端中执行：

```bash
spmm_args=(--m 8192 --k 8192 --n 1024 --nnz-per-row 32
  --profile banded --partition nnz --repeat 7 --seed 2026
  --block-nnz 256 --tile 128)

mpiexec -n 1 ./build/lab4_spmm "${spmm_args[@]}" --kernel serial
mpiexec -n 4 ./build/lab4_spmm "${spmm_args[@]}" --kernel broadcast
mpiexec -n 4 ./build/lab4_spmm "${spmm_args[@]}" --kernel allgather
mpiexec -n 4 ./build/lab4_spmm "${spmm_args[@]}" --kernel halo
mpiexec -n 4 ./build/lab4_spmm "${spmm_args[@]}" --kernel overlap
```

将 `--profile banded` 替换为 `uniform`、`skewed` 或 `blocked`，即可切换输入分布。进程数通过 `mpiexec -n` 设置。

Open MPI 提示 slots 不足时，在 `mpiexec` 后添加 `--oversubscribe`。`run.py` 使用 Open MPI 时自动添加该选项。

可执行文件向终端输出 CSV。保存单次运行的数据：

```bash
mkdir -p results/single
mpiexec -n 4 ./build/lab4_spmm "${spmm_args[@]}" --kernel halo \
  > results/single/halo.csv
```

## 分组运行

改变 A 的进程划分方式，将每组输出保存到独立目录：

```bash
for partition in rows nnz; do
  python3 run.py --partition "$partition" --output "results/partition_$partition"
done
```

改变输出列块宽度：

```bash
for tile in 32 64 128 256; do
  python3 run.py --tile "$tile" --output "results/tile_$tile"
done
```

多台主机运行时，在主机列表中逐行填写主机名及 slots，例如：

```text
node-a slots=2
node-b slots=2
```

各主机安装兼容的 MPI，使用相同架构的可执行文件，并使 `Lab4` 的绝对路径一致。配置主机之间免密 SSH 后，在其中一台执行：

```bash
python3 run.py --hostfile hosts.txt --ranks 4 --output results/multiple_hosts
```

## 怎样完成一组观察

每次固定输入与其他条件，只改变表中的一项。先核对正确性，再读取数据解释变化：

| 固定条件 | 改变哪一项 | 从哪里读数据 | 要回答的问题 |
|---|---|---|---|
| 输入、划分和进程数固定 | Broadcast → Allgather → Halo → Overlap | median_ms、received_mib；各 rank 的 local_nnz / remote_nnz | 减少接收的数据量是否一定减少总耗时？ |
| 输入、内核、tile 固定 | rows → nnz 划分 | 各 rank 的 nnz、row_begin/end、rank_elapsed_s | 谁承担了更多计算，整体为什么等待最慢进程？ |

为每组比较保留完整命令、环境文件、原始记录和汇总表；用一张对比表记录参数、耗时或周期，再写出“观察到什么、实现中哪一步能解释它”。性能趋势取决于机器与输入，不要求某个版本一定更快。

## 输出文件

`run.py` 默认将本次运行的数据保存到本实验的 `results/`，使用 `--output` 可以更改目录。

| 文件 | 内容 |
|---|---|
| `raw.csv` | 逐次耗时和每进程统计；`record` 区分 `summary`、`rank`、`trial` |
| `summary.csv` | 各规模、输入、实现和进程数的汇总数据 |
| `implementation_comparison.csv` | 各实现相对对应串行分块内核的加速比数据 |
| `process_scaling.csv` | 同一实现相对自身单进程的加速比数据 |
| `communication_volume.csv` | 每次乘法中各进程接收 B 数据的总量 |
| `environment.json` | 矩阵参数、编译器、CPU、MPI 和测量设置 |

默认每次独立执行先预热一次，再测量七次，取各次最大进程耗时的中位数；进行三个批次后再取中位数。计时包含 B 数据打包、数据交换及本地乘法。输入生成、划分、交换计划构造、C 清零、计时前屏障和输出汇总位于计时之外。C 保留在各进程中，checksum 在计时后汇总。


终端的 `i/total` 统计独立配置启动次数，每次启动还包含自己的预热与重复测量。`raw.csv` 在每个配置完成后保存，`summary.csv` 和最终图表在全部配置完成后生成。按 Ctrl+C 中断会保留已完成的原始记录；部分数据不能视为完整扫描结果。再次运行请换一个 `--output` 目录；脚本会重新测量，不自动续跑。

常用 CSV 字段：

| 字段 | 含义 |
|---|---|
| `elapsed_s`、`median_ms` | 各进程乘法耗时的最大值，分别为秒和汇总毫秒 |
| `speedup` | 对应输入的串行分块内核耗时 / 当前方法耗时 |
| `process_speedup` | 当前方法单进程耗时 / 当前方法多进程耗时 |
| `gflops` | `2 × nnz × N / elapsed_s / 1e9` |
| `row_begin`、`row_end` | 进程持有的 A 行范围，左闭右开 |
| `nnz`、`local_nnz`、`remote_nnz` | 非零数，以及引用本地或远程 B 行的非零数 |
| `send_bytes`、`receive_bytes` | 发送、接收 B 元素的逻辑字节数；汇总行对各进程求和 |
| `received_mib` | 接收 B 数据总量，单位 MiB |
| `compute_s` | 本地分块乘法耗时 |
| `pack_s` | 按需实现整理本地及待发送 B 行的耗时 |
| `mpi_s` | B 数据通信接口调用与等待耗时，包含 `wait_s` |
| `wait_s` | Overlap 等待非阻塞请求完成的耗时 |
| `rank_elapsed_s` | 当前进程的乘法耗时 |
| `setup_s` | 输入生成、划分和交换计划准备耗时，各进程取最大值 |
| `checksum` | 全部 C 元素之和 |

`summary` 和 `trial` 行的阶段耗时分别取各进程最大值，不能将这些最大值相加作为总耗时。通信字节数只统计 B 元素，排除交换计划元数据、协议开销和广播树的转发流量。
