#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <variant>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <fstream>
#include "gaussianProcess.hpp"
#include "session.hpp" 
#include "particle_filter.hpp" 
#include "cem_mpc.hpp" // The new optimizer engine

using namespace std;

// Parser function declaration
vector<Session> initializeLifts(string training_log_file);

// Helper for dynamic time decay
double getDaysBetween(const string& date1, const string& date2) {
    if (date1.empty() || date2.empty()) return 1.0;
    std::tm tm1 = {};
    std::tm tm2 = {};
    std::stringstream ss1(date1), ss2(date2);
    ss1 >> std::get_time(&tm1, "%Y-%m-%d");
    ss2 >> std::get_time(&tm2, "%Y-%m-%d");
    
    std::time_t time1 = std::mktime(&tm1);
    std::time_t time2 = std::mktime(&tm2);
    
    return std::difftime(time2, time1) / (60.0 * 60.0 * 24.0);
}

int main() {
    string filename = "data_files/post_vitruve_training_log.csv";

    // ==========================================
    // PHASE 1: TRACK HISTORICAL LATENT STATE
    // ==========================================
    vector<Session> sessions;
    try {
        sessions = initializeLifts(filename);
    } catch (const invalid_argument& e) {
        cerr << "Error loading data: " << e.what() << "\n";
        return 1;
    }

    int num_particles = 1000;
    Particle* particles = new Particle[num_particles];
    initializeParticles(particles, num_particles);
    
    // NEW: Instantiate the Gaussian Process
    GaussianProcess gp(num_particles);
    // Note: If your GP requires initialization for length scale (l) or sigma_sq, 
    // set those here (e.g., gp.l = 1.5; gp.sigma_sq = 1.0;)

    cout << "--- Filtering Historical Data (Gaussian Transition) ---\n";

    string last_date = "";

    for (size_t i = 0; i < sessions.size(); ++i) {
        const Session& current_session = sessions[i];
        string current_date = current_session.getDate();
        
        double days_elapsed = 1.0; 
        if (!last_date.empty()) {
            days_elapsed = getDaysBetween(last_date, current_date);
            if (days_elapsed <= 0) days_elapsed = 1.0; 
        }
        last_date = current_date;

        const vector<Lift>& day_lifts = current_session.getLifts();

        for (size_t j = 0; j < day_lifts.size(); ++j) {
            const Action& action = day_lifts[j].action;
            Observation obs = day_lifts[j].obs; 
            
            obs.lift_flags = {0.0, 0.0, 0.0};
            if (std::holds_alternative<bench>(action.lift_state)) obs.lift_flags[0] = 1.0;
            else if (std::holds_alternative<squat>(action.lift_state)) obs.lift_flags[1] = 1.0;
            else if (std::holds_alternative<deadlift>(action.lift_state)) obs.lift_flags[2] = 1.0;
            else if (std::holds_alternative<std::monostate>(action.lift_state)) {
                // NEW: Use gaussian_transition for rest days
                gaussian_transition(particles, action, num_particles, (j == 0) ? days_elapsed : 0.0, gp);
                continue; 
            }

            double deltaTime = (j == 0) ? days_elapsed : 0.0;
            // NEW: Use gaussian_transition for training days
            gaussian_transition(particles, action, num_particles, deltaTime, gp);

            if (obs.peak_force <= 0.0) continue; 

            observation(particles, obs, num_particles);
        }
    }

    // ... (Keep Phase 2, 3, and 4 exactly the same as before) ...

    // ==========================================
    // PHASE 2: COLLAPSE PARTICLE CLOUD TO MEAN
    // ==========================================
    // We create a single hypothetical state representing the athlete today
    Particle current_state;
    current_state.alpha = 0;
    current_state.fatigue = 0;
    current_state.upper = {0, 0, 0, 0};
    current_state.lower = {0, 0, 0, 0};
    current_state.posterior = {0, 0, 0, 0};

    for (int p = 0; p < num_particles; p++) {
        double linear_weight = exp(particles[p].weight); 
        
        current_state.alpha += particles[p].alpha * linear_weight;
        current_state.fatigue += particles[p].fatigue * linear_weight;
        
        current_state.upper.muscle_mass += particles[p].upper.muscle_mass * linear_weight;
        current_state.upper.neural_efficiency += particles[p].upper.neural_efficiency * linear_weight;
        
        current_state.lower.muscle_mass += particles[p].lower.muscle_mass * linear_weight;
        current_state.lower.neural_efficiency += particles[p].lower.neural_efficiency * linear_weight;
        
        current_state.posterior.muscle_mass += particles[p].posterior.muscle_mass * linear_weight;
        current_state.posterior.neural_efficiency += particles[p].posterior.neural_efficiency * linear_weight;
    }

    cout << "\n--- Historical Tracking Complete ---\n";
    cout << "Current Athlete Alpha: " << current_state.alpha << "\n";
    cout << "Current Athlete Fatigue: " << current_state.fatigue << "\n";

    // ==========================================
    // PHASE 3: HALLUCINATE & OPTIMIZE
    // ==========================================
    cout << "\n--- Launching AI Coach (CEM-MPC) ---\n";

    // Initialize engine:
    // 50 iterations, 1000 candidate routines per iteration,
    // top 100 candidates survive.
    CEM_MPC coach(50, 1000, 100);

    // Current measured one-repetition maximums.
    // Replace these whenever the athlete's tested or estimated 1RMs change.
    constexpr double bench_1rm_lbs = 235.0;
    constexpr double squat_1rm_lbs = 400.0;
    constexpr double deadlift_1rm_lbs = 475.0;

    // Optimize from physically calibrated strength values rather than
    // the particle filter's currently uncalibrated force estimates.
    ActionSequence optimal_routine = coach.solve_from_1rm_lbs(
        current_state,
        bench_1rm_lbs,
        squat_1rm_lbs,
        deadlift_1rm_lbs
    );

    // ==========================================
    // PHASE 4: EXPORT TO CSV
    // ==========================================
    cout << "\n--- Exporting Routine ---\n";
    ofstream out_file("optimal_100_day_program.csv");
    if (out_file.is_open()) {
        out_file << "Day,Bench_Intensity,Squat_Intensity,Deadlift_Intensity\n";
        for (int day = 0; day < HORIZON; day++) {
            out_file << day + 1 << ","
                     << optimal_routine.actions[day][0] << ","
                     << optimal_routine.actions[day][1] << ","
                     << optimal_routine.actions[day][2] << "\n";
        }
        out_file.close();
        cout << "Successfully wrote to optimal_100_day_program.csv\n";
        cout << "(Values < 0.2 indicate a rest day for that lift)\n";
    } else {
        cerr << "Failed to open output file.\n";
    }

    delete[] particles;
    return 0;
}