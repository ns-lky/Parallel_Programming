/**
 * task_2.cpp - 计算 n 个数的和
 *
 * 算法1: 逐个累加的平凡算法（链式依赖，流水线停顿）
 * 算法2: 两路链式累加（降低数据依赖，利用超标量并行）
 * 算法3: 两路链式累加 + 循环展开(unroll)优化（减少循环开销）
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <string>

using namespace std;
using namespace std::chrono;

// 初始化数组
void init_data(double* arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] = (double)(rand() % 1000) / 100.0;
    }
}

/**
 * 算法1: 逐个累加（链式依赖）
 * 每一步 sum += a[i] 都依赖上一步的 sum 结果，
 * 形成严格的数据依赖链，CPU无法利用超标量并行执行。
 */
double sum_naive(const double* arr, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum;
}

/**
 * 算法2: 两路链式累加（指令级并行）
 * 使用两个独立的累加变量 sum1, sum2，
 * 交替累加奇偶位置的元素，两条加法指令之间无数据依赖，
 * CPU 可以利用超标量架构同时执行两条加法。
 */
double sum_two_way(const double* arr, int n) {
    double sum1 = 0.0, sum2 = 0.0;
    int i;
    for (i = 0; i + 1 < n; i += 2) {
        sum1 += arr[i];
        sum2 += arr[i + 1];
    }
    // 处理奇数个元素的情况
    if (i < n) {
        sum1 += arr[i];
    }
    return sum1 + sum2;
}

/**
 * 算法3: 循环展开(unroll)优化的两路累加
 * 在两路并行的基础上，每次迭代处理8个元素（4组x2路），
 * 减少循环控制（比较、跳转）的开销，
 * 同时保持指令级并行度。
 */
double sum_two_way_unroll4(const double* arr, int n) {
    double sum1 = 0.0, sum2 = 0.0;
    int i;
    // 每次处理 8 个元素 (4 次展开 x 2 路)
    for (i = 0; i + 7 < n; i += 8) {
        sum1 += arr[i];
        sum2 += arr[i + 1];
        sum1 += arr[i + 2];
        sum2 += arr[i + 3];
        sum1 += arr[i + 4];
        sum2 += arr[i + 5];
        sum1 += arr[i + 6];
        sum2 += arr[i + 7];
    }
    // 处理剩余元素
    for (; i < n; i++) {
        sum1 += arr[i];
    }
    return sum1 + sum2;
}

// 验证结果一致性（使用相对误差，不同累加顺序会产生浮点舍入差异）
bool verify(double a, double b) {
    double denom = max(abs(a), abs(b));
    if (denom < 1e-10) return true;
    return abs(a - b) / denom < 1e-6;
}

int main() {
    // 测试规模：2^15 到 2^29
    vector<int> sizes;
    for (int i = 15; i <= 29; ++i) {
        sizes.push_back(1 << i);   
    }

    cout << "=== 任务2: n个数求和 ===" << endl;
    cout << "算法1: 链式累加（逐个累加，指令间有数据依赖）" << endl;
    cout << "算法2: 两路链式累加（两个独立累加变量，ILP友好）" << endl;
    cout << "算法3: 两路+循环展开x4（减少循环开销+ILP）" << endl;
    cout << endl;

    srand(42);

    for (int n : sizes) {
        double* arr = new double[n];
        init_data(arr, n);

        // 根据 n 的大小调整重复次数，避免测试时间过长
        int repeat;
        if (n <= 10000) repeat = 10000;
        else if (n <= 100000) repeat = 1000;
        else if (n <= 1000000) repeat = 100;
        else if (n <= 10000000) repeat = 10;
        else repeat = 3;   // 适用于 2^26 ≈ 67M

        cout << "--- N = " << n << " (重复 " << repeat << " 次) ---" << endl;
        cout << left << setw(30) << "算法"
             << setw(15) << "时间(ms)"
             << setw(15) << "加速比"
             << setw(10) << "验证" << endl;
        cout << string(70, '-') << endl;

        double baseline_time = 0;
        double ref_result = 0;

        // 算法1: 链式累加
        {
            double result = 0;
            auto start = high_resolution_clock::now();
            for (int r = 0; r < repeat; r++) {
                result = sum_naive(arr, n);
            }
            auto end = high_resolution_clock::now();
            double time_ms = duration_cast<microseconds>(end - start).count() / 1000.0 / repeat;
            baseline_time = time_ms;
            ref_result = result;
            cout << left << setw(30) << "1.链式累加"
                 << setw(15) << fixed << setprecision(4) << time_ms
                 << setw(15) << "1.00x"
                 << setw(10) << "REF" << endl;
        }

        // 算法2: 两路链式
        {
            double result = 0;
            auto start = high_resolution_clock::now();
            for (int r = 0; r < repeat; r++) {
                result = sum_two_way(arr, n);
            }
            auto end = high_resolution_clock::now();
            double time_ms = duration_cast<microseconds>(end - start).count() / 1000.0 / repeat;
            cout << left << setw(30) << "2.两路链式累加"
                 << setw(15) << fixed << setprecision(4) << time_ms
                 << setw(15) << (to_string(baseline_time / time_ms).substr(0, 4) + "x")
                 << setw(10) << (verify(result, ref_result) ? "PASS" : "FAIL") << endl;
        }

        // 算法3: 两路+循环展开x4
        {
            double result = 0;
            auto start = high_resolution_clock::now();
            for (int r = 0; r < repeat; r++) {
                result = sum_two_way_unroll4(arr, n);
            }
            auto end = high_resolution_clock::now();
            double time_ms = duration_cast<microseconds>(end - start).count() / 1000.0 / repeat;
            cout << left << setw(30) << "3.两路+展开x4"
                 << setw(15) << fixed << setprecision(4) << time_ms
                 << setw(15) << (to_string(baseline_time / time_ms).substr(0, 4) + "x")
                 << setw(10) << (verify(result, ref_result) ? "PASS" : "FAIL") << endl;
        }

        cout << endl;
        delete[] arr;
    }

    return 0;
}