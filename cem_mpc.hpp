#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "particle_filter.hpp"
#include "box_muller.hpp"
 
// Axiom Strength bounded CEM-MPC rollout model.
// Version marker makes it easy to verify that the intended header was compiled.
inline constexpr const char* AXIOM_CEM_MPC_VERSION = "bounded-1rm-v2";

inline constexpr int HORIZON = 100;
inline constexpr int ACTION_DIM = 3; // Bench, squat, deadlift

namespace axiom_mpc_detail {

inline constexpr double GRAVITY_MPS2 = 9.80665;
inline constexpr double KG_TO_LBS = 2.20462262185;
inline constexpr double LBS_TO_NEWTONS = 0.45359237 * GRAVITY_MPS2;
inline constexpr double ACTIVE_ACTION_THRESHOLD = 0.20;

// The MPC rollout uses measured 1RMs as physical anchors. Particle-filter latent
// variables are deliberately not converted directly into pounds or newtons.
inline constexpr double MAX_100_DAY_GAIN = 0.20;      // +20% hard ceiling
inline constexpr double MIN_100_DAY_CAPACITY = 0.90;  // -10% hard floor
inline constexpr double DAILY_DETRAINING = 0.00010;
inline constexpr double TRAINING_GAIN_RATE = 0.00180;
inline constexpr double FATIGUE_DECAY_TAU_DAYS = 3.0;
inline constexpr double FATIGUE_COST_RATE = 0.10;
inline constexpr double READINESS_FATIGUE_COEFF = 0.08;
inline constexpr double MAX_NORMALIZED_FATIGUE = 3.0;

inline bool positive_finite(double value) {
    return std::isfinite(value) && value > 0.0;
}

inline double force_n_to_lbs(double force_n) {
    return (force_n / GRAVITY_MPS2) * KG_TO_LBS;
}

inline double lbs_to_force_n(double pounds) {
    return pounds * LBS_TO_NEWTONS;
}

inline double round_to_nearest_5(double pounds) {
    return std::round(pounds / 5.0) * 5.0;
}

} // namespace axiom_mpc_detail

struct ActionSequence {
    // Normalized daily actions: [bench, squat, deadlift].
    double actions[HORIZON][ACTION_DIM]{};
};

struct LiftPrescription {
    int sets = 0;
    int reps = 0;
    double load_pct = 0.0;
};

struct MPC_RolloutState {
    std::array<double, ACTION_DIM> baseline_force_n{};
    std::array<double, ACTION_DIM> capacity_force_n{};
    double fatigue = 0.0; // dimensionless and bounded
};

struct MPC_TrajectoryEvaluation {
    double objective = -std::numeric_limits<double>::infinity();
    double projected_total_force_n = 0.0;
    double capacity_total_force_n = 0.0;
    double penalty_score = 0.0;
    double final_fatigue = 0.0;
    std::array<double, ACTION_DIM> projected_force_n{};
    std::array<double, ACTION_DIM> capacity_force_n{};
};

struct MPC_CandidateTrajectory {
    MPC_TrajectoryEvaluation evaluation;
    ActionSequence sequence;
};

class CEM_MPC {
private:
    int num_iterations_;
    int num_samples_;
    int num_elites_;

    static LiftPrescription decode_action(double value) {
        using namespace axiom_mpc_detail;

        LiftPrescription prescription;
        if (value <= ACTIVE_ACTION_THRESHOLD) {
            return prescription;
        }

        if (value <= 0.50) {
            prescription.sets = 4;
            prescription.reps = 8;
            const double t = (value - 0.20) / 0.30;
            prescription.load_pct =
                0.60 + 0.10 * std::clamp(t, 0.0, 1.0);
        } else if (value <= 0.80) {
            prescription.sets = 3;
            prescription.reps = 5;
            const double t = (value - 0.50) / 0.30;
            prescription.load_pct =
                0.72 + 0.13 * std::clamp(t, 0.0, 1.0);
        } else {
            prescription.sets = 2;
            prescription.reps = 3;
            const double t = (value - 0.80) / 0.20;
            prescription.load_pct =
                0.85 + 0.07 * std::clamp(t, 0.0, 1.0);
        }

        return prescription;
    }

    static double normalized_training_dose(const LiftPrescription& prescription) {
        if (prescription.sets <= 0 ||
            prescription.reps <= 0 ||
            prescription.load_pct <= 0.0) {
            return 0.0;
        }

        // Typical sessions remain O(1), preventing the deadlift or any other lift
        // from receiving disproportionately large state updates.
        const double volume =
            static_cast<double>(prescription.sets * prescription.reps) *
            prescription.load_pct;
        const double intensity_factor =
            std::pow(prescription.load_pct / 0.75, 1.5);

        return std::clamp((volume / 24.0) * intensity_factor, 0.0, 1.5);
    }

