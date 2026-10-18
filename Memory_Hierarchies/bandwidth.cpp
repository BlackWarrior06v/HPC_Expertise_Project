#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

void triad(std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c, const double s, std::size_t n){
    for (std::size_t k = 0; k < n; ++k){
        a[k] = b[k] + c[k] * s;
    }
}

double measure(std::vector<double>& a, const std::vector<double>& b, const std::vector<double>& c, const double s, std::size_t n){
    const std::size_t total_elements = 200'000'000;
    const std::size_t reps = std::max<std::size_t>(1, total_elements / n);

    // warmup
    triad(a, b, c, s, n);
    auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < reps; ++r) {
        triad(a, b, c, s, n);
        asm volatile("" ::: "memory");
    }
    auto t1 = std::chrono::steady_clock::now();

    double seconds = std::chrono::duration<double>(t1 - t0).count();
    double gbs = 3.0 * 8.0 * n * reps / seconds / 1e9;   // without write-allocate

    return gbs;
}

int main()
{
    const std::size_t KB = 1024;
    const std::size_t n_sizes = 20;          // from 1 KB to 512 MB
    const std::size_t n_reps = 5;

    std::vector<double> a,b,c;
    double s = 3.0;
    std::vector<double> gbs(n_sizes);
    const std::size_t n_max = (KB << (n_sizes - 1)) / sizeof(double);
    a.reserve(n_max); b.reserve(n_max); c.reserve(n_max);

    std::FILE* out = std::fopen("bandwidth.txt", "w");
    if (!out) { std::perror("bandwidth.txt"); return 1; }
    std::fprintf(out, "# bytes_per_array  working_set_bytes  GB/s_code  GB/s_with_write_allocate\n");

    for (std::size_t k = 0; k < n_sizes; ++k) {
        const std::size_t size_bytes = KB << k;              // 1 KB * 2^k
        const std::size_t n = size_bytes / sizeof(double);
        a.resize(n);
        b.resize(n);
        c.resize(n);


        // --- Init ---
        for (std::size_t i = 0; i < n; ++i){
            a[i] = 0.0;
            b[i] = 1.0;
            c[i] = 2.0;
        }

        double best = 0.0;
        for (std::size_t r = 0; r < n_reps; ++r) {
            best = std::max(best, measure(a,b,c,s,n));
        }
        double sum = 0.0;
        for (std::size_t i = 0; i < n; ++i) sum += a[i];
        if (sum != 7.0 * n) std::printf("ERROR at size %zu: sum = %f\n", n, sum);
        gbs[k] = best;

        std::printf("%8zu KB  best try %7.2f GB/s\n",
                    size_bytes / KB, gbs[k]);
        std::fprintf(out, "%12zu  %12zu  %10.3f  %10.3f\n",
             size_bytes, 3 * size_bytes, gbs[k], gbs[k] * 4.0 / 3.0);
        std::fflush(out);   // if the job is killed, measured data is already saved
    }

    std::fclose(out);
    return 0;
}