#!/usr/bin/env python3
"""Plot Lab5 buffer/compute/input handshakes from a VCD without an external viewer."""
import argparse
from pathlib import Path
import sys

from cli_utils import positive_int, nonnegative_int, run_main

SIGNALS = {
    'compute_active': 'TOP.compute_active',
    'r_valid': 'TOP.r_valid',
    'r_ready': 'TOP.r_ready',
    'current_buffer': 'TOP.matrix_accelerator.current_buffer',
    'buffer_ready': 'TOP.matrix_accelerator.buffer_ready',
}


def read_cycles(path):
    """Sample at clk=0: the same pre-edge handshake phase as simulator.cpp."""
    scopes, identifiers, values, samples = [], {}, {}, []
    timestamp = None
    definitions = True
    with Path(path).open() as stream:
        for line in stream:
            fields = line.strip().split()
            if not fields:
                continue
            if definitions:
                if fields[0] == '$scope':
                    scopes.append(fields[2])
                elif fields[0] == '$upscope':
                    scopes.pop()
                elif fields[0] == '$var':
                    full_name = '.'.join([*scopes, fields[4]])
                    for name, wanted in SIGNALS.items():
                        if full_name == wanted:
                            identifiers[fields[3]] = name
                            values[name] = 0
                elif fields[0] == '$enddefinitions':
                    definitions = False
                    missing = set(SIGNALS) - set(identifiers.values())
                    if missing:
                        raise RuntimeError('VCD 缺少 Lab5 信号：' + ', '.join(sorted(missing)))
                continue
            if fields[0].startswith('#'):
                if timestamp is not None and timestamp % 2 == 0:
                    samples.append((timestamp // 2, values.copy()))
                timestamp = int(fields[0][1:])
            else:
                encoded, identifier = (fields[0][1:], fields[1]) if fields[0][0] in 'bB' else (fields[0][0], fields[0][1:])
                if identifier in identifiers and set(encoded) <= {'0', '1'}:
                    values[identifiers[identifier]] = int(encoded, 2)
    if timestamp is not None and timestamp % 2 == 0:
        samples.append((timestamp // 2, values.copy()))
    if not samples:
        raise RuntimeError('VCD 没有可绘制的时钟周期，请扩大 --trace-cycles。')
    return samples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=Path('results/waveform.png'))
    parser.add_argument('--start', type=nonnegative_int, help='窗口起点；默认从第一次计算之前开始')
    parser.add_argument('--cycles', type=positive_int, default=1200)
    args = parser.parse_args()
    samples = read_cycles(args.input)
    first_compute = next((cycle for cycle, values in samples if values['compute_active']), samples[0][0])
    start = args.start if args.start is not None else max(samples[0][0], first_compute - 40)
    selected = [(cycle, values) for cycle, values in samples if start <= cycle < start + args.cycles]
    if len(selected) < 2:
        raise RuntimeError('选择的波形窗口没有足够数据，请调整 --start/--cycles 或重新扩大 VCD 窗口。')
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    x = [cycle for cycle, _ in selected]
    waveforms = [
        ('compute_active', [v['compute_active'] for _, v in selected], '#167541'),
        ('r_valid & r_ready', [int(bool(v['r_valid'] and v['r_ready'])) for _, v in selected], '#2469ba'),
        ('current_buffer', [v['current_buffer'] for _, v in selected], '#7852a3'),
        ('buffer_ready[0]', [v['buffer_ready'] & 1 for _, v in selected], '#b76315'),
        ('buffer_ready[1]', [(v['buffer_ready'] >> 1) & 1 for _, v in selected], '#b76315'),
    ]
    figure, axes = plt.subplots(len(waveforms), 1, figsize=(12, 6.3), sharex=True, layout='constrained')
    for axis, (name, y, color) in zip(axes, waveforms):
        axis.step(x, y, where='post', color=color, linewidth=1.5)
        axis.fill_between(x, 0, y, step='post', color=color, alpha=0.13)
        axis.set_ylim(-0.15, 1.2)
        axis.set_yticks([0, 1])
        axis.set_ylabel(name, rotation=0, ha='right', va='center', fontsize=10)
        axis.grid(axis='x', color='#d7e0eb', linewidth=0.7)
    switches = [x[index] for index in range(1, len(selected)) if selected[index][1]['current_buffer'] != selected[index - 1][1]['current_buffer']]
    for cycle in switches:
        for axis in axes:
            axis.axvline(cycle, color='#706275', linestyle='--', linewidth=0.9)
        axes[0].text(cycle + 4, 1.07, 'buffer switch', fontsize=9, color='#654375')
    axes[-1].set_xlim(x[0], x[-1])
    axes[-1].set_xlabel('Simulation cycle (including reset)')
    figure.suptitle('Lab5: compute, input transfer and buffer selection', fontsize=13)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(args.output, dpi=160)
    plt.close(figure)
    print(f'Waveform saved: {args.output}; window {x[0]}..{x[-1]}, buffer switches={len(switches)}', file=sys.stderr)


if __name__ == '__main__':
    run_main(main)
