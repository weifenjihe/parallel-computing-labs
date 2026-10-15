# 配置开发环境

下面先准备操作系统提供的基本工具。各实验需要的 OpenMP、MPI 或 Verilator 依赖，继续按该实验的“操作步骤”安装。

## Windows：WSL2 与 Ubuntu

### 1. 安装 WSL2

在 Windows 中，以管理员身份打开 PowerShell，执行：

```powershell
wsl --install -d Ubuntu
```

按系统提示重启，然后打开 Ubuntu，设置 Linux 用户名和密码。输入密码时终端通常不会显示字符，输入完成后按回车即可。

??? info "WSL2 是什么？"
    WSL2 让我们在 Windows 上使用 Linux 环境。在 Ubuntu 终端中，`apt`、`g++` 和后面的实验命令由 Linux 执行；Windows PowerShell 与 Ubuntu 终端是两个不同的环境。

    如果安装过程遇到系统版本或虚拟化设置的问题，可以查阅 [Microsoft 的 WSL 安装文档](https://learn.microsoft.com/zh-cn/windows/wsl/install)。

### 2. 安装基本工具

在 **Ubuntu 终端**中执行：

```bash
sudo apt update
sudo apt install -y git g++ make cmake python3 python3-venv
```

`sudo` 可能要求输入刚才设置的 Linux 密码。

### 3. 选择工作目录

```bash
mkdir -p ~/parallel-work
cd ~/parallel-work
```

可以把下载的 Lab 文件夹放在这里，或在这里克隆仓库。如果使用 VS Code，可安装 WSL 扩展，再从 Ubuntu 终端的项目目录执行 `code .`，让编辑器连接到同一个 WSL 环境。

## macOS：命令行工具与 Homebrew

### 1. 安装命令行工具

打开“终端”，执行：

```bash
xcode-select --install
```

如果提示已经安装，可以继续下一步。

### 2. 安装 Homebrew

如果尚未安装 Homebrew，请按 [Homebrew 官网](https://brew.sh/zh-cn/)的步骤安装，并完成安装器提示的终端配置。之后执行：

```bash
brew --version
```

出现版本信息，表示当前终端已经可以使用 Homebrew。

### 3. 为当前实验安装依赖

Lab1～Lab3 的操作页会安装 LLVM 和 libomp，并要求在构建前执行：

```bash
export CXX="$(brew --prefix llvm)/bin/clang++"
export OpenMP_ROOT="$(brew --prefix libomp)"
```

第一行选择 C++ 编译器，第二行告诉构建工具在哪里寻找 OpenMP 运行库。这两行作用于当前终端；重新打开终端后，在构建前再次执行。

Lab4 按其文档安装 Open MPI，Lab5 按其文档安装 Verilator。

!!! tip "切换编译器后如何重新构建？"
    CMake 会记住第一次配置时选用的编译器。如果原来的 `build/` 使用了另一个编译器，可以保留它，改用一个新的构建目录，例如 `build-llvm/`。此时后续运行可执行文件也要使用新目录中的路径。

    测量脚本也需要选择这个目录，否则无法确认是否运行了新程序。以 Lab1 为例：

    ```bash
    cmake -S . -B build-llvm -DCMAKE_BUILD_TYPE=Release
    cmake --build build-llvm --parallel 4
    python3 run.py --build-dir build-llvm --m 256 --k 512 --n 64 \
      --nnz-per-row 8 --repeat 1 --rounds 1 --verify --output results/quick_llvm
    ```

    `--build-dir` 适用于 Lab1～Lab4，`environment.json` 会记录实际目录与编译器。

    也可以仅删除当前实验生成的 `build/` 后重新配置；源码位于 `src/` 和 `include/`，Lab5 的 RTL 位于 `rtl/`。

## Python 虚拟环境

各实验都使用自己的 `.venv/`，用于安装 `requirements.txt` 中的 Python 依赖。在当前 Lab 目录执行：

```bash
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -r requirements.txt
```

激活后，当前终端使用这个实验的 Python 环境。切换实验时，先执行 `deactivate`，再进入新 Lab 并激活它的 `.venv/`。再次打开终端时，不必重复创建环境，进入 Lab 后执行 `source .venv/bin/activate` 即可。

## 接下来

阅读[终端与项目目录](terminal.md)，然后前往 [Lab1 操作步骤](../lab1/run.md)。
