/**
 * task_1.cpp - n*n 矩阵每列与给定向量的内积
 *
 * 算法1: 逐列访问的平凡算法（列优先遍历，cache 不友好）
 * 算法2: cache 优化算法（行优先遍历，利用空间局部性）
 * 算法3: cache 优化 + 循环展开（减少循环开销 + ILP 并行）
 *
 * 矩阵按行优先(row-major)存储，因此逐列访问会导致大量 cache miss，
 * 而按行遍历可以充分利用 cache line 的空间局部性。
 * 在 cache 优化基础上使用循环展开，可进一步减少循环控制开销并提升 ILP。
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>

using namespace std;
using namespace std::chrono;

// 初始化矩阵和向量（随机值）
void init_data(double* matrix, double* vec, int n) {
    for (int i = 0; i < n * n; i++) {
        matrix[i] = (double)(rand() % 100) / 10.0;
    }
    for (int i = 0; i < n; i++) {
        vec[i] = (double)(rand() % 100) / 10.0;
    }
}

/**
 * 算法1：逐列访问的平凡算法
 * 对每一列 j，计算 result[j] = sum(matrix[i][j] * vec[i]) for i=0..n-1
 * 外层循环遍历列 j，内层循环遍历行 i
 * 由于矩阵按行存储，matrix[i][j] = matrix[i*n+j]，
 * 内层循环中 i 每次+1 导致跨行访问，步长为 n，cache 不友好。
 */
void naive_column_access(const double* matrix, const double* vec, double* result, int n) {
    for (int j = 0; j < n; j++) {
        result[j] = 0.0;
        for (int i = 0; i < n; i++) {
            result[j] += matrix[i * n + j] * vec[i];
        }
    }
}

/**
 * 算法2：cache 优化算法
 * 交换循环顺序：外层循环遍历行 i，内层循环遍历列 j
 * 这样内层循环中 matrix[i*n+j] 在内存中是连续的，
 * 一次 cache line 加载可以服务多次访问，大幅减少 cache miss。
 */
void cache_optimized(const double* matrix, const double* vec, double* result, int n) {
    memset(result, 0, n * sizeof(double));
    for (int i = 0; i < n; i++) {
        double vi = vec[i]; // 提升到寄存器，避免重复访问
        for (int j = 0; j < n; j++) {
            result[j] += matrix[i * n + j] * vi;
        }
    }
}

/**
 * 算法3：cache 优化 + 循环展开（内层列展开x4 + 外层行展开x4）
 *
 * 内层列展开x4：每次迭代处理4列，减少循环控制（cmp/jmp）开销，
 *   同时4个 result[j] 互相独立，CPU可并行执行4条乘加指令（ILP）。
 *
 * 外层行展开x4：每次处理4行，将4个 vec[i] 提升到寄存器，
 *   同一列的4次乘加共享 result[j] 的读写，减少对 result 数组的访存次数，
 *   提高寄存器利用率和计算访存比。
 */
void cache_optimized_unroll(const double* matrix, const double* vec, double* result, int n) {
    memset(result, 0, n * sizeof(double));
    int i;
    // 外层行展开x4
    for (i = 0; i + 3 < n; i += 4) {
        double vi0 = vec[i];
        double vi1 = vec[i + 1];
        double vi2 = vec[i + 2];
        double vi3 = vec[i + 3];
        const double* row0 = matrix + i * n;
        const double* row1 = matrix + (i + 1) * n;
        const double* row2 = matrix + (i + 2) * n;
        const double* row3 = matrix + (i + 3) * n;
        int j;
        // 内层列展开x4
        for (j = 0; j + 3 < n; j += 4) {
            result[j]     += row0[j]     * vi0 + row1[j]     * vi1 + row2[j]     * vi2 + row3[j]     * vi3;
            result[j + 1] += row0[j + 1] * vi0 + row1[j + 1] * vi1 + row2[j + 1] * vi2 + row3[j + 1] * vi3;
            result[j + 2] += row0[j + 2] * vi0 + row1[j + 2] * vi1 + row2[j + 2] * vi2 + row3[j + 2] * vi3;
            result[j + 3] += row0[j + 3] * vi0 + row1[j + 3] * vi1 + row2[j + 3] * vi2 + row3[j + 3] * vi3;
        }
        // 处理列方向剩余元素
        for (; j < n; j++) {
            result[j] += row0[j] * vi0 + row1[j] * vi1 + row2[j] * vi2 + row3[j] * vi3;
        }
    }
    // 处理行方向剩余元素
    for (; i < n; i++) {
        double vi = vec[i];
        for (int j = 0; j < n; j++) {
            result[j] += matrix[i * n + j] * vi;
        }
    }
}

