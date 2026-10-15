#include "simulator.hpp"
#include "verification.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>

namespace gemm {
void benchmark(const Config& cfg) {
    std::vector<Result> samples;
    for (int trial = 0; trial < cfg.repeat; ++trial)
        samples.push_back(simulate(cfg, cfg.seed+std::uint32_t(trial)*7919u, trial==0 && !cfg.trace.empty()));
    std::vector<std::size_t> order(samples.size()); std::iota(order.begin(),order.end(),0);
    std::sort(order.begin(),order.end(),[&](auto a,auto b){ return samples[a].cycles<samples[b].cycles; });
    const auto middle = order[order.size()/2];
    if (cfg.verify)
        for (const auto& sample : samples) verify_output(cfg, sample.output);
    if (!cfg.dump_output.empty()) {
        std::ofstream stream(cfg.dump_output,std::ios::binary);
        const auto& output = samples[middle].output;
        stream.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()*sizeof(std::int32_t)));
    }
    std::cout << std::setprecision(12)
              << "record,m,k,n,array,tile_m,tile_k,tile_n,buffering,latency,beat_cycles,stall_percent,seed,trial,"
                 "cycles,compute_cycles,input_wait_cycles,output_cycles,overlap_cycles,macs,read_requests,write_requests,"
                 "read_bytes,write_bytes,overlap_read_beats,macs_per_cycle,pe_utilization,compute_utilization,spm_bytes,host_seconds,checksum\n";
    auto write = [&](const char* record,std::size_t trial) {
        const auto& r = samples[trial];
        const double rate = double(r.macs)/r.cycles;
        constexpr std::uint64_t spm_bytes = 8ull*LAB_SPM_TILE*LAB_SPM_K+4ull*LAB_SPM_TILE*LAB_SPM_TILE;
        std::cout << record << ',' << cfg.m << ',' << cfg.k << ',' << cfg.n << ',' << LAB_ARRAY << ','
                  << cfg.tile_m << ',' << cfg.tile_k << ',' << cfg.tile_n << ',' << (cfg.double_buffer?"double":"single") << ','
                  << cfg.latency << ',' << cfg.beat_cycles << ',' << cfg.stall_percent << ',' << cfg.seed << ',' << trial << ','
                  << r.cycles << ',' << r.compute_cycles << ',' << r.input_wait_cycles << ',' << r.output_cycles << ','
                  << r.overlap_cycles << ',' << r.macs << ',' << r.dram.read_requests << ',' << r.dram.write_requests << ','
                  << r.dram.read_bytes << ',' << r.dram.write_bytes << ',' << r.dram.overlap_read_beats << ','
                  << rate << ',' << rate/(LAB_ARRAY*LAB_ARRAY) << ',' << double(r.macs)/(r.compute_cycles*LAB_ARRAY*LAB_ARRAY) << ','
                  << spm_bytes << ',' << r.host_seconds << ',' << r.checksum << '\n';
    };
    write("summary",middle);
    for (std::size_t trial = 0; trial < samples.size(); ++trial) write("trial",trial);
}
}
