// This is the class for the Gaussian Process
#include <cmath>
#include <vector>
#include <iostream>
#include <cassert>
#include "gaussianProcess.hpp"
#include "matrix.hpp"
#include "particle_filter.hpp"
#include <span>

using namespace std;

vector<double> GaussianProcess::convolve(const vector<double>& impulse, const vector<double>& t_vec, int n, double tau) {
    assert(impulse.size() >= n && t_vec.size() >= n);
    vector<double> F(n, 0.0);
        for (int i = 0; i < n; i++) {
            if (i == 0) {
                F[i] = impulse[i];
            } else {
                double delta_t = t_vec[i] - t_vec[i-1];
                F[i] = impulse[i] + F[i - 1] * exp(-delta_t / tau);
            }
        }
        return F;
    }

void GaussianProcess::setMeanFunction(
        const vector<double>& t_vec,
        const vector<double>& fit_bench_impulse, 
        const vector<double>& fit_squat_impulse, 
        const vector<double>& fit_deadlift_impulse, 
        const vector<double>& fatigue_impulse, 
        const double parameters[], 
        int num) {
    assert(num > 0);
    mean.resize(num);

    vector<double> fit_bench = convolve(fit_bench_impulse, t_vec, num, tau_fitness);
    vector<double> fit_squat = convolve(fit_squat_impulse, t_vec, num, tau_fitness);
    vector<double> fit_deadlift = convolve(fit_deadlift_impulse, t_vec, num, tau_fitness);
    vector<double> fatigue = convolve(fatigue_impulse, t_vec, num, tau_fatigue);
    
    double p0 = parameters[0];
    double k_bench = parameters[1];
    double k_squat = parameters[2];
    double k_deadlift = parameters[3];
    double k_fatigue = parameters[4];

    for (int i = 0; i < num; i++) {
        mean[i] = p0 + (k_bench * fit_bench[i]) + (k_squat * fit_squat[i]) + (k_deadlift * fit_deadlift[i]) - (k_fatigue * fatigue[i]);
    }
}

double GaussianProcess::compute_kernel_scalar(pair<double, double> pair) const {
    auto [d, d2] = pair;
    constexpr double root_5 = 2.2360679775;
    const double v = (root_5 * d) / l;
    const double v2 = (5.0 * d2) / (l * l);

    return (1 + v + v2/3.0) * std::exp(-v);
}

matrix GaussianProcess::populate_covariance_kernel(Particle* particles, int n) {
    matrix K(n,n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            K(i,j) = compute_kernel_scalar(update_geometry(particles[i], particles[j]));
        }
    }
    for (int i = 0; i < n; i++) {
        K(i,i) = 1 + 1e-12;
    }
    return K;
}

// K is the train train matern 5/2 kernel
void GaussianProcess::computeMaternKernel(const vector<double>& X) {
    int n = X.size();
    variance.rows = n;
    variance.cols = n;

    double c1 = sqrt(5)/l;
    double c2 = 5/(3*l*l);

    for (int i = 0; i < n; ++i) {
        double X_i = X[i];
        for (int j = i; j < n; ++j) {
            double r = fabs(X_i - X[j]);
            double c3 = c1*r;
            variance(i,j) = sigma_sq*(1 + c3 + c2*r*r)*exp(-1*c3);
            if (i != j) {
                variance(j,i) = variance(i,j);
            }
        }
    }
}

// K_star is the train test covariance matrix
matrix GaussianProcess::compute_K_star(const vector<double>& X, const vector<double>& X_star) {

    int n = X.size();
    int m = X_star.size();

    matrix k_star(n,m);

    double c1 = sqrt(5)/l;
    double c2 = 5/(3*l*l);

    for (int i = 0; i < n; ++i) {
        double X_i = X[i];
        double* row = k_star.get_row(i);
        for (int j = 0; j < m; ++j) {
            double r = fabs(X_i - X_star[j]);
            double c3 = c1*r;
            *(row+j) = sigma_sq*(1 + c3 + c2*r*r)*exp(-1*c3);
        }
    }

    return k_star;
}

// K_star_star is the test test covariance matrix
matrix GaussianProcess::compute_K_star_star(const vector<double>& X_star) {
    int m = X_star.size();

    double c1 = sqrt(5)/l;
    double c2 = 5/(3*l*l);

    matrix k_star_star(m,m);

    for (int i = 0; i < m; ++i) {
        double X_star_i = X_star[i];
        double* row = k_star_star.get_row(i);
        for (int j = i; j < m; ++j) {
            double r = fabs(X_star_i - X_star[j]);
            double c3 = c1*r;
            k_star_star(i,j) = sigma_sq*(1 + c3 + c2*r*r)*exp(-1*c3);
            if (i != j) {
                k_star_star(j,i) = k_star_star(i,j);
            }
        }
    }

    return k_star_star;
}

void GaussianProcess::showGP() const {
    for (int i = 0; i < min(5, static_cast<int>(mean.size())); ++i) {
        cout << "Mean: " << mean[i] << " ";
    }
    cout << "\n";
    for (int i = 0; i < min(5, static_cast<int>(variance.rows)); ++i) {
        cout << "Variance: ";
        for (int j = 0; j < min(5,static_cast<int>(variance.cols)); ++j) {
            cout << variance.x[i*variance.cols+j] << " ";
        }
        cout << "\n";
    } 
}

// void evaluate(std::span<const Trajectory> rollout_data);