# 常见问题

这里按“环境、构建、运行、读数据”整理常见问题。定位问题时，先确认当前实验目录和终端环境。

## 环境与目录

??? question "Windows 上这些命令在哪个终端执行？"
    安装 WSL2 的命令在管理员 PowerShell 中执行。安装 `apt` 依赖、编译和运行实验的命令都在 WSL2 的 Ubuntu 终端中执行，详见[配置开发环境](preparation/environment.md)。

??? question "提示找不到 run.py 或 CMakeLists.txt？"
    执行 `pwd` 和 `ls` 看当前位置。进入当前 Lab，确认能看到它的 `run.py` 和源码。Lab1～Lab4 还应有 `CMakeLists.txt`；Lab5 使用自己的 `build.py`。

??? question "切换实验后，Python 依赖为什么又找不到？"
    各实验使用独立的 `.venv/`。先退出旧环境，进入新 Lab，再执行 `source .venv/bin/activate`。如果还没有创建该环境，按当前实验操作页安装。

## 构建

??? question "macOS 上提示找不到 OpenMP？"
    在 Lab1～Lab3，按操作页安装 `llvm` 和 `libomp`，在当前终端设置 `CXX` 与 `OpenMP_ROOT`，再配置 CMake。已有 `build/` 可能缓存了旧编译器，需重新配置该实验的构建目录，详见[环境准备](preparation/environment.md)。

??? question "修改源码后，为什么行为没有变化？"
    `run.py` 运行已有的可执行文件。Lab1～Lab4 修改 C++ 后重新执行 `cmake --build build --parallel 4`；Lab5 修改 C++ 或 RTL 后重新执行 `python3 build.py`。

??? question "编译命令里的 parallel 4 就是实验四线程吗？"
    它是构建工具的并发数。程序的线程数使用 `--threads` 设置，MPI 进程数使用 `--ranks` 或 `mpiexec -n` 设置。Lab5 的阵列边长使用 `--array` 设置。

## 运行

??? question "MPI 是不是一定要准备多台电脑？"
    不需要。默认在当前电脑启动多个进程，可以完成本实验的通信方式比较。多主机时，还需要配置兼容的 MPI、主机连接和可执行文件路径，见 [Lab4 操作步骤](lab4/run.md)。

??? question "Open MPI 提示没有足够的 slots？"
    使用 `run.py` 时，脚本会针对 Open MPI 添加 `--oversubscribe`。直接运行时可以在 `mpiexec` 后添加该选项。允许启动更多进程并不等于增加了物理 CPU 资源，测量时仍要记录实际进程数和电脑配置。

??? question "Lab5 提示找不到某个仿真模型？"
    先在 Lab5 用 `build.py` 构建运行所需的 `--array` 模型。运行时的 `--spm-tile`、`--spm-k` 应与构建参数一致。`run.py` 使用已有模型，不会自动构建。

??? question "Lab5 的分块参数怎样设置？"
    `--tile` 同时设置 M、K、N 三个分块方向，也可以分别使用 `--tile-m`、`--tile-k`、`--tile-n` 覆盖。M、N 分块不能超过构建容量 `--spm-tile`，K 分块不能超过 `--spm-k`。详见[分块教程](lab5/tiling.md)。

## 数据与观察

??? question "为什么同样的程序每次时间略有不同？"
    CPU 调度、缓存、电源状态和后台工作都会影响时间。先看 `raw.csv` 的重复记录，并保持一组测量的环境一致。详见[怎样测量程序](preparation/measurement.md)。

??? question "线程数或进程数增加，为什么可能没有变快？"
    增加参与者也增加了调度、同步或通信工作。数据访问还可能受带宽影响，任务也可能分配不均。先固定输入和实现，查看工作量与等待、通信等字段，再判断当前限制在哪里。

??? question "怎样避免下一次运行覆盖上一次数据？"
    每组参数使用独立的 `--output` 目录名，保留完整命令和 `environment.json`。直接重定向 CSV 时也使用不同文件名。

??? question "checksum 相同是不是就说明完全正确？"
    它只表示输出元素总和相同，不足以证明每个元素正确。阅读实现时要确认输出归属、循环范围和同步方式。完整核对输出时，使用各实验的 `--verify` 逐元素或逐桶比较；Lab5 还提供 `--save-output` 保存 C。

## 参数与验证

??? question "换成 build-llvm 后为什么没有运行新程序？"
    Lab1～Lab4 的构建和测量必须选择同一个目录。执行 `python3 run.py --build-dir build-llvm ...`，把省略号替换为本实验快速验证页的参数。检查 `environment.json` 中的 `build_directory` 和 `compiler`。

??? question "参数拼错、漏了值，或者分块设成 0？"
    程序会明确报错并退出。先执行 `--help`，按操作页检查参数范围。稀疏乘法的每行非零数不能超过 K；Lab5 的分块不能超过已构建的 SPM 容量。不要使用被拒绝配置的旧 CSV 作分析。

??? question "请求四线程，程序却只得到两线程？"
    查看错误提示与 `environment.json` 中的 OpenMP 环境设置。已有 `OMP_THREAD_LIMIT=2` 会限制实际线程数。可以改为 `--threads 2`，或在确认不需要该限制后，在当前终端执行 `unset OMP_THREAD_LIMIT`。重新运行使用新的输出目录。

??? question "如何确认修改后的实现仍正确？"
    重新构建，执行本实验的快速验证命令并保留 `--verify`。`VERIFY PASS` 表示当前输出通过逐元素或逐桶比较；`VERIFY FAIL` 会显示错误位置与期望值。仅比较 checksum 不够。小输入验证通过后，再做性能测量。

??? question "扫描很久或中途按了 Ctrl+C，还能看哪些结果？"
    先看进度 `i/total`，完整默认扫描有大量独立配置。`raw.csv` 保存已完成配置，最终汇总与图表在全部完成后生成。中断不是完整实验结果，重新运行不自动续跑，请选择新的输出目录。

??? question "Lab5 报 exceeded --max-cycles？"
    模型触及单次仿真的周期上限。先检查维度、分块、供数和握手；`stall-percent` 必须小于 100。确认任务规模确实更大后，再提高 `--max-cycles`，例如 `--max-cycles 200000000`。
