#include "benchmark.hpp"
#include "verification.hpp"
#include "support.hpp"
#include <omp.h>

namespace spmm {
void benchmark(const Config& cfg) {
    const auto a = generate_sparse(cfg.m, cfg.k, cfg.nnz_per_row, cfg.profile, cfg.seed);
    const auto b = generate_dense(cfg.k, cfg.n, cfg.seed + 1);
    const double partition_begin = lab::now();
    const auto blocks = partition_rows(a, cfg.block_nnz);
    const double partition_s = lab::now() - partition_begin;
    DenseMatrix c(cfg.m, cfg.n);
    omp_set_dynamic(0); omp_set_num_threads(cfg.threads);
    omp_sched_t kind = omp_sched_static;
    if (cfg.schedule == "dynamic") kind = omp_sched_dynamic;
    else if (cfg.schedule == "guided") kind = omp_sched_guided;
    omp_set_schedule(kind, cfg.chunk);
    std::vector<double> times, checksums;
    std::vector<Execution> samples;
    for (int trial = -1; trial < cfg.repeat; ++trial) {
        std::fill(c.values.begin(), c.values.end(), 0.0);
        Execution execution;
        execution.stats.resize(cfg.threads);
        const double start = lab::now();
        if (cfg.kernel == "serial") serial(a, b, c, execution);
        else if (cfg.kernel == "rows") parallel_rows(a, b, c, execution);
        else if (cfg.kernel == "balanced") parallel_balanced(a, b, c, blocks, execution);
        else parallel_tiled(a, b, c, blocks, cfg.tile, execution);
        const double elapsed = lab::now() - start;
        const double checksum = std::accumulate(c.values.begin(), c.values.end(), 0.0);
        if (cfg.verify) verify_output(a, b, c);
        if (trial >= 0) {
            times.push_back(elapsed); checksums.push_back(checksum);
            samples.push_back(std::move(execution));
        }
    }
    const auto middle = lab::median_index(times);
    const auto& execution = samples[middle];
    lab::csv_precision();
    std::cout << "record,profile,kernel,schedule,chunk,threads,m,k,n,nnz,block_nnz,row_blocks,tile,thread_id,trial,"
                 "row_visits,nnz_visits,updates,compute_s,wait_s,elapsed_s,gflops,partition_s,checksum\n";
    auto prefix = [&](const char* record, int tid, std::size_t sample) {
        std::cout << record << ',' << cfg.profile << ',' << cfg.kernel << ',' << cfg.schedule << ',' << cfg.chunk << ','
                  << execution.threads << ',' << cfg.m << ',' << cfg.k << ',' << cfg.n << ',' << a.values.size() << ','
                  << cfg.block_nnz << ',' << blocks.size() << ',' << cfg.tile << ',' << tid << ',' << sample << ',';
    };
    auto suffix = [&](std::size_t sample) {
        std::cout << ',' << times[sample] << ',' << (2.0 * a.values.size() * cfg.n / times[sample] / 1e9)
                  << ',' << partition_s << ',' << checksums[sample] << '\n';
    };
    ThreadStats total;
    for (int t = 0; t < execution.threads; ++t) {
        total.row_visits += execution.stats[t].row_visits;
        total.nnz_visits += execution.stats[t].nnz_visits;
        total.updates += execution.stats[t].updates;
    }
    prefix("summary", -1, middle);
    std::cout << total.row_visits << ',' << total.nnz_visits << ',' << total.updates << ",0,0";
    suffix(middle);
    for (int t = 0; t < execution.threads; ++t) {
        const auto& stat = execution.stats[t];
        prefix("thread", t, middle);
        std::cout << stat.row_visits << ',' << stat.nnz_visits << ',' << stat.updates << ',' << stat.compute_s << ',' << stat.wait_s;
        suffix(middle);
    }
    for (std::size_t sample = 0; sample < times.size(); ++sample) {
        prefix("trial", -1, sample);
        std::cout << "0,0," << static_cast<std::uint64_t>(a.values.size()) * cfg.n << ",0,0";
        suffix(sample);
    }
}
}
