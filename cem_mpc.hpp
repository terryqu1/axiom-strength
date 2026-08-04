#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "particle_filter.hpp"
#include "session.hpp"
#include "box_muller.hpp"

using namespace std;

const int HORIZON = 100;
const int ACTION_DIM = 3; // Bench, Squat, Deadlift

struct ActionSequence {
    // 100 days of [Bench_Intensity, Squat_Intensity, Deadlift_Intensity]
    double actions[HORIZON][ACTION_DIM]; 
};

class CEM_MPC {
private:
    int num_iterations;
    int num_samples;
    int num_elites;

    // We need a deterministic step for the hallucination so the optimizer has a stable gradient
    void deterministic_time_integration(Particle* p, double deltaTime) {
        p->upper.muscle_mass *= exp(-deltaTime / tau_muscle_decay_upper);
        p->upper.neural_efficiency *= exp(-deltaTime / tau_neural_decay_upper);
        p->lower.muscle_mass *= exp(-deltaTime / tau_muscle_decay_lower);
        p->lower.neural_efficiency *= exp(-deltaTime / tau_neural_decay_lower);
        p->posterior.muscle_mass *= exp(-deltaTime / tau_muscle_decay_posterior);
        p->posterior.neural_efficiency *= exp(-deltaTime / tau_neural_decay_posterior);
        p->fatigue *= exp(-deltaTime / tau_fatigue_decay);

        // Calculate the resultant force
        p->upper.peak_force_est = p->alpha * (p->upper.muscle_mass * p->upper.neural_efficiency / (1 + p->fatigue));
        p->lower.peak_force_est = p->alpha * (p->lower.muscle_mass * p->lower.neural_efficiency / (1 + p->fatigue));
        p->posterior.peak_force_est = p->alpha * (p->posterior.muscle_mass * p->posterior.neural_efficiency / (1 + p->fatigue));
    }

double evaluate_trajectory(Particle state, const ActionSequence& seq) {
        
        // Anchor weights to the START of the 100-day block.
        double block_start_upper = state.upper.peak_force_est;
        double block_start_lower = state.lower.peak_force_est;
        double block_start_post  = state.posterior.peak_force_est;

        double mean_intensity[ACTION_DIM] = {0.0};
        int active_training_days = 0;
        int sbd_days = 0; // Track days where they do all 3 lifts

        for (int day = 0; day < HORIZON; day++) {
            
            int lifts_today = 0;

            if (seq.actions[day][0] > 0.2) {
                bench b; 
                double intensity = 0.65 + ((seq.actions[day][0] - 0.2) / 0.8) * 0.20;
                b.load = (block_start_upper / 9.81) * intensity; 
                b.reps = 5; b.rpe = 8; b.incline_bias = 0; b.leverage_bias = 0.3; b.tempo_stress = 0; b.range_of_motion = 0;
                for (int set = 0; set < 3; set++) visit(BiomechanicalTransitionVisitor{&state}, variant<std::monostate, bench, squat, deadlift>(b));
                lifts_today++;
                mean_intensity[0] += seq.actions[day][0];
            }
            if (seq.actions[day][1] > 0.2) {
                squat s; 
                double intensity = 0.65 + ((seq.actions[day][1] - 0.2) / 0.8) * 0.20;
                s.load = (block_start_lower / 9.81) * intensity; 
                s.reps = 5; s.rpe = 8; s.leverage_bias = 0.3; s.tempo_stress = 0; s.stance_width = 0.3; s.range_of_motion = 0;
                for (int set = 0; set < 3; set++) visit(BiomechanicalTransitionVisitor{&state}, variant<std::monostate, bench, squat, deadlift>(s));
                lifts_today++;
                mean_intensity[1] += seq.actions[day][1];
            }
            if (seq.actions[day][2] > 0.2) {
                deadlift d; 
                double intensity = 0.70 + ((seq.actions[day][2] - 0.2) / 0.8) * 0.20;
                d.load = (block_start_post / 9.81) * intensity; 
                d.reps = 5; d.rpe = 8; d.leverage_bias = 1.0; d.stance_width = 0.4; d.tempo_stress = 0;
                for (int set = 0; set < 3; set++) visit(BiomechanicalTransitionVisitor{&state}, variant<std::monostate, bench, squat, deadlift>(d));
                lifts_today++;
                mean_intensity[2] += seq.actions[day][2];
            }

            if (lifts_today > 0) active_training_days++;
            if (lifts_today == 3) sbd_days++; 

            // Apply daily fatigue recovery and progression
            deterministic_time_integration(&state, 1.0);
        }

        // Variance Calculation (Smoothness)
        double total_variance = 0.0;
        for (int a = 0; a < ACTION_DIM; a++) mean_intensity[a] /= HORIZON;
        for (int day = 0; day < HORIZON; day++) {
            for (int a = 0; a < ACTION_DIM; a++) {
                double diff = seq.actions[day][a] - mean_intensity[a];
                total_variance += (diff * diff);
            }
        }
        total_variance /= HORIZON; 

        // Base Target: Total estimated force at the END of the 100 days
        double total_force = state.upper.peak_force_est + state.lower.peak_force_est + state.posterior.peak_force_est;
        
       // ==========================================
        // NEW: Exponential Penalty System (No Dead Zones!)
        // ==========================================
        double penalty_score = 0.0;

        // 1. Variance Penalty 
        penalty_score += (total_variance * 2.0); 

        // 2. Heavy SBD Day Penalty 
        if (sbd_days > 10) penalty_score += (sbd_days - 10) * 0.05;

        // 3. Volume Envelope 
        if (active_training_days > 60) {
            penalty_score += (active_training_days - 60) * 0.05; 
        } else if (active_training_days < 35) {
            penalty_score += (35 - active_training_days) * 0.05; 
        }

        // e^(-x) smoothly approaches 0 but never hits it. 
        // A perfect routine has a penalty_score of 0.0, resulting in a multiplier of 1.0!
        double penalty_multiplier = std::exp(-penalty_score);

        return total_force * penalty_multiplier;
    }

public:
    CEM_MPC(int iters = 50, int samples = 500, int elites = 50) 
        : num_iterations(iters), num_samples(samples), num_elites(elites) {}

