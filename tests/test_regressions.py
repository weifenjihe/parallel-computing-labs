"""Run after building Lab1-Lab4 and the Lab5 8x8 model and installing lab dependencies."""
import csv
import io
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BINARY = {1: 'lab1_spmm', 2: 'lab2_locality', 3: 'lab3_histogram', 4: 'lab4_spmm'}
SMALL = {
    1: ['--m', '17', '--k', '23', '--n', '19', '--nnz-per-row', '3', '--repeat', '1', '--threads', '2', '--tile', '7'],
    2: ['--n', '17', '--repeat', '1', '--threads', '2', '--tile', '7'],
    3: ['--n', '1001', '--bins', '17', '--stripes', '3', '--repeat', '1', '--threads', '2'],
    4: ['--m', '3', '--k', '2', '--n', '7', '--nnz-per-row', '1', '--repeat', '1', '--tile', '3'],
    5: ['--m', '17', '--k', '23', '--n', '19', '--tile-m', '7', '--tile-k', '7', '--tile-n', '7'],
}


def execute(command, cwd, extra_env=None, timeout=45):
    env = os.environ.copy()
    env['MPLCONFIGDIR'] = str(ROOT / '.cache/test-matplotlib')
    env.pop('OMP_THREAD_LIMIT', None)
    env.update(extra_env or {})
    process = subprocess.Popen(command, cwd=cwd, env=env, text=True,
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(process.pid, signal.SIGKILL)
        process.communicate()
        raise AssertionError(f'Timed out: {command}')
    return subprocess.CompletedProcess(command, process.returncode, stdout, stderr)


def python_for(lab):
    local = ROOT / f'Lab{lab}/.venv/bin/python3'
    return str(local) if local.exists() else sys.executable


def program(lab, arguments, ranks=1):
    binary = ROOT / f'Lab{lab}/build' / BINARY[lab] if lab < 5 else ROOT / 'Lab5/build/a8_t64_k128/sim'
    command = [str(binary), *arguments]
    if lab == 4:
        launcher = shutil.which('mpiexec') or shutil.which('mpirun')
        version = subprocess.check_output([launcher, '--version'], text=True)
        command = [launcher, *(['--oversubscribe'] if 'Open MPI' in version else []), '-n', str(ranks), *command]
    return execute(command, ROOT / f'Lab{lab}')


class RegressionTests(unittest.TestCase):
    def test_help_and_missing_values(self):
        for lab in range(1, 6):
            with self.subTest(lab=lab):
                result = program(lab, ['--help'])
                self.assertEqual(result.returncode, 0, result.stderr)
                self.assertIn('--verify', result.stdout)
                result = program(lab, SMALL[lab] + ['--n'])
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn('Missing value', result.stderr)

    def test_unknown_options_and_enums(self):
        for lab in range(1, 6):
            with self.subTest(lab=lab):
                result = program(lab, SMALL[lab] + ['--misspelled', '1'])
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn('Unknown option', result.stderr)
                key = '--buffering' if lab == 5 else '--kernel'
                result = program(lab, SMALL[lab] + [key, 'typo'])
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn('Invalid', result.stderr)

    def test_mpi_argument_errors_report_before_all_ranks_exit(self):
        result = program(4, SMALL[4] + ['--kernel', 'typo'], ranks=4)
        self.assertGreater(result.returncode, 0, result.stderr)
        self.assertIn('Invalid --kernel: typo', result.stderr)
        self.assertNotIn('Signal: Segmentation fault', result.stderr)
        result = program(4, ['--help'], ranks=4)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_zero_and_malformed_numbers(self):
        for lab in range(1, 6):
            for key, value in [('--repeat', '0'), ('--n', '0'), ('--n', '-1'), ('--n', '2junk'), ('--seed', '4294967296')]:
                with self.subTest(lab=lab, key=key, value=value):
                    result = program(lab, SMALL[lab] + [key, value])
                    self.assertGreater(result.returncode, 0, result.stderr)
        for lab in [1, 2, 4, 5]:
            key = '--tile-k' if lab == 5 else '--tile'
            result = program(lab, SMALL[lab] + [key, '0'])
            self.assertGreater(result.returncode, 0, result.stderr)

    def test_csr_unique_column_contract(self):
        for lab in [1, 4]:
            result = program(lab, SMALL[lab] + ['--k', '8', '--nnz-per-row', '32'])
            self.assertGreater(result.returncode, 0, result.stderr)
            self.assertIn('must not exceed', result.stderr)

    def test_spm_capacity_stalls_and_watchdog(self):
        for arguments in [['--tile-m', '65'], ['--tile-n', '65'], ['--tile-k', '129'], ['--stall-percent', '100'], ['--beat-cycles', '0']]:
            with self.subTest(arguments=arguments):
                result = program(5, SMALL[5] + arguments)
                self.assertGreater(result.returncode, 0, result.stderr)
        result = program(5, SMALL[5] + ['--max-cycles', '1'])
        self.assertGreater(result.returncode, 0, result.stderr)
        self.assertIn('exceeded --max-cycles', result.stderr)

    def test_verified_kernels_and_empty_mpi_owners(self):
        kernels = {1: ['serial', 'rows', 'balanced', 'tiled'], 2: ['serial', 'naive', 'tiled'],
                   3: ['serial', 'critical', 'atomic', 'locks', 'private'],
                   4: ['serial', 'broadcast', 'allgather', 'halo', 'overlap'], 5: ['single', 'double']}
        for lab, variants in kernels.items():
            for variant in variants:
                with self.subTest(lab=lab, variant=variant):
                    key = '--buffering' if lab == 5 else '--kernel'
                    result = program(lab, SMALL[lab] + [key, variant, '--verify'], ranks=4 if lab == 4 and variant != 'serial' else 1)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn('VERIFY PASS', result.stderr)
                    rows = list(csv.DictReader(io.StringIO(result.stdout)))
                    self.assertEqual(sum(r['record'] == 'summary' for r in rows), 1)
                    if lab in [1, 2, 3]:
                        expected = {1: 17 * 3 * 19, 2: 17 * 17, 3: 1001}[lab]
                        self.assertEqual(int(rows[0]['updates']), expected)

    def test_python_ranges_and_limited_threads(self):
        for lab in range(1, 6):
            for key in ['--repeat', '--n']:
                with self.subTest(lab=lab, key=key):
                    result = execute([python_for(lab), 'run.py', key, '0'], ROOT / f'Lab{lab}')
                    self.assertGreater(result.returncode, 0, result.stderr)
                    self.assertNotIn('Traceback', result.stderr)
        inputs = {1: ['--m', '17', '--k', '23', '--n', '19', '--nnz-per-row', '3'],
                  2: ['--n', '17', '--tile', '7'], 3: ['--n', '1001', '--stripes', '1', '--distribution', 'uniform']}
        with tempfile.TemporaryDirectory() as output:
            for lab in [1, 2, 3]:
                result = execute([python_for(lab), 'run.py', *inputs[lab], '--threads', '4', '--repeat', '1', '--rounds', '1', '--output', str(Path(output) / str(lab))], ROOT / f'Lab{lab}', {'OMP_THREAD_LIMIT': '2'})
                self.assertGreater(result.returncode, 0, result.stderr)
                self.assertIn('OMP_THREAD_LIMIT', result.stderr)
                self.assertNotIn('StatisticsError', result.stderr)
        for args in [['--tile-m', '65'], ['--tile-k', '129'], ['--stall-percent', '100'], ['--k', '0']]:
            result = execute([python_for(5), 'run.py', '--m', '17', '--array', '8', *args], ROOT / 'Lab5')
            self.assertGreater(result.returncode, 0, result.stderr)
            self.assertNotIn('Traceback', result.stderr)

    def test_alternate_build_directory(self):
        lab = ROOT / 'Lab1'
        cache = (lab / 'build/CMakeCache.txt').read_text()
        compiler = next(line.split('=', 1)[1] for line in cache.splitlines() if line.startswith('CMAKE_CXX_COMPILER:'))
        with tempfile.TemporaryDirectory(prefix='build-test-', dir=lab) as alternate:
            result = execute(['cmake', '-S', '.', '-B', alternate, '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_COMPILER=' + compiler], lab)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = execute(['cmake', '--build', alternate, '--parallel', '4'], lab)
            self.assertEqual(result.returncode, 0, result.stderr)
            output = Path(alternate) / 'measurements'
            result = execute([python_for(1), 'run.py', '--build-dir', alternate, '--m', '17', '--k', '23', '--n', '19', '--nnz-per-row', '3', '--repeat', '1', '--rounds', '1', '--verify', '--output', str(output)], lab)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn('VERIFY PASS', result.stderr)
            metadata = json.loads((output / 'environment.json').read_text())
            self.assertEqual(Path(metadata['build_directory']), Path(alternate).resolve())

class VerificationFailureTests(unittest.TestCase):
    def test_reference_checks_reject_wrong_elements_and_same_sum_counts(self):
        bodies = {
            1: ('#include "verification.hpp"', 'spmm::CsrMatrix a; a.rows=1; a.cols=1; a.row_ptr={0,1}; a.col_idx={0}; a.values={2}; spmm::DenseMatrix b(1,1), c(1,1); b.values={3}; c.values={6}; spmm::verify_output(a,b,c); c.values[0]=5;', 'spmm::verify_output(a,b,c);'),
            2: ('#include "verification.hpp"', 'transpose::Matrix a(2), b(2); a.values={1,2,3,4}; b.values={1,3,2,4}; transpose::verify_output(a,b); b.values[0]=9;', 'transpose::verify_output(a,b);'),
            3: ('#include "verification.hpp"', 'histogram::Input a{0,1,0}; histogram::Counts b{2,1}; histogram::verify_output(a,b); b={1,2};', 'histogram::verify_output(a,b);'),
            4: ('#include "verification.hpp"', 'MPI_Init(&argc,&argv); spmm::Config cfg; cfg.m=cfg.k=cfg.n=1; spmm::Distribution d; d.rank=0; d.ranks=1; d.row_begin=0; spmm::CsrMatrix a; a.rows=a.cols=1; a.row_ptr={0,1}; a.col_idx={0}; a.values={2}; auto b=spmm::generate_dense_rows(0,1,1,cfg.seed+1); spmm::DenseMatrix c(1,1); c.values[0]=2*b.values[0]; spmm::verify_output(a,c,cfg,d,MPI_COMM_WORLD); c.values[0]+=1;', 'spmm::verify_output(a,c,cfg,d,MPI_COMM_WORLD);'),
            5: ('#include "verification.hpp"\n#include "support.hpp"', 'gemm::Config cfg; cfg.m=cfg.k=cfg.n=1; int expected=(int(lab::mix(cfg.seed)%7)-3)*(int(lab::mix(cfg.seed+1)%7)-3); std::vector<std::int32_t> output{expected}; gemm::verify_output(cfg,output); ++output[0];', 'gemm::verify_output(cfg,output);'),
        }
        with tempfile.TemporaryDirectory() as directory:
            for lab, (includes, setup, verify) in bodies.items():
                with self.subTest(lab=lab):
                    source = Path(directory) / f'verification{lab}.cpp'
                    finalize = 'MPI_Finalize();' if lab == 4 else ''
                    source.write_text(includes + '\n#include <iostream>\n#include <stdexcept>\nint main(int argc,char** argv){' + setup + '\ntry{' + verify + '}catch(const std::runtime_error& e){ std::cout<<e.what();' + finalize + 'return 0;} ' + finalize + 'return 1;}')
                    binary = Path(directory) / f'verification{lab}'
                    if lab == 5:
                        compiler = shutil.which('c++')
                        library = str(ROOT / 'Lab5/src/verification.cpp')
                    else:
                        cache = (ROOT / f'Lab{lab}/build/CMakeCache.txt').read_text()
                        compiler = next(line.split('=', 1)[1] for line in cache.splitlines() if line.startswith('CMAKE_CXX_COMPILER:'))
                        library = str(ROOT / f'Lab{lab}/build/liblab{lab}_core.a')
                    if lab == 4:
                        compiler = shutil.which('mpicxx') or shutil.which('mpic++')
                    result = execute([compiler, '-std=c++17', str(source), '-I' + str(ROOT / f'Lab{lab}/include'), library, '-o', str(binary)], ROOT)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    command = [str(binary)]
                    if lab == 4:
                        command = [shutil.which('mpiexec') or shutil.which('mpirun'), '-n', '1', *command]
                    result = execute(command, ROOT)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertIn('VERIFY FAIL', result.stdout)


if __name__ == '__main__':
    unittest.main()