    static MPC_RolloutState make_rollout_state(
        double bench_lbs,
        double squat_lbs,
        double deadlift_lbs
    ) {
        using namespace axiom_mpc_detail;

        if (!positive_finite(bench_lbs) ||
            !positive_finite(squat_lbs) ||
            !positive_finite(deadlift_lbs)) {
            throw std::invalid_argument(
                "Bench, squat, and deadlift 1RMs must be positive finite values."
            );
        }

        // These broad bounds catch unit mistakes such as passing newtons as pounds.
        constexpr double MIN_1RM_LBS = 20.0;
        constexpr double MAX_1RM_LBS = 1500.0;
        if (bench_lbs < MIN_1RM_LBS || bench_lbs > MAX_1RM_LBS ||
            squat_lbs < MIN_1RM_LBS || squat_lbs > MAX_1RM_LBS ||
            deadlift_lbs < MIN_1RM_LBS || deadlift_lbs > MAX_1RM_LBS) {
            throw std::invalid_argument(
                "A supplied 1RM is outside 20-1500 lb. Pass measured 1RMs in pounds."
            );
        }

        MPC_RolloutState state;
        state.baseline_force_n = {
            lbs_to_force_n(bench_lbs),
            lbs_to_force_n(squat_lbs),
            lbs_to_force_n(deadlift_lbs)
        };
        state.capacity_force_n = state.baseline_force_n;
        state.fatigue = 0.0;
        return state;
    }

    static void apply_training_day(
        MPC_RolloutState& state,
        const std::array<double, ACTION_DIM>& actions
    ) {
        using namespace axiom_mpc_detail;

        // Recovery occurs each day before the new session's fatigue is added.
        state.fatigue *= std::exp(-1.0 / FATIGUE_DECAY_TAU_DAYS);

        double total_daily_dose = 0.0;

        for (int lift = 0; lift < ACTION_DIM; ++lift) {
            const LiftPrescription prescription = decode_action(actions[lift]);
            const double dose = normalized_training_dose(prescription);
            total_daily_dose += dose;

            const double baseline = state.baseline_force_n[lift];
            const double ceiling = baseline * (1.0 + MAX_100_DAY_GAIN);
            const double floor = baseline * MIN_100_DAY_CAPACITY;

            // Capacity decays slightly without sufficient stimulus.
            state.capacity_force_n[lift] *= (1.0 - DAILY_DETRAINING);

            if (dose > 0.0) {
                const double current_relative_gain =
                    state.capacity_force_n[lift] / baseline - 1.0;
                const double remaining_gain_fraction = std::clamp(
                    1.0 - current_relative_gain / MAX_100_DAY_GAIN,
                    0.0,
                    1.0
                );
                const double recovery_modifier =
                    std::exp(-0.30 * state.fatigue);
                const double relative_gain =
                    TRAINING_GAIN_RATE * dose *
                    remaining_gain_fraction * recovery_modifier;

                state.capacity_force_n[lift] *= (1.0 + relative_gain);
            }

            // This clamp is the hard numerical guarantee against force explosion.
            state.capacity_force_n[lift] = std::clamp(
                state.capacity_force_n[lift],
                floor,
                ceiling
            );
        }

        state.fatigue += FATIGUE_COST_RATE * total_daily_dose;
        state.fatigue = std::clamp(
            state.fatigue,
            0.0,
            MAX_NORMALIZED_FATIGUE
        );
    }