    ActionSequence solve(const Particle& initial_state) {
        
        // Distribution parameters: Mean and Standard Deviation for every action on every day
        double mu[HORIZON][ACTION_DIM];
        double sigma[HORIZON][ACTION_DIM];

        // Initialize distributions to the center (0.5) with a wide spread (0.25)
        for (int d = 0; d < HORIZON; d++) {
            for (int a = 0; a < ACTION_DIM; a++) {
                mu[d][a] = 0.5;
                sigma[d][a] = 0.25;
            }
        }

        ActionSequence best_sequence;
        double best_reward = -INFINITY;

        cout << "Starting CEM-MPC Optimization over " << HORIZON << " days...\n";

        for (int iter = 0; iter < num_iterations; iter++) {
            vector<pair<double, ActionSequence>> population(num_samples);

            // 1. Sample Population
            for (int s = 0; s < num_samples; s++) {
                for (int d = 0; d < HORIZON; d++) {
                    for (int a = 0; a < ACTION_DIM; a++) {
                        double noise = generateGaussianPoint_cached();
                        double val = mu[d][a] + noise * sigma[d][a];
                        population[s].second.actions[d][a] = std::clamp(val, 0.0, 1.0);
                    }
                }
                // 2. Evaluate Trajectory
                population[s].first = evaluate_trajectory(initial_state, population[s].second);
            }

            // 3. Sort Population (descending by reward)
            std::sort(population.begin(), population.end(), 
                      [](const auto& a, const auto& b) { return a.first > b.first; });

            // Track absolute best
            if (population[0].first > best_reward) {
                best_reward = population[0].first;
                best_sequence = population[0].second;
            }

            if (iter % 10 == 0) {
                cout << "  Iter " << iter << " | Best Total Force Est: " << population[0].first << " N\n";
            }

            // 4. Update Distribution (Fit Gaussian to Elites)
            for (int d = 0; d < HORIZON; d++) {
                for (int a = 0; a < ACTION_DIM; a++) {
                    double new_mu = 0.0;
                    for (int e = 0; e < num_elites; e++) {
                        new_mu += population[e].second.actions[d][a];
                    }
                    new_mu /= num_elites;

                    double new_var = 0.0;
                    for (int e = 0; e < num_elites; e++) {
                        double diff = population[e].second.actions[d][a] - new_mu;
                        new_var += diff * diff;
                    }
                    new_var /= num_elites;

                    mu[d][a] = new_mu;
                    
                    // Add an epsilon (0.02) to sigma to prevent premature convergence (distribution collapse)
                    sigma[d][a] = sqrt(new_var) + 0.02; 
                }
            }
        }
        
        cout << "Optimization Complete. Max Projected Total: " << best_reward << " N\n";
        
        // --- NEW READOUT: Print the next 5 active sessions with sets/reps ---
        cout << "\n=== UPCOMING AI-OPTIMIZED ROUTINE ===\n";
        int sessions_printed = 0;
        
        for (int day = 0; day < HORIZON; day++) {
            double bench_int = best_sequence.actions[day][0];
            double squat_int = best_sequence.actions[day][1];
            double deadlift_int = best_sequence.actions[day][2];
            
            // If any lift is above 0.2, it's considered a training day
            if (bench_int > 0.2 || squat_int > 0.2 || deadlift_int > 0.2) {
                cout << "Day " << day + 1 << ": ";
                
                // Note: The engine currently evaluates all lifts as 1 set of 5 reps
                if (bench_int > 0.2) {
                    double b_load = 60.0 + ((bench_int - 0.2) / 0.8) * 100.0;
                    cout << "[Bench: 1x5 @ " << round(b_load) * 2.20462 << " lbs] ";
                }
                if (squat_int > 0.2) {
                    double s_load = 100.0 + ((squat_int - 0.2) / 0.8) * 120.0;
                    cout << "[Squat: 1x5 @ " << round(s_load) * 2.20462 << " lbs] ";
                }
                if (deadlift_int > 0.2) {
                    double d_load = 120.0 + ((deadlift_int - 0.2) / 0.8) * 140.0;
                    cout << "[Deadlift: 1x5 @ " << round(d_load) * 2.20462 << " lbs] ";
                }
                cout << "\n";
                
                sessions_printed++;
                if (sessions_printed >= 5) break; // Stop after 5 sessions
            }
        }
        cout << "=====================================\n";

        return best_sequence;
    }
};