#include "simulator.hpp"
#include "validation.hpp"
#include <iostream>

int main(int argc, char** argv) {
    try {
        gemm::Config cfg;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") {
                std::cout << "SPM/systolic GEMM: C[M,N] = A[M,K] * B[K,N]\n--m 256 --k 256 --n 256 --tile-m 32 --tile-k 32 --tile-n 32\n--buffering single|double --latency 12 --beat-cycles 1\n--stall-percent 6 (0..99) --seed 2026 --repeat 1 --verify\n--max-cycles 100000000 --dump-output FILE\n--trace FILE --trace-start 0 --trace-cycles 4096\n";
                return 0;
            }
            if (key == "--verify") { cfg.verify = true; continue; }
            if (key == "--m") {
                cfg.m = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--k") {
                cfg.k = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--n") {
                cfg.n = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--tile-m") {
                cfg.tile_m = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--tile-k") {
                cfg.tile_k = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--tile-n") {
                cfg.tile_n = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--latency") {
                cfg.latency = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--beat-cycles") {
                cfg.beat_cycles = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--stall-percent") {
                cfg.stall_percent = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--seed") {
                cfg.seed = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--repeat") {
                cfg.repeat = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--buffering") {
                const auto value = lab::argument(argc, argv, i, key);
                lab::choice(value, key, {"single", "double"});
                cfg.double_buffer = value == "double";
            }
            else if (key == "--trace") {
                cfg.trace = lab::argument(argc, argv, i, key);
            }
            else if (key == "--trace-start") {
                cfg.trace_start = lab::number<std::uint64_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--trace-cycles") {
                cfg.trace_cycles = lab::number<std::uint64_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--dump-output") {
                cfg.dump_output = lab::argument(argc, argv, i, key);
            }
            else if (key == "--max-cycles") {
                cfg.max_cycles = lab::number<std::uint64_t>(lab::argument(argc, argv, i, key), key);
            }
            else throw std::invalid_argument("Unknown option: " + key + ". Use --help for usage.");
        }
        lab::positive(cfg.m, "--m");
        lab::positive(cfg.k, "--k");
        lab::positive(cfg.n, "--n");
        lab::positive(cfg.tile_m, "--tile-m");
        lab::positive(cfg.tile_k, "--tile-k");
        lab::positive(cfg.tile_n, "--tile-n");
        lab::positive(cfg.beat_cycles, "--beat-cycles");
        lab::positive(cfg.repeat, "--repeat");
        lab::positive(cfg.trace_cycles, "--trace-cycles");
        lab::positive(cfg.max_cycles, "--max-cycles");
        lab::require(cfg.tile_m <= LAB_SPM_TILE && cfg.tile_n <= LAB_SPM_TILE, "--tile-m/--tile-n must not exceed SPM capacity " + std::to_string(LAB_SPM_TILE) + ".");
        lab::require(cfg.tile_k <= LAB_SPM_K, "--tile-k must not exceed SPM K capacity " + std::to_string(LAB_SPM_K) + ".");
        lab::require(cfg.stall_percent <= 99, "--stall-percent must be between 0 and 99; 100 prevents all address handshakes.");
        const auto address_limit = std::numeric_limits<std::uint32_t>::max() - 256ull;
        lab::require(std::uint64_t(cfg.m) * cfg.k <= address_limit / 2
                     && std::uint64_t(cfg.k) * cfg.n <= address_limit / 2
                     && std::uint64_t(cfg.m) * cfg.n <= address_limit / 4,
                     "Matrix storage exceeds the 32-bit DRAM address range.");
        const auto footprint = 2ull * cfg.m * cfg.k + 2ull * cfg.k * cfg.n + 4ull * cfg.m * cfg.n;
        lab::require(footprint <= std::numeric_limits<std::uint32_t>::max() - 256ull, "Matrix storage exceeds the 32-bit DRAM address range.");
        gemm::benchmark(cfg);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 2;
    }
}
