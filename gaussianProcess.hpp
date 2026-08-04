#pragma once
#include <vector>
#include "matrix.hpp"

struct Particle;

using namespace std;

class GaussianProcess {
private:
    const int tau_fitness = 10;
    const int tau_fatigue = 5;
    const int k1 = 1; 
    const int k2 = 2; 
    const double l = 0.5; 
    const double sigma_sq = 1; 
public:
    // predictive mean
    vector<double> mean;
    // variance
    matrix variance;

    GaussianProcess(int n) : variance(n, n) {};
    // sets the baseline
    void setBaseline();
    // convolve is the helper function to the mean function
    vector<double> convolve(const vector<double>& impulse, const vector<double>& t_vec, int n, double tau);
    // sets the mean function using the fitness banister equation
    void setMeanFunction(
        const vector<double>& t_vec,
        const vector<double>& fit_bench_impulse, 
        const vector<double>& fit_squat_impulse, 
        const vector<double>& fit_deadlift_impulse, 
        const vector<double>& fatigue_impulse, 
        const double parameters[], 
        int num);
    // computes the matern 5/2 covariance kernel
    double compute_kernel_scalar(pair<double, double>) const;

    matrix populate_covariance_kernel(Particle* particles, int n);
    // X is the t_vec (time vector)
    void computeMaternKernel(const vector<double>& X);
    // X_star is the time vector for the unknown test ones
    matrix compute_K_star(const vector<double>& X, const vector<double>& X_star);

    matrix compute_K_star_star(const vector<double>& X_star);

    // prints 5 entries to console
    void showGP() const;

};