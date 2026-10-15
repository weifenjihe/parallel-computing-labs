# 实验回归检查

先按各 Lab 的 README 构建 Lab1～Lab4，并运行 `python3 build.py --array 8` 构建 Lab5 的模型。各 Lab 的 Python 环境需已安装自己的 `requirements.txt`。

在仓库根目录执行：

```bash
python3 -m unittest discover -s tests -v
```

检查覆盖命令帮助、缺参、未知实现、非法数值、CSR 列号唯一性约束、SPM 容量、仿真周期上限、实际线程限制、独立构建目录，以及五个实验的参考验证。MPI 检查包含进程持有空数据的情况。

参考验证的反例会修改一个输出元素，或者保持直方图总计数不变但交换桶计数，确认 `VERIFY FAIL` 确实检测到错误。测试在临时目录中构建小程序，不修改实验源码。