// 验证两种算法结果一致
bool verify_results(const double* r1, const double* r2, int n) {
    for (int i = 0; i < n; i++) {
        if (abs(r1[i] - r2[i]) > 1e-6) {
            return false;
        }
    }
    return true;
}

int main() {
    // 测试不同规模的矩阵：2^5 到 2^14
    vector<int> sizes;
    for (int i = 5; i <= 14; ++i) {
        sizes.push_back(1 << i);   // 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384
    }

    cout << "=== 任务1: 矩阵列向量内积 ===" << endl;
    cout << "矩阵按行优先(row-major)存储" << endl;
    cout << "算法1: 逐列访问（外层列、内层行，跨行步长大，cache不友好）" << endl;
    cout << "算法2: cache优化（外层行、内层列，连续访问，cache友好）" << endl;
    cout << "算法3: cache优化+循环展开（行列各展开x4，减少循环开销+ILP并行）" << endl;
    cout << endl;
    cout << left << setw(8)  << "N"
         << setw(16) << "平凡(ms)"
         << setw(16) << "Cache(ms)"
         << setw(16) << "Unroll(ms)"
         << setw(14) << "加速1vs2"
         << setw(14) << "加速1vs3"
         << setw(8)  << "验证" << endl;
    cout << string(92, '-') << endl;

    srand(42);

    for (int n : sizes) {
        double* matrix = new double[n * n];
        double* vec = new double[n];
        double* result1 = new double[n];
        double* result2 = new double[n];
        double* result3 = new double[n];

        init_data(matrix, vec, n);

        // 预热
        naive_column_access(matrix, vec, result1, n);
        cache_optimized(matrix, vec, result2, n);
        cache_optimized_unroll(matrix, vec, result3, n);

        // 根据矩阵大小确定重复次数
        int repeat = (n <= 512) ? 50 : (n <= 1024) ? 20 : (n <= 2048) ? 5 : 2;

        // 测试算法1：逐列访问
        auto start = high_resolution_clock::now();
        for (int r = 0; r < repeat; r++) {
            naive_column_access(matrix, vec, result1, n);
        }
        auto end = high_resolution_clock::now();
        double time_naive = duration_cast<microseconds>(end - start).count() / 1000.0 / repeat;

        // 测试算法2：cache优化
        start = high_resolution_clock::now();
        for (int r = 0; r < repeat; r++) {
            cache_optimized(matrix, vec, result2, n);
        }
        end = high_resolution_clock::now();
        double time_cache = duration_cast<microseconds>(end - start).count() / 1000.0 / repeat;

        // 测试算法3：cache优化+循环展开
        start = high_resolution_clock::now();
        for (int r = 0; r < repeat; r++) {
            cache_optimized_unroll(matrix, vec, result3, n);
        }
        end = high_resolution_clock::now();
        double time_unroll = duration_cast<microseconds>(end - start).count() / 1000.0 / repeat;

        bool correct2 = verify_results(result1, result2, n);
        bool correct3 = verify_results(result1, result3, n);
        string verify_str = (correct2 && correct3) ? "PASS" : "FAIL";

        cout << left << setw(8)  << n
             << setw(16) << fixed << setprecision(3) << time_naive
             << setw(16) << fixed << setprecision(3) << time_cache
             << setw(16) << fixed << setprecision(3) << time_unroll
             << setw(14) << fixed << setprecision(2) << (time_naive / time_cache)
             << setw(14) << fixed << setprecision(2) << (time_naive / time_unroll)
             << setw(8)  << verify_str << endl;

        delete[] matrix;
        delete[] vec;
        delete[] result1;
        delete[] result2;
        delete[] result3;
    }

    return 0;
}