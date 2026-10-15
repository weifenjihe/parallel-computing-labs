# 共享计数与同步

## 1. 加一并不只是“写一个数”

普通的 `counts[v] += 1` 可以理解为读出旧值、加一、写回。如果两个线程都读到了旧值 5，随后各自写回 6，两次输入却只增加了一次计数。

这是一种共享访问冲突。并行实现必须让更新正确发生，再讨论性能。本实验提供的实现使用不同的同步或数据分工方式。

## 2. 全局临界区

简化写法如下：

```cpp
#pragma omp critical
{
    counts[v] += 1;
}
```

同一个临界区同一时刻只能由一个线程执行。不论线程更新哪个桶，都需要获得进入机会。它便于表达保护范围，但这里每个输入元素都要进入一次。

在 `src/histogram_critical.cpp` 中，找到临界区内外分别包含哪些工作。

## 3. 原子加

```cpp
#pragma omp atomic update
counts[v] += 1;
```

原子操作保证这一次共享更新的原子性。它把保护范围缩到一次加法，但多个线程更新同一地址时，仍要协调访问；访问不同地址也可能受到缓存行和一致性机制影响。

在 `src/histogram_atomic.cpp` 中确认被保护的操作，再比较它与临界区的代码结构。

## 4. 分段锁

分段锁让不同的桶可以使用不同的锁。本实验按下面的规则选择：

```text
lock_index = bucket % stripes
```

`stripes=1` 时全部桶共享一把锁；增加锁数后，一些不同桶的更新可以使用不同锁。仍然映射到同一把锁的更新，需要等待彼此完成。

!!! tip "先看输入会落到哪里"
    在 hotspot 输入中，大部分值直接映射到桶 0。无论一共准备多少把锁，桶 0 都使用同一把锁。因此“增加锁数”和“分散热点更新”是两个不同的改变。

阅读 `src/workspace.cpp` 查看锁的创建与销毁，再在 `src/histogram_locks.cpp` 中找到选锁、获取锁、更新和释放锁。

## 5. 每个线程先自己计数

Private 实现为每个线程准备一份计数：

```text
线程 0 → private_counts[0][...]
线程 1 → private_counts[1][...]
线程 2 → private_counts[2][...]
……

最终 counts[b] = 各线程 private_counts[thread][b] 之和
```

输入阶段中，一个线程只写自己的数组，所以逐元素计数不需要抢同一个共享桶。结束计数后先同步，再按桶分配合并工作：每个最终桶由一个合并任务负责。

这种方法多用了计数空间，也增加了合并工作。随着线程数和桶数变化，两者的代价也会变化。实际工作空间还带有填充，阅读 `include/histogram.hpp` 和 `src/workspace.cpp` 时可以找到布局。

在 `src/histogram_private.cpp` 中依次找到：线程编号、私有计数入口、输入循环、计数后的屏障，以及合并循环。

## 6. 动手试一试

按[操作步骤](run.md)构建并通过快速验证后，运行完整测量，固定输入规模、桶数和线程数，比较不同更新方式。

### 比较不同锁数

```bash
python3 run.py --n 10000000 --stripes 1 4 16 64 256 \
  --output results/lock_counts
```

分别阅读 uniform 与 hotspot 的记录。先根据输入分布画出“值 → 桶 → 锁”的对应关系，再看增加锁数是否改变了主要竞争位置。

### 改变热点比例

```bash
for hot in 0 50 90 99; do
  python3 run.py --n 10000000 --distribution hotspot --hot-percent "$hot" \
    --output "results/hot_$hot"
done
```

保持其他参数相同，观察 Atomic、Locks 和 Private 随热点比例的变化。Private 也要把数据读入和完成合并，因此逐元素共享冲突减少后，仍有其他工作需要完成。

### 看清楚统计字段

从 `summary.csv` 先看汇总时间，再从 `raw.csv` 的线程记录看 `compute_s`、`merge_s` 和 `wait_s`。

!!! note "wait_s 不是锁等待计时器"
    这里的 `wait_s` 是显式屏障处的等待。锁或原子更新的代价发生在计数循环内部，计入 `compute_s`。Private 的 `merge_s` 已包含在 `compute_s` 中，不能再重复相加。

可以用自己的记录回答：某个改变减少了哪些共享更新？增加了多少额外工作？在 uniform 和 hotspot 中，为什么同一种改动可能表现不同？

[前往操作步骤](run.md){ .md-button .md-button--primary }
