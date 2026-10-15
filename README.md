# Parallel Computing Labs

五个实验分别位于 Lab1–Lab5。每个文件夹包含自己的源码、依赖、构建配置和运行脚本，可以单独复制使用。进入对应文件夹后编译和运行，编译产物与运行数据分别保存到该文件夹的 `build/` 和 `results/`。

| 实验 | 内容 | 实现方式 |
|---|---|---|
| [Lab1](Lab1/README.md) | OpenMP 稀疏矩阵乘法（SpMM） | CSR 串行、按行并行、非零数行块、输出列分块 |
| [Lab2](Lab2/README.md) | 矩阵转置 | 串行、按行并行、分块并行转置 |
| [Lab3](Lab3/README.md) | 直方图 | 全局锁、原子操作、分段锁、线程私有计数 |
| [Lab4](Lab4/README.md) | MPI 稀疏矩阵乘法（SpMM） | 完整 B 广播与收集、所需 B 行交换、通信与计算重叠 |
| [Lab5](Lab5/README.md) | SPM 与脉动阵列稠密矩阵乘法 | 软件 DRAM 供数、分块、单缓冲与双缓冲 |

## 实验网站

实验网站位于 `website/`，包含中国科学院大学《并行处理》研究生课程的实验介绍、教程与操作步骤。本地预览和 GitHub Pages 部署方法见 [网站说明](website/README.md)。

## Windows 环境

在管理员 PowerShell 中安装 WSL2，按提示重启并完成 Ubuntu 初始化：

```powershell
wsl --install -d Ubuntu
```

后续操作在 Ubuntu 终端中执行。将源码放在 WSL 用户目录下，例如 `~/parallel-computing-labs`。

各实验所需的软件包安装命令见对应 README。

## macOS 环境

安装 Xcode Command Line Tools：

```bash
xcode-select --install
```

通过 Homebrew 安装各实验所需工具，具体命令见对应 README。Lab1–3 使用 Homebrew LLVM 和 OpenMP；Lab4 使用 Open MPI；Lab5 使用 Verilator。

## 获取源码

```bash
git clone https://github.com/MAdrid1011/parallel-computing-labs.git
cd parallel-computing-labs
```

选择一个实验，进入对应目录。例如：

```bash
cd Lab1
```

在该文件夹内，按照它的 README 安装依赖、编译，并先执行“第一次运行：快速验证”。确认 `VERIFY PASS` 后，再扫描完整性能配置或修改参数。Lab1～Lab4 可用 `--build-dir` 选择构建目录；五个实验都支持 `--verify`。

修改实验实现后，可按 [回归检查说明](tests/README.md) 验证参数错误处理、参考验证和构建目录选择。

## 文件组织

```text
parallel-computing-labs/
  Lab1/  include/, src/, CMakeLists.txt, run.py, README.md
  Lab2/  include/, src/, CMakeLists.txt, run.py, README.md
  Lab3/  include/, src/, CMakeLists.txt, run.py, README.md
  Lab4/  include/, src/, CMakeLists.txt, run.py, README.md
  Lab5/  rtl/, include/, src/, build.py, run.py, README.md
  website/  docs/, mkdocs.yml, README.md
  README.md
```
