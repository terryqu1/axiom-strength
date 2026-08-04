#include "particle_filter.hpp"
#include <algorithm>
#include "box_muller.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <variant>
#include <numeric>
#include <span>
#include <utility>
#include "matrix.hpp"
#include "gaussianProcess.hpp"

using namespace std;


const double sigma_sensor = 150; // sensor coefficient for peak force 
const double sigma_vel_sensor = 0.4; // sensor coefficient for velocity loss

// clamp function
double my_clamp(double val, double lo, double hi) {
    return max(min(val, hi), lo);
}

// initialize particles to normal distribution
void initializeParticles(Particle* particle, int num_particles) {
    double min_alpha = 0.5;
    double max_alpha = 10.0;
    double min_beta = 0.01;
    double max_beta = 0.1;
    double lo = 100;
    double hi = 1000;

    for (int i = 0; i < num_particles; i++) {
        particle[i].alpha = min_alpha + (static_cast<double>(rand()) / RAND_MAX) * (max_alpha - min_alpha);
        particle[i].beta = min_beta + (static_cast<double>(rand()) / RAND_MAX) * (max_beta - min_beta);
        particle[i].fatigue = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        particle[i].lower.muscle_mass = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        particle[i].lower.neural_efficiency = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        particle[i].upper.muscle_mass = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        particle[i].upper.neural_efficiency = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        particle[i].posterior.muscle_mass = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        particle[i].posterior.neural_efficiency = lo + (static_cast<double>(rand()) / RAND_MAX) * (hi - lo);
        
        particle[i].weight = -log(num_particles);
    }
}

// print first 10 particles to console
void showParticles(const Particle* particle) {
    for (int i = 0; i < 5; i++) {
        cout << " Fatigue: " << particle[i].fatigue << "\n";
        cout << " Upper:\n Muscle mass: " << particle[i].upper.muscle_mass << " Neural: " << particle[i].upper.neural_efficiency << " Peak force est: " << particle[i].upper.peak_force_est << "\n"; 
        cout << " Lower:\n Muscle mass " << particle[i].lower.muscle_mass << " Neural: " << particle[i].lower.neural_efficiency << " Peak force est: " << particle[i].lower.peak_force_est << "\n";
        cout << " Posterior:\n Muscle mass " << particle[i].posterior.muscle_mass << " Neural: " << particle[i].posterior.neural_efficiency << " Peak force est: " << particle[i].posterior.peak_force_est << "\n";
    }
}

// transition function

void transition(Particle* particles, const Action& current_action, int num_particles, double deltaTime) {
    for (int i = 0; i < num_particles; i++) {
        visit(BiomechanicalTransitionVisitor{&particles[i]}, current_action.lift_state);
        BiomechanicalTransitionVisitor{&particles[i]}.time_integration(deltaTime);
    }
}

double calculateSampleSize(Particle* particle, int num_particles) {
    double sum_sq = 0.0;
    for (int i = 0; i < num_particles; i++) {
        sum_sq += particle[i].weight * particle[i].weight; // assumes weights are currently linear
    }
    return 1.0 / sum_sq;
}

void resample(Particle* particle, int num_particles) {
    Particle* new_particles = new Particle[num_particles];
    double* cumulative_weight = new double[num_particles];

    cumulative_weight[0] = particle[0].weight;
    for (int i = 1; i < num_particles; i++) {
        cumulative_weight[i] = cumulative_weight[i-1] + particle[i].weight;
    }

    double step = 1.0 / num_particles;
    double cursor = (static_cast<double>(rand())/RAND_MAX)*step; // Able to pick any point at random
    int index = 0;

    for (int i = 0; i < num_particles; i++) {
        while (index < num_particles - 1 && cursor > cumulative_weight[index]) {
            index++;
        }
        new_particles[i] = particle[index];
        new_particles[i].weight = 1.0 / num_particles;
        cursor += step;
    }

    for (int i = 0; i < num_particles; i++) {
        particle[i] = new_particles[i];
    }

    delete[] new_particles;
    delete[] cumulative_weight;
}

// observation function
// calculate gaussian likelihood
// recalculuate and normalize weights

void observation(Particle* particle, const Observation& observation, int num_particles) {

    double sum_weights = 0.0;
    double max_log_weight = -INFINITY;

    for (int i = 0; i < num_particles; i++) {
        double error_force = observation.peak_force - particle[i].upper.peak_force_est;
        if (observation.lift_flags[1] == 1) {
            error_force = observation.peak_force - particle[i].lower.peak_force_est;
        } else if (observation.lift_flags[2] == 1) {
            error_force = observation.peak_force - particle[i].posterior.peak_force_est;
        }

        particle[i].weight += -(error_force*error_force)/(2.0*sigma_sensor*sigma_sensor);
        if (particle[i].weight > max_log_weight) {
            max_log_weight = particle[i].weight;
        }
    }

    for (int i = 0; i < num_particles; i++) {
        particle[i].weight -= max_log_weight;
        particle[i].weight = exp(particle[i].weight);
        sum_weights += particle[i].weight;
    }

    for (int i = 0; i < num_particles; i++) {
        particle[i].weight /= sum_weights;
    }

    if (calculateSampleSize(particle, num_particles) < num_particles / 2.0) {
        resample(particle, num_particles);
    }

    for (int i = 0; i < num_particles; i++) {
        particle[i].weight = log(particle[i].weight);
    }

}

