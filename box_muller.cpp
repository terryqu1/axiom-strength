#include <random>
#include <cmath>
#include <limits>
#include "box_muller.hpp"

using namespace std;

static constexpr double pi = 3.141592653589793;

static mt19937 rng(chrono::steady_clock::now().time_since_epoch().count());

static double rnd(double l, double r) {
    return uniform_real_distribution<double>(l, r)(rng);
}

// uses the box muller transform to get a random number from a normal distribution centered at 0.
double generateGaussianPoint_cached() {

    static bool has_cache = false;
    static double z1;
    if (has_cache) {
        has_cache = false;
        return z1;
    }
    double u1 = 0.0;
    const double min_val = numeric_limits<double>::min();
    while (u1 < min_val) {
        u1 = rnd(0,1);
    }
    double u2 = rnd(0,1);
    double z0 = sqrt(-2*log(u1))*cos(2*pi*u2);
    has_cache = true;
    z1 = sqrt(-2*log(u1))*sin(2*pi*u2);

    return z0;
}