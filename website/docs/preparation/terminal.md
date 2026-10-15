# 终端与项目目录

终端会在一个“当前目录”中执行命令。很多初次运行的问题，都与当前位置或正在使用的环境有关。先认识下面几个操作，后面的命令就容易理解了。

## 确认自己在哪里

```bash
pwd
ls
```

`pwd` 显示当前目录的完整路径；`ls` 列出目录中的文件。进入 Lab1 后，应该能看到 `src/`、`include/`、`CMakeLists.txt` 和 `run.py`。

```bash
cd Lab1
```

`cd` 切换目录。`cd ..` 返回上一层。如果路径中有空格，使用引号包住整个路径。

!!! note "文档中的 cd Lab1 从哪里开始？"
    从包含 `Lab1/` 的目录开始。通过 Git 获取代码时，这通常是仓库根目录；单独下载 Lab1 时，这是解压后包含 `Lab1/` 的目录。已经进入 Lab1 后，无需再次执行 `cd Lab1`。

## 认识源码与生成文件

```text
Lab1/
├── include/         头文件：类型和函数接口
├── src/             C++ 实现
├── CMakeLists.txt   构建配置
├── run.py           依次测量不同实现
├── build/           编译生成的文件
└── results/         本次运行的数据
```

Lab2～Lab4 的组织方式相近。Lab5 另外有 `rtl/`，保存 SystemVerilog 电路描述，并用自己的 `build.py` 构建仿真模型。

头文件声明“可以使用什么”，实现文件定义“具体怎么做”。阅读计算过程时，可以从教程指出的 `src/` 文件入手，再回到 `include/` 查看参数和数据结构。

## 构建与运行是两个步骤

对 Lab1～Lab4：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
python3 run.py --help
```

第一行让 CMake 读取当前目录 `.` 中的构建配置，把生成文件放到 `build/`。第二行编译源码。第三行查看当前实验的参数；随后按操作页的快速验证命令运行，确认环境与结果。

`--parallel 4` 指编译时最多并行执行四项构建工作。**它不决定实验程序的线程数或 MPI 进程数**；运行程序的并行度由 `--threads`、`--ranks` 或 `mpiexec -n` 设置。

对 Lab5：

```bash
python3 build.py --array 8
python3 run.py --m 32 --array 8 --tile 16 --verify --output results/quick
```

修改 C++ 或 RTL 后，重新构建，再运行程序。

## 复制多行命令

文档中行末的 `\` 表示命令在下一行继续。它适用于这里使用的 Bash 或 zsh。复制时应包含整个代码块；不要在 `\` 后额外添加空格。

出现以 `--` 开始的内容时，它们通常是参数。例如：

```bash
python3 run.py --threads 4 --output results/four_threads
```

这条命令指定实验并行度，并把输出写入一个单独目录。每个实验支持的参数以它自己的操作页为准。

## 保存数据

`run.py` 将数据保存在当前实验的 `results/`。如果希望保留多次运行，使用不同的 `--output` 目录名，例如 `results/tile32` 和 `results/tile64`。

可执行文件直接输出 CSV 时，可以使用重定向 `>` 保存。它会覆盖同名文件，因此不同配置使用不同文件名：

```bash
mkdir -p results/single
```

然后按照当前实验“单独运行一个实现”的命令，把输出保存到这个目录。

## 接下来

阅读[怎样测量程序](measurement.md)，了解怎样让两次运行可以比较。
