#include "benchmark.hpp"
#include "validation.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        spmm::Config cfg;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") {
                std::cout << "CSR SpMM: C[M,N] = A[M,K] * B[K,N]\n--m 4096 --k 8192 --n 1024 --nnz-per-row 32 --seed 2026\n--profile uniform|skewed|blocked --kernel serial|rows|balanced|tiled\n--threads 4 --repeat 7 --schedule static|dynamic|guided --chunk 0\n--block-nnz 256 --tile 128 --verify\n";
                return 0;
            }
            if (key == "--verify") { cfg.verify = true; continue; }
            if (key == "--m") {
                cfg.m = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--k") {
                cfg.k = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--n") {
                cfg.n = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--nnz-per-row") {
                cfg.nnz_per_row = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--block-nnz") {
                cfg.block_nnz = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--tile") {
                cfg.tile = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--seed") {
                cfg.seed = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--threads") {
                cfg.threads = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--repeat") {
                cfg.repeat = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--chunk") {
                cfg.chunk = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--profile") {
                cfg.profile = lab::argument(argc, argv, i, key);
            }
            else if (key == "--kernel") {
                cfg.kernel = lab::argument(argc, argv, i, key);
            }
            else if (key == "--schedule") {
                cfg.schedule = lab::argument(argc, argv, i, key);
            }
            else throw std::invalid_argument("Unknown option: " + key + ". Use --help for usage.");
        }
        lab::positive(cfg.m, "--m");
        lab::positive(cfg.k, "--k");
        lab::positive(cfg.n, "--n");
        lab::positive(cfg.tile, "--tile");
        lab::positive(cfg.block_nnz, "--block-nnz");
        lab::positive(cfg.repeat, "--repeat");
        lab::positive(cfg.threads, "--threads");
        lab::require(cfg.nnz_per_row <= cfg.k, "--nnz-per-row must not exceed --k (CSR columns must be unique).");
        lab::require(cfg.k <= std::numeric_limits<std::uint32_t>::max(), "--k exceeds the CSR column-index range.");
        lab::matrix_size(cfg.m, cfg.n, sizeof(double), "C");
        lab::matrix_size(cfg.k, cfg.n, sizeof(double), "B");
        lab::matrix_size(cfg.m, cfg.nnz_per_row, sizeof(double), "CSR");
        lab::choice(cfg.profile, "--profile", {"uniform", "skewed", "blocked"});
        lab::choice(cfg.kernel, "--kernel", {"serial", "rows", "balanced", "tiled"});
        lab::choice(cfg.schedule, "--schedule", {"static", "dynamic", "guided"});
        spmm::benchmark(cfg);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 2;
    }
}
