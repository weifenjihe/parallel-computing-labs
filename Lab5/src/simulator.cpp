#include "simulator.hpp"
#include "support.hpp"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <memory>
#include <numeric>
#include <stdexcept>

namespace gemm {
Result simulate(const Config& cfg, std::uint32_t timing_seed, bool save_trace) {
    Matrices matrices(cfg);
    SoftwareDram dram(matrices.memory, cfg, timing_seed);
    auto context = std::make_unique<VerilatedContext>();
    context->traceEverOn(save_trace);
    auto top = std::make_unique<Vmatrix_accelerator>(context.get());
    std::unique_ptr<VerilatedVcdC> trace;
    if (save_trace) { trace = std::make_unique<VerilatedVcdC>(); top->trace(trace.get(), 3); trace->open(cfg.trace.c_str()); }
    std::uint64_t tick_count = 0;
    auto step = [&]() {
        top->clk = 0; top->eval();
        dram.drive(*top, tick_count); top->eval();
        const auto bus = dram.sample(*top);
        const bool tracing = trace && tick_count>=cfg.trace_start && tick_count-cfg.trace_start<cfg.trace_cycles;
        if (tracing) trace->dump(context->time());
        context->timeInc(1);
        top->clk = 1; top->eval();
        if (tracing) trace->dump(context->time());
        context->timeInc(1);
        dram.update(bus, tick_count); ++tick_count;
    };
    top->rst_n = 0; top->start = 0;
    top->cfg_m = cfg.m; top->cfg_k = cfg.k; top->cfg_n = cfg.n;
    top->cfg_tile_m = cfg.tile_m; top->cfg_tile_k = cfg.tile_k; top->cfg_tile_n = cfg.tile_n;
    top->cfg_a_base = matrices.a_base; top->cfg_b_base = matrices.b_base; top->cfg_c_base = matrices.c_base;
    top->cfg_double_buffer = cfg.double_buffer;
    step(); step(); top->rst_n = 1;
    const auto begin = lab::now();
    top->start = 1; step(); top->start = 0;
    std::uint64_t working_cycles = 1;
    while (!top->done) {
        if (working_cycles >= cfg.max_cycles)
            throw std::runtime_error("Simulation exceeded --max-cycles=" + std::to_string(cfg.max_cycles)
                                     + ". Inspect the configuration/handshakes or increase the limit.");
        step(); ++working_cycles;
    }
    Result result{top->cycles, top->compute_cycles, top->input_wait_cycles, top->output_cycles,
                  top->overlap_cycles, top->mac_count, dram.stats};
    result.host_seconds = lab::now()-begin;
    result.output = matrices.output(cfg);
    result.checksum = std::accumulate(result.output.begin(), result.output.end(), std::int64_t{0});
    top->final(); if (trace) trace->close();
    return result;
}
}
