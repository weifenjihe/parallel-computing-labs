#include "benchmark.hpp"
#include "verification.hpp"
#include "support.hpp"
#include <omp.h>

namespace transpose {
void benchmark(const Config& cfg) {
    const auto input = generate_matrix(cfg.n, cfg.seed);
    Matrix output(cfg.n);
    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);
    std::vector<double> times;
    std::vector<std::uint64_t> checksums;
    std::vector<Execution> samples;
    for (int trial = -1; trial < cfg.repeat; ++trial) {
        std::fill(output.values.begin(), output.values.end(), 0);
        Execution execution;
        execution.stats.resize(cfg.threads);
        const double start = lab::now();
        if (cfg.kernel == "serial") serial(input, output, execution);
        else if (cfg.kernel == "naive") parallel_naive(input, output, execution);
        else parallel_tiled(input, output, cfg.tile, execution);
        const double elapsed = lab::now() - start;
        const auto checksum = std::accumulate(output.values.begin(), output.values.end(), std::uint64_t{0});
        if (cfg.verify) verify_output(input, output);
        if (trial >= 0) {
            times.push_back(elapsed);
            checksums.push_back(checksum);
            samples.push_back(std::move(execution));
        }
    }
    const auto middle = lab::median_index(times);
    const auto& execution = samples[middle];
    ThreadStats total;
    for (int t = 0; t < execution.threads; ++t) {
        total.row_visits += execution.stats[t].row_visits;
        total.tile_visits += execution.stats[t].tile_visits;
        total.updates += execution.stats[t].updates;
    }
    lab::csv_precision();
    std::cout << "record,kernel,n,tile,threads,seed,thread_id,trial,row_visits,tile_visits,updates,"
                 "compute_s,wait_s,elapsed_s,effective_gbps,checksum\n";
    auto prefix = [&](const char* record, int tid, std::size_t trial) {
        std::cout << record << ',' << cfg.kernel << ',' << cfg.n << ',' << cfg.tile << ','
                  << execution.threads << ',' << cfg.seed << ',' << tid << ',' << trial << ',';
    };
    auto suffix = [&](std::size_t trial) {
        std::cout << ',' << times[trial] << ',' << (2.0 * cfg.n * cfg.n * sizeof(std::uint32_t) / times[trial] / 1e9)
                  << ',' << checksums[trial] << '\n';
    };
    prefix("summary", -1, middle);
    std::cout << total.row_visits << ',' << total.tile_visits << ',' << total.updates << ",0,0";
    suffix(middle);
    for (int t = 0; t < execution.threads; ++t) {
        const auto& stat = execution.stats[t];
        prefix("thread", t, middle);
        std::cout << stat.row_visits << ',' << stat.tile_visits << ',' << stat.updates << ','
                  << stat.compute_s << ',' << stat.wait_s;
        suffix(middle);
    }
    for (std::size_t trial = 0; trial < times.size(); ++trial) {
        prefix("trial", -1, trial);
        std::cout << "0,0," << cfg.n * cfg.n << ",0,0";
        suffix(trial);
    }
}
}
