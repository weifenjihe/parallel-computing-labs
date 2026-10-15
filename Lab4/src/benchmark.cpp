#include "benchmark.hpp"
#include "verification.hpp"
#include "support.hpp"
#include <array>

namespace spmm {
namespace {
std::vector<RankStats> collect(const RankStats& stat, const Distribution& d, MPI_Comm comm) {
    const std::array<std::uint64_t, 7> counts{stat.row_begin, stat.row_end, stat.nnz, stat.local_nnz,
                                            stat.remote_nnz, stat.send_bytes, stat.receive_bytes};
    const std::array<double, 5> times{stat.compute_s, stat.pack_s, stat.mpi_s, stat.wait_s, stat.rank_elapsed_s};
    std::vector<std::uint64_t> all_counts(d.rank == 0 ? d.ranks * counts.size() : 0);
    std::vector<double> all_times(d.rank == 0 ? d.ranks * times.size() : 0);
    MPI_Gather(counts.data(), counts.size(), MPI_UINT64_T, all_counts.data(), counts.size(), MPI_UINT64_T, 0, comm);
    MPI_Gather(times.data(), times.size(), MPI_DOUBLE, all_times.data(), times.size(), MPI_DOUBLE, 0, comm);
    std::vector<RankStats> result;
    if (d.rank == 0)
        for (int p = 0; p < d.ranks; ++p) {
            const auto* v = all_counts.data() + p * counts.size();
            const auto* t = all_times.data() + p * times.size();
            result.push_back({v[0], v[1], v[2], v[3], v[4], v[5], v[6], t[0], t[1], t[2], t[3], t[4]});
        }
    return result;
}

RankStats aggregate(const std::vector<RankStats>& stats) {
    RankStats total;
    for (const auto& s : stats) {
        total.row_end += s.row_end - s.row_begin;
        total.nnz += s.nnz; total.local_nnz += s.local_nnz; total.remote_nnz += s.remote_nnz;
        total.send_bytes += s.send_bytes; total.receive_bytes += s.receive_bytes;
        total.compute_s = std::max(total.compute_s, s.compute_s);
        total.pack_s = std::max(total.pack_s, s.pack_s);
        total.mpi_s = std::max(total.mpi_s, s.mpi_s);
        total.wait_s = std::max(total.wait_s, s.wait_s);
        total.rank_elapsed_s = std::max(total.rank_elapsed_s, s.rank_elapsed_s);
    }
    return total;
}
}

void benchmark(const Config& cfg, MPI_Comm comm) {
    MPI_Barrier(comm);
    const auto preparation_start = MPI_Wtime();
    const auto d = make_distribution(cfg, comm);
    const auto a = generate_sparse_rows(d.row_begin, d.row_end, cfg.m, cfg.k, cfg.nnz_per_row, cfg.profile, cfg.seed);
    const auto b = generate_dense_rows(d.b_begin, d.b_end, cfg.n, cfg.seed + 1);
    DenseMatrix c(a.rows, cfg.n);
    auto workspace = prepare_workspace(cfg, d, a, b, comm);
    const auto local_setup = MPI_Wtime() - preparation_start;
    double setup = 0;
    MPI_Allreduce(&local_setup, &setup, 1, MPI_DOUBLE, MPI_MAX, comm);
    std::vector<double> times, checksums;
    std::vector<std::vector<RankStats>> samples;
    for (int trial = -1; trial < cfg.repeat; ++trial) {
        std::fill(c.values.begin(), c.values.end(), 0);
        RankStats stat;
        stat.row_begin = d.row_begin; stat.row_end = d.row_end; stat.nnz = a.values.size();
        stat.local_nnz = workspace.local_nnz; stat.remote_nnz = workspace.remote_nnz;
        stat.send_bytes = workspace.send_bytes; stat.receive_bytes = workspace.receive_bytes;
        MPI_Barrier(comm);
        const auto start = MPI_Wtime();
        if (cfg.kernel == "serial") serial(a, c, workspace, cfg, stat);
        else if (cfg.kernel == "broadcast") broadcast(a, c, workspace, cfg, d, comm, stat);
        else if (cfg.kernel == "allgather") allgather(a, b, c, workspace, cfg, d, comm, stat);
        else if (cfg.kernel == "halo") halo(b, c, workspace, cfg, d, comm, stat);
        else overlap(b, c, workspace, cfg, d, comm, stat);
        stat.rank_elapsed_s = MPI_Wtime() - start;
        double elapsed = 0;
        MPI_Reduce(&stat.rank_elapsed_s, &elapsed, 1, MPI_DOUBLE, MPI_MAX, 0, comm);
        const auto local_checksum = std::accumulate(c.values.begin(), c.values.end(), 0.0);
        double checksum = 0;
        MPI_Reduce(&local_checksum, &checksum, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
        auto ranks = collect(stat, d, comm);
        if (cfg.verify) verify_output(a, c, cfg, d, comm);
        if (d.rank == 0 && trial >= 0) {
            times.push_back(elapsed); checksums.push_back(checksum); samples.push_back(std::move(ranks));
        }
    }
    if (d.rank != 0) return;
    const auto middle = lab::median_index(times);
    lab::csv_precision();
    std::cout << "record,kernel,profile,partition,m,k,n,nnz_per_row,tile,block_nnz,ranks,seed,rank_id,trial,"
                 "row_begin,row_end,nnz,local_nnz,remote_nnz,send_bytes,receive_bytes,compute_s,pack_s,mpi_s,"
                 "wait_s,rank_elapsed_s,setup_s,elapsed_s,gflops,checksum\n";
    auto write = [&](const char* record, int rank, std::size_t trial, const RankStats& stat) {
        std::cout << record << ',' << cfg.kernel << ',' << cfg.profile << ',' << cfg.partition << ','
                  << cfg.m << ',' << cfg.k << ',' << cfg.n << ',' << cfg.nnz_per_row << ',' << cfg.tile << ','
                  << cfg.block_nnz << ',' << d.ranks << ',' << cfg.seed << ',' << rank << ',' << trial << ','
                  << stat.row_begin << ',' << stat.row_end << ',' << stat.nnz << ',' << stat.local_nnz << ','
                  << stat.remote_nnz << ',' << stat.send_bytes << ',' << stat.receive_bytes << ','
                  << stat.compute_s << ',' << stat.pack_s << ',' << stat.mpi_s << ',' << stat.wait_s << ','
                  << stat.rank_elapsed_s << ',' << setup << ',' << times[trial] << ','
                  << (2.0 * cfg.m * cfg.nnz_per_row * cfg.n / times[trial] / 1e9) << ',' << checksums[trial] << '\n';
    };
    write("summary", -1, middle, aggregate(samples[middle]));
    for (int p = 0; p < d.ranks; ++p) write("rank", p, middle, samples[middle][p]);
    for (std::size_t trial = 0; trial < times.size(); ++trial) write("trial", -1, trial, aggregate(samples[trial]));
}
}
