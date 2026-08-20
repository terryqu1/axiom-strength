#pragma once
#include "box_muller.hpp"
#include "gaussianProcess.hpp"
#include <algorithm>
#include <variant>
#include <array>

class GaussianProcess;

const int NUM_PARTICLES = 1000;
const int k = 100;

// [0,1] scaling

struct bench {
    // incline_bias: 0.0 (Flat) to 0.3 (Incline) to 1.0 (Shoulder Press)
    double incline_bias;
    // leverage_bias: 0.1 (Close Grip) to 0.3 (Comp Grip) to 0.7 (Wide Grip)
    double leverage_bias;
    // tempo_stress: 0 (Touch & Go) to 1 (1s Pause) to 5 (5s Pause)
    double tempo_stress;
    // range_of_motion: 0 (To Chest) to 0.3 (Spoto/Board)
    double range_of_motion;
    double load;
    double reps;
    double rpe;
};

struct squat {
    // leverage_bias: 0 (Front) to 0.3 (High Bar) to 0.7 (Low Bar) to 1.0 (Good Morning)
    double leverage_bias;
    // tempo_stress: 0 (Normal) to 1 (Pause) to 3 (Tempo)
    double tempo_stress;
    // stance_width: 0 (Close) to 0.3 (Normal) to 0.7 (Wide)
    double stance_width;
    // range_of_motion: 0 (Comp Depth) to 1 (Deep/Deficit)
    double range_of_motion;
    double belt_used = 0; // 0 for no, 1 for yes
    double load;
    double reps;
    double rpe;
};

struct deadlift {
    // leverage_bias: 0 (Sumo/Upright) to 0.5 (Hybrid) to 1.0 (Conventional)
    double leverage_bias;
    // stance_width: 0 (Narrow) to 0.3 (Conv) to 0.4 (Hybrid) to 0.7 (Sumo) to 1.0 (Ultra Sumo)
    double stance_width;
    // tempo_stress: 0 (Normal) to 2 (Pause) to 5 (Long Pause)
    double tempo_stress;
    double straps_used = 0; 
    double belt_used = 0;
    double load;
    double reps;
    double rpe = 7;
};

struct local_state {
    double muscle_mass;
    double neural_efficiency;
    double peak_force_est;
    double velocity_loss_est;
};

// a hypothesis of the athlete's state
struct alignas(32) Particle {
    union {
        struct {
            double alpha;   // global scaling constant (for peak_force_est) that the engine will learn itself
            double beta;    // another global scaling constant (for vel_loss_est) that the engine will learn itself
            double fatigue;
            double log_tau = std::log(3.0); // log form of tau fatigue rate
            double log_gain_morph = std::log(0.0018); // log form of training rate gain
            double log_gain_neuro = std::log(0.0025); // neural efficiency gains
            local_state upper;
            local_state lower;
            local_state posterior;
        };
        double state_vec[17];
    };
    double weight;
    Particle(): state_vec{0}, weight(1.0 / NUM_PARTICLES) {}
};

// lift coefficients for bench
const double w_incline = 1;
const double w_tempo = 1;
const double w_leverage = 1;
const double w_range = 1;
const double w_intensity = 1;

// lift coefficients for squat
const double s_leverage = 1;
const double s_tempo = 1;
const double s_stance = 1;
const double s_rom = 1;
const double s_intensity = 1;

// lift coefficients for deadlift
const double d_leverage = 1;
const double d_tempo = 1;
const double d_stance = 1;
const double d_intensity = 1;

constexpr double biological_scaling_factor = 5e-4;
constexpr double fatigue_scaling_factor = 5e-3;

// decay coefficients
const double tau_muscle_decay_upper = 30; // the greater the slower it decays
const double tau_neural_decay_upper = 15;

const double tau_muscle_decay_lower = 30;
const double tau_neural_decay_lower = 15;

const double tau_muscle_decay_posterior = 30;
const double tau_neural_decay_posterior = 15;

const double tau_fatigue_decay = 3;


// noise coefficients
const double noise_sigma = 0.1;

struct BiomechanicalTransitionVisitor {
    Particle* particle;
    double energy_deficit;