    MPC_TrajectoryEvaluation evaluate_trajectory(
        const MPC_RolloutState& initial_state,
        const ActionSequence& sequence
    ) const {
        using namespace axiom_mpc_detail;

        MPC_RolloutState state = initial_state;
        std::array<double, ACTION_DIM> mean_intensity{};

        int active_training_days = 0;
        int sbd_days = 0;
        int consecutive_training_days = 0;
        int excessive_consecutive_days = 0;

        for (int day = 0; day < HORIZON; ++day) {
            std::array<double, ACTION_DIM> actions{};
            int lifts_today = 0;

            for (int lift = 0; lift < ACTION_DIM; ++lift) {
                actions[lift] = sequence.actions[day][lift];
                mean_intensity[lift] += actions[lift];
                if (actions[lift] > ACTIVE_ACTION_THRESHOLD) {
                    ++lifts_today;
                }
            }

            if (lifts_today > 0) {
                ++active_training_days;
                ++consecutive_training_days;
                if (consecutive_training_days > 3) {
                    ++excessive_consecutive_days;
                }
            } else {
                consecutive_training_days = 0;
            }

            if (lifts_today == ACTION_DIM) {
                ++sbd_days;
            }

            apply_training_day(state, actions);
        }

        double total_variance = 0.0;
        for (int lift = 0; lift < ACTION_DIM; ++lift) {
            mean_intensity[lift] /= static_cast<double>(HORIZON);
        }
        for (int day = 0; day < HORIZON; ++day) {
            for (int lift = 0; lift < ACTION_DIM; ++lift) {
                const double diff =
                    sequence.actions[day][lift] - mean_intensity[lift];
                total_variance += diff * diff;
            }
        }
        total_variance /= static_cast<double>(HORIZON);

        double penalty_score = total_variance * 1.5;
        if (sbd_days > 10) {
            penalty_score += static_cast<double>(sbd_days - 10) * 0.08;
        }
        if (active_training_days > 60) {
            penalty_score +=
                static_cast<double>(active_training_days - 60) * 0.06;
        } else if (active_training_days < 35) {
            penalty_score +=
                static_cast<double>(35 - active_training_days) * 0.04;
        }
        penalty_score +=
            static_cast<double>(excessive_consecutive_days) * 0.05;

        const double readiness_multiplier =
            std::exp(-READINESS_FATIGUE_COEFF * state.fatigue);

        MPC_TrajectoryEvaluation result;
        result.penalty_score = penalty_score;
        result.final_fatigue = state.fatigue;
        result.capacity_force_n = state.capacity_force_n;

        for (int lift = 0; lift < ACTION_DIM; ++lift) {
            result.projected_force_n[lift] =
                state.capacity_force_n[lift] * readiness_multiplier;
            result.capacity_total_force_n += state.capacity_force_n[lift];
            result.projected_total_force_n += result.projected_force_n[lift];
        }

        result.objective =
            result.projected_total_force_n * std::exp(-penalty_score);
        return result;
    }