// Helper: Cholesky Decomposition to sample from the GP Covariance Matrix
matrix compute_cholesky(matrix& K, int n) {
    matrix L(n, n); 
    
    // Explicitly zero-initialize in case the matrix struct does not
    for(int i = 0; i < n; ++i) {
        for(int j = 0; j < n; ++j) {
            L(i,j) = 0.0;
        }
    }

    for (int i = 0; i < n; i++) {
        for (int j = 0; j <= i; j++) {
            double sum = 0.0;
            for (int k = 0; k < j; k++) {
                sum += L(i, k) * L(j, k);
            }
            if (i == j) {
                // Add a tiny jitter to the diagonal for numerical stability (prevent negative roots)
                L(i, j) = std::sqrt(std::max(1e-9, K(i, i) - sum));
            } else {
                L(i, j) = (1.0 / L(j, j)) * (K(i, j) - sum);
            }
        }
    }
    return L;
}

// The new Gaussian Transition Function
void gaussian_transition(Particle* particles, const Action& action, int num_particles, double dt, GaussianProcess& gp) {
    
    // 1. The Mean Function (Deterministic Update)
    for (int i = 0; i < num_particles; ++i) {
        // Apply your existing deterministic time decay
        particles[i].fatigue *= std::exp(-dt / tau_fatigue_decay);
        // ... (apply decay to muscle mass and neural efficiency here) ...
        
        // Apply the action stimulus
        std::visit(BiomechanicalTransitionVisitor{&particles[i]}, action.lift_state);
    }

    // 2. The Variance Function (Evaluate GP Covariance)
    matrix K = gp.populate_covariance_kernel(particles, num_particles);

    // 3. Decompose the Covariance (Sigma = L * L^T)
    matrix L = compute_cholesky(K, num_particles);

    // 4. Sample and Apply Correlated Noise 
    // We apply this variance independently across each of your latent dimensions
    const int NUM_STATE_DIMS = 7; // e.g., upper/lower/post (muscle+neural) + fatigue

    for (int state_dim = 0; state_dim < NUM_STATE_DIMS; ++state_dim) {
        
        // Generate standard normal vector Z
        std::vector<double> Z(num_particles);
        for (int i = 0; i < num_particles; ++i) {
            Z[i] = generateGaussianPoint_cached();
        }

        // Multiply L * Z to get the correlated noise for this dimension
        std::vector<double> noise(num_particles, 0.0);
        for (int i = 0; i < num_particles; ++i) {
            for (int j = 0; j <= i; ++j) { 
                noise[i] += L(i, j) * Z[j];
            }
        }

        // Inject the correlated variance into the particles
        for (int i = 0; i < num_particles; ++i) {
            switch (state_dim) {
                case 0: particles[i].upper.muscle_mass += noise[i]; break;
                case 1: particles[i].upper.neural_efficiency += noise[i]; break;
                case 2: particles[i].lower.muscle_mass += noise[i]; break;
                case 3: particles[i].lower.neural_efficiency += noise[i]; break;
                case 4: particles[i].posterior.muscle_mass += noise[i]; break;
                case 5: particles[i].posterior.neural_efficiency += noise[i]; break;
                case 6: particles[i].fatigue += noise[i]; break;
            }
        }
    }
}


std::ostream& operator<<(std::ostream& os, const Observation& obs) {
    os << "Observations: ";
    for (auto const& i : obs.vec) {
        os << i << " ";
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const Action& action) {
    std::visit([&os](auto const& alt) {
        using T = std::decay_t<decltype(alt)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            os << "Rest";
        } else if constexpr (std::is_same_v<T, squat>) {
            os << "Load: " << alt.load << " Reps: " << alt.reps << " RPE: " << alt.rpe << " leverage_bias: " << alt.leverage_bias << " tempo_stress: " << alt.tempo_stress << " stance_width: " << alt.stance_width << " ROM: " << alt.range_of_motion << " Belt used: " << alt.belt_used;
        } else if constexpr (std::is_same_v<T, bench>) {
            os << "Load: " << alt.load << " Reps: " << alt.reps << " RPE: " << alt.rpe << " incline_bias: " << alt.incline_bias << " leverage_bias: " << alt.leverage_bias << " tempo_stress: " << alt.tempo_stress << " ROM: " << alt.range_of_motion;
        } else if constexpr (std::is_same_v<T, deadlift>) {
            os << "Load: " << alt.load << " Reps: " << alt.reps << " RPE: " << alt.rpe << " leverage_bias: " << alt.leverage_bias << " stance_width: " << alt.stance_width << " tempo_stress: " << alt.tempo_stress << " Straps used: " << alt.straps_used << " Belt used: " << alt.belt_used;
        }
    }, action.lift_state);
    return os;
}

// calculate euclidean distance
pair<double, double> update_geometry(const Particle& p1, const Particle& p2) {
    std::span span_A{p1.state_vec};
    std::span span_B{p2.state_vec};

    const double d2 = std::transform_reduce(span_A.begin(), span_A.end(), span_B.begin(), 0.0, std::plus(), [](double x, double y) {return (x-y)*(x-y);});
    const double d = sqrt(d2);
    std::pair new_pair(d,d2);
    return new_pair;
}