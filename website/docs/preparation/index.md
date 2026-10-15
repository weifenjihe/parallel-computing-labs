# 开始之前

在运行实验之前，我们先把环境和文件准备好。如果你已经使用过终端和 CMake，可以按对应实验的“操作步骤”配置；如果这是第一次接触，请顺着本节往下阅读。

## 我们需要准备什么？

实验由 C++、Python 和少量 SystemVerilog 文件组成，它们各有自己的用途。

| 工具 | 在实验中做什么？ | 使用范围 |
|---|---|---|
| C++ 编译器 | 将 `src/` 中的代码编译成可执行程序。 | Lab1～Lab5 |
| CMake | 组织 C++ 项目的构建。 | Lab1～Lab4 |
| Python | 依次运行配置，整理程序输出的 CSV 数据。 | Lab1～Lab5 |
| OpenMP | 启动线程，分配循环迭代，协调共享数据。 | Lab1～Lab3 |
| MPI | 启动多个进程，进行数据交换。 | Lab4 |
| Verilator | 将 RTL 构建成硬件仿真模型。 | Lab5 |

不用一次把所有工具都安装好。先准备当前实验所需的依赖，再进入对应实验。

## 选择运行环境

=== "Windows"
    使用 **WSL2 + Ubuntu**。安装 WSL2 在 Windows 的 PowerShell 中完成；后续安装依赖、编译和运行都在 Ubuntu 终端中完成。

    将实验放在 Ubuntu 的用户目录中，例如 `~/parallel-computing-labs`。这样可以让编辑、编译和运行都使用同一个 Linux 环境。

=== "macOS"
    使用 macOS 的终端。先安装 Xcode Command Line Tools 和 Homebrew，再按实验页安装依赖。

    Lab1～Lab3 需要支持 OpenMP 的编译器。文档使用 Homebrew 的 LLVM 与 libomp，并说明如何选择它们。

进入[配置开发环境](environment.md)查看具体步骤。

## 获取实验代码

每个实验的“实验介绍”页都有独立源码下载。解压后会得到 `Lab1/`、`Lab2/` 等目录，直接进入目标目录即可按文档操作。

如果使用 Git 获取完整仓库，可以在终端执行：

```bash
git clone https://github.com/MAdrid1011/parallel-computing-labs.git
cd parallel-computing-labs
```

之后进入当前实验。例如 Lab1：

```bash
cd Lab1
```

!!! note "每个实验单独构建"
    在 `Lab1/` 里构建的程序只属于 Lab1。切换到 Lab2 后，需要在 `Lab2/` 里构建自己的程序。各目录的 `build/`、`results/` 和 Python 环境相互独立。

## 建议的操作顺序

1. 打开目标实验的“实验介绍”，先确定输入、输出和计算任务。
2. 阅读教程中的基本实现，找到对应源码。
3. 进入目标 Lab，按“操作步骤”安装依赖并构建。
4. 先执行操作页的“第一次运行：快速验证”，确认 `VERIFY PASS` 和输出文件，再运行完整测量。
5. 按教程改变一个条件，保存参数、环境与数据，用对比表说明自己观察到的变化。

如果不熟悉 `cd`、`build/` 或虚拟环境，接着阅读[终端与项目目录](terminal.md)。