    ActionSequence solve_prepared(const MPC_RolloutState& initial_state) const {
        using namespace axiom_mpc_detail;

        double mu[HORIZON][ACTION_DIM]{};
        double sigma[HORIZON][ACTION_DIM]{};

        for (int day = 0; day < HORIZON; ++day) {
            for (int action = 0; action < ACTION_DIM; ++action) {
                mu[day][action] = 0.35;
                sigma[day][action] = 0.22;
            }
        }

        ActionSequence best_sequence{};
        MPC_TrajectoryEvaluation best_evaluation;

        std::cout << "CEM-MPC model: " << AXIOM_CEM_MPC_VERSION << "\n";
        std::cout << "Starting CEM-MPC Optimization over "
                  << HORIZON << " days...\n";
        std::cout << "Calibrated starting 1RMs: Bench "
                  << std::round(force_n_to_lbs(initial_state.baseline_force_n[0]))
                  << " lbs, Squat "
                  << std::round(force_n_to_lbs(initial_state.baseline_force_n[1]))
                  << " lbs, Deadlift "
                  << std::round(force_n_to_lbs(initial_state.baseline_force_n[2]))
                  << " lbs\n";

        for (int iteration = 0; iteration < num_iterations_; ++iteration) {
            std::vector<MPC_CandidateTrajectory> population(
                static_cast<std::size_t>(num_samples_)
            );

            for (int sample = 0; sample < num_samples_; ++sample) {
                for (int day = 0; day < HORIZON; ++day) {
                    for (int action = 0; action < ACTION_DIM; ++action) {
                        const double noise = generateGaussianPoint_cached();
                        const double value =
                            mu[day][action] + noise * sigma[day][action];
                        population[sample].sequence.actions[day][action] =
                            std::clamp(value, 0.0, 1.0);
                    }
                }

                population[sample].evaluation = evaluate_trajectory(
                    initial_state,
                    population[sample].sequence
                );
            }

            std::sort(
                population.begin(),
                population.end(),
                [](const MPC_CandidateTrajectory& lhs,
                   const MPC_CandidateTrajectory& rhs) {
                    return lhs.evaluation.objective > rhs.evaluation.objective;
                }
            );

            if (population.front().evaluation.objective >
                best_evaluation.objective) {
                best_evaluation = population.front().evaluation;
                best_sequence = population.front().sequence;
            }

            if (iteration % 10 == 0) {
                std::cout << "  Iter " << iteration
                          << " | Best objective: "
                          << population.front().evaluation.objective
                          << " | Projected total: "
                          << force_n_to_lbs(
                                 population.front().evaluation.projected_total_force_n
                             )
                          << " lb-equivalent"
                          << " | Final fatigue: "
                          << population.front().evaluation.final_fatigue
                          << "\n";
            }

            for (int day = 0; day < HORIZON; ++day) {
                for (int action = 0; action < ACTION_DIM; ++action) {
                    double new_mu = 0.0;
                    for (int elite = 0; elite < num_elites_; ++elite) {
                        new_mu +=
                            population[elite].sequence.actions[day][action];
                    }
                    new_mu /= static_cast<double>(num_elites_);

                    double new_variance = 0.0;
                    for (int elite = 0; elite < num_elites_; ++elite) {
                        const double difference =
                            population[elite].sequence.actions[day][action] -
                            new_mu;
                        new_variance += difference * difference;
                    }
                    new_variance /= static_cast<double>(num_elites_);

                    mu[day][action] = new_mu;
                    sigma[day][action] =
                        std::sqrt(new_variance) + 0.015;
                }
            }
        }

        std::cout << "Optimization Complete.\n"
                  << "  Best objective score: "
                  << best_evaluation.objective << "\n"
                  << "  Penalty score: "
                  << best_evaluation.penalty_score << "\n"
                  << "  Final normalized fatigue: "
                  << best_evaluation.final_fatigue << "\n"
                  << "  Projected readiness-adjusted 1RMs: Bench "
                  << force_n_to_lbs(best_evaluation.projected_force_n[0])
                  << " lbs, Squat "
                  << force_n_to_lbs(best_evaluation.projected_force_n[1])
                  << " lbs, Deadlift "
                  << force_n_to_lbs(best_evaluation.projected_force_n[2])
                  << " lbs\n"
                  << "  Underlying capacity estimates: Bench "
                  << force_n_to_lbs(best_evaluation.capacity_force_n[0])
                  << " lbs, Squat "
                  << force_n_to_lbs(best_evaluation.capacity_force_n[1])
                  << " lbs, Deadlift "
                  << force_n_to_lbs(best_evaluation.capacity_force_n[2])
                  << " lbs\n";

        std::cout << "\n=== UPCOMING AI-OPTIMIZED ROUTINE ===\n";
        int sessions_printed = 0;

        const std::array<double, ACTION_DIM> baseline_lbs = {
            force_n_to_lbs(initial_state.baseline_force_n[0]),
            force_n_to_lbs(initial_state.baseline_force_n[1]),
            force_n_to_lbs(initial_state.baseline_force_n[2])
        };
        const std::array<std::string, ACTION_DIM> lift_names = {
            "Bench", "Squat", "Deadlift"
        };

        for (int day = 0; day < HORIZON; ++day) {
            bool active_day = false;
            for (int lift = 0; lift < ACTION_DIM; ++lift) {
                if (best_sequence.actions[day][lift] >
                    ACTIVE_ACTION_THRESHOLD) {
                    active_day = true;
                    break;
                }
            }
            if (!active_day) {
                continue;
            }

            std::cout << "Day " << day + 1 << ": ";
            for (int lift = 0; lift < ACTION_DIM; ++lift) {
                const LiftPrescription prescription =
                    decode_action(best_sequence.actions[day][lift]);
                if (prescription.sets == 0) {
                    continue;
                }

                const double prescribed_lbs = round_to_nearest_5(
                    baseline_lbs[lift] * prescription.load_pct
                );
                std::cout << "[" << lift_names[lift] << ": "
                          << prescription.sets << "x"
                          << prescription.reps << " @ "
                          << prescribed_lbs << " lbs] ";
            }
            std::cout << "\n";

            ++sessions_printed;
            if (sessions_printed >= 5) {
                break;
            }
        }

        if (sessions_printed == 0) {
            std::cout <<
                "No active sessions were selected above the action threshold.\n";
        }
        std::cout << "=====================================\n";

        return best_sequence;
    }

public:
    CEM_MPC(int iterations = 50, int samples = 500, int elites = 50)
        : num_iterations_(iterations),
          num_samples_(samples),
          num_elites_(elites) {
        if (num_iterations_ <= 0 ||
            num_samples_ <= 0 ||
            num_elites_ <= 0) {
            throw std::invalid_argument(
                "CEM iterations, samples, and elites must be positive."
            );
        }
        if (num_elites_ > num_samples_) {
            throw std::invalid_argument(
                "The number of CEM elites cannot exceed the sample count."
            );
        }
    }

    // Mandatory safe entry point. The historical state remains available to the
    // caller, but its uncalibrated latent quantities are not interpreted as force.
    ActionSequence solve_from_1rm_lbs(
        const Particle& historical_state,
        double bench_lbs,
        double squat_lbs,
        double deadlift_lbs
    ) const {
        (void)historical_state;
        return solve_prepared(
            make_rollout_state(bench_lbs, squat_lbs, deadlift_lbs)
        );
    }

    // Convenience overload when no Particle object is needed by the caller.
    ActionSequence solve_from_1rm_lbs(
        double bench_lbs,
        double squat_lbs,
        double deadlift_lbs
    ) const {
        return solve_prepared(
            make_rollout_state(bench_lbs, squat_lbs, deadlift_lbs)
        );
    }

    // Intentionally disabled: latent particle states are not calibrated physical
    // force measurements. This prevents accidental reintroduction of the runaway
    // muscle_mass * neural_efficiency force equation inside MPC.
    ActionSequence solve(const Particle&) const = delete;
};