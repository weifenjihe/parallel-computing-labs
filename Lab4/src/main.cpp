#include "benchmark.hpp"
#include "validation.hpp"
#include <iostream>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    int rank = 0, ranks = 1;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &ranks);
    bool benchmark_started = false;
    try {
        spmm::Config cfg;
        for (int i = 1; i < argc; ++i) {
            const std::string key = argv[i];
            if (key == "--help") {
                if (rank == 0) std::cout << "MPI CSR SpMM: use mpiexec -n P ./build/lab4_spmm [options]\n--m 4096 --k 8192 --n 1024 --nnz-per-row 32 --seed 2026\n--profile uniform|skewed|blocked|banded --partition rows|nnz\n--kernel serial|broadcast|allgather|halo|overlap\n--tile 128 --block-nnz 256 --repeat 7 --verify\nserial requires exactly one MPI process.\n";
                MPI_Finalize();
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
            else if (key == "--tile") {
                cfg.tile = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--block-nnz") {
                cfg.block_nnz = lab::number<std::size_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--repeat") {
                cfg.repeat = lab::number<int>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--seed") {
                cfg.seed = lab::number<std::uint32_t>(lab::argument(argc, argv, i, key), key);
            }
            else if (key == "--profile") {
                cfg.profile = lab::argument(argc, argv, i, key);
            }
            else if (key == "--kernel") {
                cfg.kernel = lab::argument(argc, argv, i, key);
            }
            else if (key == "--partition") {
                cfg.partition = lab::argument(argc, argv, i, key);
            }
            else throw std::invalid_argument("Unknown option: " + key + ". Use --help for usage.");
        }
        lab::positive(cfg.m, "--m");
        lab::positive(cfg.k, "--k");
        lab::positive(cfg.n, "--n");
        lab::positive(cfg.tile, "--tile");
        lab::positive(cfg.block_nnz, "--block-nnz");
        lab::positive(cfg.repeat, "--repeat");
        lab::require(cfg.nnz_per_row <= cfg.k, "--nnz-per-row must not exceed --k (CSR columns must be unique).");
        lab::require(cfg.k <= std::numeric_limits<std::uint32_t>::max(), "--k exceeds the CSR column-index range.");
        lab::matrix_size(cfg.m, cfg.n, sizeof(double), "C");
        lab::matrix_size(cfg.k, cfg.n, sizeof(double), "B");
        lab::matrix_size(cfg.m, cfg.nnz_per_row, sizeof(double), "CSR");
        lab::choice(cfg.profile, "--profile", {"uniform", "skewed", "blocked", "banded"});
        lab::choice(cfg.kernel, "--kernel", {"serial", "broadcast", "allgather", "halo", "overlap"});
        lab::choice(cfg.partition, "--partition", {"rows", "nnz"});
        lab::require(cfg.kernel != "serial" || ranks == 1, "--kernel serial requires mpiexec -n 1.");
        lab::require(cfg.k * cfg.n <= static_cast<std::size_t>(std::numeric_limits<int>::max()), "K*N exceeds the MPI element-count range.");
        benchmark_started = true;
        spmm::benchmark(cfg, MPI_COMM_WORLD);
        MPI_Finalize();
        return 0;
    } catch (const std::exception& error) {
        if (!benchmark_started) {
            if (rank == 0) std::cerr << "Error: " << error.what() << "\n";
            MPI_Finalize();
        } else {
            std::cerr << "Error (rank " << rank << "): " << error.what() << "\n";
            MPI_Abort(MPI_COMM_WORLD, 2);
        }
        return 2;
    }
}
