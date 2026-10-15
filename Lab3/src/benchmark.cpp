#include "benchmark.hpp"
#include "verification.hpp"
#include "support.hpp"

namespace histogram {
void benchmark(const Config& cfg) {
    const auto input = generate_input(cfg.n, cfg.bins, cfg.distribution, cfg.seed, cfg.hot_percent);
    Counts counts(cfg.bins, 0);
    Workspace workspace(cfg.bins, cfg.threads, cfg.stripes, cfg.kernel);
    omp_set_dynamic(0);
    omp_set_num_threads(cfg.threads);
    std::vector<double> times;
    std::vector<std::uint64_t> checksums;
    std::vector<Execution> samples;
    for (int trial = -1; trial < cfg.repeat; ++trial) {
        std::fill(counts.begin(), counts.end(), 0);
        std::fill(workspace.local_counts.begin(), workspace.local_counts.end(), 0);
        Execution execution;
        execution.stats.resize(cfg.threads);
        const double start = lab::now();
        if (cfg.kernel == "serial") serial(input, counts, execution);
        else if (cfg.kernel == "critical") critical(input, counts, execution);
        else if (cfg.kernel == "atomic") atomic(input, counts, execution);
        else if (cfg.kernel == "locks") locks(input, counts, workspace, execution);
        else private_counts(input, counts, workspace, execution);
        const double elapsed = lab::now() - start;
        const auto checksum = std::accumulate(counts.begin(), counts.end(), std::uint64_t{0});
        if (cfg.verify) verify_output(input, counts);
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
        total.updates += execution.stats[t].updates;
        total.merged_bins += execution.stats[t].merged_bins;
    }
    lab::csv_precision();
    std::cout << "record,kernel,distribution,n,bins,stripes,threads,seed,hot_percent,thread_id,trial,"
                 "updates,merged_bins,compute_s,wait_s,merge_s,elapsed_s,million_items_s,checksum\n";
    auto prefix = [&](const char* record, int tid, std::size_t trial) {
        std::cout << record << ',' << cfg.kernel << ',' << cfg.distribution << ',' << cfg.n << ','
                  << cfg.bins << ',' << cfg.stripes << ',' << execution.threads << ',' << cfg.seed << ','
                  << cfg.hot_percent << ',' << tid << ',' << trial << ',';
    };
    auto suffix = [&](std::size_t trial) {
        std::cout << ',' << times[trial] << ',' << (cfg.n / times[trial] / 1e6) << ',' << checksums[trial] << '\n';
    };
    prefix("summary", -1, middle);
    std::cout << total.updates << ',' << total.merged_bins << ",0,0,0";
    suffix(middle);
    for (int t = 0; t < execution.threads; ++t) {
        const auto& stat = execution.stats[t];
        prefix("thread", t, middle);
        std::cout << stat.updates << ',' << stat.merged_bins << ',' << stat.compute_s << ','
                  << stat.wait_s << ',' << stat.merge_s;
        suffix(middle);
    }
    for (std::size_t trial = 0; trial < times.size(); ++trial) {
        prefix("trial", -1, trial);
        std::cout << cfg.n << ",0,0,0,0";
        suffix(trial);
    }
}
}
