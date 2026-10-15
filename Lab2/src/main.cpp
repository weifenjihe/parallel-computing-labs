#include "benchmark.hpp"
#include "validation.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        transpose::Config cfg;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") {
                std::cout << "Matrix transpose: B[j,i] = A[i,j]\n--n 2048 --tile 32 --seed 2026\n--kernel serial|naive|tiled --threads 4 --repeat 7 --verify\n";
                return 0;
            }
            if (key == "--verify") { cfg.verify = true; continue; }
            if (key == "--n") {
                cfg.n = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--tile") {
                cfg.tile = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--threads") {
                cfg.threads = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--repeat") {
                cfg.repeat = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--seed") {
                cfg.seed = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--kernel") {
                cfg.kernel = lab::argument(argc, argv, i, key);
            }
            else throw std::invalid_argument("Unknown option: " + key + ". Use --help for usage.");
        }
        lab::positive(cfg.n, "--n");
        lab::positive(cfg.tile, "--tile");
        lab::positive(cfg.threads, "--threads");
        lab::positive(cfg.repeat, "--repeat");
        lab::matrix_size(cfg.n, cfg.n, sizeof(std::uint32_t), "Matrix");
        lab::choice(cfg.kernel, "--kernel", {"serial", "naive", "tiled"});
        transpose::benchmark(cfg);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 2;
    }
}