    void operator() (const std::monostate&) const {
        ;
    }
    void operator() (const bench& active_bench) const {
        double morph_rate = std::exp(particle->log_gain_morph);
        double neural_rate = std::exp(particle->log_gain_neuro);

        double upper_stimulus_hypertrophy = morph_rate * (active_bench.load * active_bench.rpe) * (active_bench.reps) * ((1+ w_incline*active_bench.incline_bias) * (1+w_tempo*active_bench.tempo_stress) * (1+w_leverage*active_bench.leverage_bias) * (1+w_range*active_bench.range_of_motion));
        double upper_stimulus_neural = neural_rate * (w_intensity * active_bench.load * active_bench.rpe) * (active_bench.reps) * ((1+ w_incline*active_bench.incline_bias) * (1+w_tempo*active_bench.tempo_stress) * (1+w_leverage*active_bench.leverage_bias) * (1+w_range*active_bench.range_of_motion));
        particle->upper.muscle_mass += upper_stimulus_hypertrophy;
        particle->upper.neural_efficiency += upper_stimulus_neural;
        particle->fatigue += (active_bench.load * active_bench.reps * active_bench.rpe) * fatigue_scaling_factor;
    };
    void operator() (const squat& active_squat) const {
        double morph_rate = std::exp(particle->log_gain_morph);
        double neural_rate = std::exp(particle->log_gain_neuro);

        double lower_stimulus_hypertrophy = morph_rate * (active_squat.load * active_squat.rpe) * (active_squat.reps) * ((1+s_stance*active_squat.stance_width) * (1+s_tempo*active_squat.tempo_stress) * (1+s_leverage*active_squat.leverage_bias) * (1+s_rom*active_squat.range_of_motion));
        double lower_stimulus_neural = neural_rate * (s_intensity * active_squat.load * active_squat.rpe) * (active_squat.reps) * ((1+s_stance*active_squat.stance_width) * (1+s_tempo*active_squat.tempo_stress) * (1+s_leverage*active_squat.leverage_bias) * (1+s_rom*active_squat.range_of_motion));
        particle->lower.muscle_mass += lower_stimulus_hypertrophy;
        particle->lower.neural_efficiency += lower_stimulus_neural;
        particle->fatigue += (active_squat.load * active_squat.reps * active_squat.rpe) * fatigue_scaling_factor;
    };
    void operator() (const deadlift& active_deadlift) const {
        double morph_rate = std::exp(particle->log_gain_morph);
        double neural_rate = std::exp(particle->log_gain_neuro);

        double posterior_stimulus_hypertrophy = morph_rate * (active_deadlift.load * active_deadlift.rpe) * (active_deadlift.reps) * ((1+d_stance*active_deadlift.stance_width) * (1+d_tempo*active_deadlift.tempo_stress) * (1+d_leverage*active_deadlift.leverage_bias));
        double posterior_stimulus_neural = neural_rate * (d_intensity * active_deadlift.load * active_deadlift.rpe) * (active_deadlift.reps) * ((1+d_stance*active_deadlift.stance_width) * (1+d_tempo*active_deadlift.tempo_stress) * (1+d_leverage*active_deadlift.leverage_bias));
        particle->posterior.muscle_mass += posterior_stimulus_hypertrophy;
        particle->posterior.neural_efficiency += posterior_stimulus_neural;
        particle->fatigue += (active_deadlift.load * active_deadlift.reps * active_deadlift.rpe) * fatigue_scaling_factor;
    };
    void time_integration(double deltaTime) {
        particle->upper.muscle_mass = particle->upper.muscle_mass * exp(-deltaTime / tau_muscle_decay_upper);
        particle->upper.neural_efficiency = particle->upper.neural_efficiency * exp(-deltaTime / tau_neural_decay_upper);
        particle->lower.muscle_mass = particle->lower.muscle_mass * exp(-deltaTime / tau_muscle_decay_lower);
        particle->lower.neural_efficiency = particle->lower.neural_efficiency * exp(-deltaTime / tau_neural_decay_lower);
        particle->posterior.muscle_mass = particle->posterior.muscle_mass * exp(-deltaTime / tau_muscle_decay_posterior);
        particle->posterior.neural_efficiency = particle->posterior.neural_efficiency * exp(-deltaTime / tau_neural_decay_posterior);
        particle->fatigue = particle->fatigue * exp(-deltaTime / std::exp(particle->log_tau));
    
        particle->alpha *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * 0.01, 0.8, 1.2);
        particle->beta  *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * 0.01, 0.8, 1.2);

        particle->upper.muscle_mass *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
        particle->upper.neural_efficiency *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
        particle->lower.muscle_mass *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
        particle->lower.neural_efficiency *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
        particle->posterior.muscle_mass *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
        particle->posterior.neural_efficiency *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
        particle->fatigue *= std::clamp(1 + generateGaussianPoint_cached() * sqrt(deltaTime) * noise_sigma, 0.5, 1.5);
    
        particle->upper.peak_force_est = particle->alpha * (particle->upper.muscle_mass * particle->upper.neural_efficiency / (1 + particle->fatigue));
        particle->lower.peak_force_est = particle->alpha * (particle->lower.muscle_mass * particle->lower.neural_efficiency / (1 + particle->fatigue));
        particle->posterior.peak_force_est = particle->alpha * (particle->posterior.muscle_mass * particle->posterior.neural_efficiency / (1 + particle->fatigue));    
    };
};

struct alignas(32) Action {
    variant<std::monostate, bench, squat, deadlift> lift_state; // monostate for rest
    Action() : lift_state(std::monostate{}) {};
};

struct alignas(32) Observation {
    union {
        struct {
            double peak_force;
            double velocity_loss;
            double hrv;
            double total_sleep_minutes;
            double set_mean_velocity;
            double set_mean_force;
            double bodyweight;
            double smoothed_bodyweight;
            double delta_bw;
            double active_energy_burned;
            double basal_energy_burned;
            std::array<double, 3> lift_flags;
            double pad;
        };
        double vec[15];
    }; 
    Observation() : vec{0} {}
    inline constexpr static std::size_t total_size = sizeof(vec)/sizeof(vec[0]);
};

void initializeParticles(Particle* particle, int num_particles = NUM_PARTICLES);
void showParticles(const Particle* particle);
void transition(Particle* particle, const Action& action, int num_particles = NUM_PARTICLES, double deltaTime = 1.0);
void observation(Particle* particle, const Observation& observation, int num_particles=NUM_PARTICLES);
std::ostream& operator<<(std::ostream& os, const Observation& obs);
std::ostream& operator<<(std::ostream& os, const Action& action);
pair<double, double> update_geometry(const Particle& p1, const Particle& p2);
void gaussian_transition(Particle* particles, const Action& action, int num_particles, double dt, GaussianProcess& gp);
tuple<double, double, double> calculateParameters(Particle* particles, int num_particles);