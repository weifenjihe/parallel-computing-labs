#include "benchmark.hpp"
#include "validation.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        histogram::Config cfg;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") {
                std::cout << "Integer histogram: counts[input[i]] += 1\n--n 10000000 --bins 256 --stripes 16 --seed 2026\n--kernel serial|critical|atomic|locks|private\n--distribution uniform|hotspot --hot-percent 90\n--threads 4 --repeat 7 --verify\n";
                return 0;
            }
            if (key == "--verify") { cfg.verify = true; continue; }
            if (key == "--n") {
                cfg.n = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--bins") {
                cfg.bins = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--stripes") {
                cfg.stripes = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--threads") {
                cfg.threads = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--repeat") {
                cfg.repeat = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--hot-percent") {
                cfg.hot_percent = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--seed") {
                cfg.seed = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--distribution") {
                cfg.distribution = lab::argument(argc, argv, i, key);
            }
            else if (key == "--kernel") {
                cfg.kernel = lab::argument(argc, argv, i, key);
            }
            else throw std::invalid_argument("Unknown option: " + key + ". Use --help for usage.");
        }
        lab::positive(cfg.n, "--n");
        lab::positive(cfg.bins, "--bins");
        lab::positive(cfg.stripes, "--stripes");
        lab::positive(cfg.threads, "--threads");
        lab::positive(cfg.repeat, "--repeat");
        lab::require(cfg.hot_percent <= 100, "--hot-percent must be between 0 and 100.");
        lab::require(cfg.bins <= std::numeric_limits<std::uint32_t>::max(), "--bins exceeds the input-value range.");
        lab::matrix_size(cfg.n, 1, sizeof(std::uint32_t), "Input");
        lab::choice(cfg.kernel, "--kernel", {"serial", "critical", "atomic", "locks", "private"});
        lab::choice(cfg.distribution, "--distribution", {"uniform", "hotspot"});
        histogram::benchmark(cfg);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 2;
    }
}
