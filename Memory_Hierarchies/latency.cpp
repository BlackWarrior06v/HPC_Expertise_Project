#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

// Sattolo: random permutation with a SINGLE cycle (start from the identity)
void sattoloShuffle(std::vector<int>& a)
{
    std::mt19937_64 rng(12345);
    for (std::size_t i = a.size() - 1; i > 0; --i) {
        std::uniform_int_distribution<std::size_t> dist(0, i - 1);   // j < i, never j = i
        std::swap(a[i], a[dist(rng)]);
    }
}

double measure(const std::vector<int>& next, std::size_t steps, std::size_t& p_out)
{
    std::size_t p = 0;

    // Warm-up: traverse the array, but never more than 'steps' elements.
    // For arrays larger than the cache, traversing the whole array is useless.
    const std::size_t warm = std::min(next.size(), steps);
    for (std::size_t k = 0; k < warm; ++k) p = next[p];

    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t k = 0; k < steps; ++k) p = next[p];
    auto t1 = std::chrono::steady_clock::now();

    p_out = p;
    return std::chrono::duration<double, std::nano>(t1 - t0).count() / steps;
}

int main()
{
    const std::size_t KB = 1024;
    const std::size_t n_sizes = 21;          // from 1 KB to 1 GB
    const std::size_t n_reps = 5;
    const std::size_t steps = 20'000'000;

    std::vector<int> next;
    std::vector<double> t_cons(n_sizes), t_rand(n_sizes);
    std::size_t p_out = 0, checksum = 0;

    std::FILE* out = std::fopen("latency.txt", "w");
    if (!out) { std::perror("latency.txt"); return 1; }
    std::fprintf(out, "# size_bytes  size_KB  ns_consecutive  ns_random\n");

    for (std::size_t k = 0; k < n_sizes; ++k) {
        const std::size_t size_bytes = KB << k;              // 1 KB * 2^k
        const std::size_t n = size_bytes / sizeof(int);
        next.resize(n);

        // --- Sequential traversal ---
        for (std::size_t i = 0; i < n - 1; ++i) next[i] = static_cast<int>(i + 1);
        next[n - 1] = 0;

        double best = 1e300;
        for (std::size_t r = 0; r < n_reps; ++r) {
            best = std::min(best, measure(next, steps, p_out));
            checksum += p_out;
        }
        t_cons[k] = best;

        // --- Random traversal ---
        for (std::size_t i = 0; i < n; ++i) next[i] = static_cast<int>(i);
        sattoloShuffle(next);

        best = 1e300;
        for (std::size_t r = 0; r < n_reps; ++r) {
            best = std::min(best, measure(next, steps, p_out));
            checksum += p_out;
        }
        t_rand[k] = best;

        std::printf("%8zu KB  sequential %7.2f ns  random %7.2f ns\n",
                    size_bytes / KB, t_cons[k], t_rand[k]);
        std::fprintf(out, "%12zu  %8zu  %10.3f  %10.3f\n",
                     size_bytes, size_bytes / KB, t_cons[k], t_rand[k]);
        std::fflush(out);   // if the job is killed, measured data is already saved
    }

    std::fclose(out);
    std::printf("checksum: %zu\n", checksum);   // prevents the compiler from removing the loops
    return 0;
}