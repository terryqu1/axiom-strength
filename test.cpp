#include "particle_filter.hpp"
#include <cstdlib> 
#include <iostream>
#include "box_muller.hpp"
#include "gaussianProcess.hpp"
#include "iostream"
#include "matrix.hpp"
#include <vector>
#include "session.hpp"

using namespace std;
 
int main() { 
    const int NUM_PARTICLES = 1000;
    Particle* particle = new Particle[NUM_PARTICLES];

    initializeParticles(particle, NUM_PARTICLES);

    // intialize session
    Session session1;

    cout << "Step 0: \n";
    showParticles(particle);

    Action action1;
    action1.lift_state = bench{};


    Observation observation1;

    observation1.peak_force = 731.5;
    observation1.velocity_loss = 28.3;
    observation1.hrv = 70;

    session1.addAction(action1, observation1);

    Lift lift1 = session1.viewLift();

    transition(particle, lift1.action, NUM_PARTICLES);
    observation(particle, lift1.obs, NUM_PARTICLES);
    showParticles(particle);


    delete[] particle;

    GaussianProcess GP(5);

    vector<double> t = {1,2,3,4};
    vector<double> bench_impulse = {2935.8,4033.5,6525.8,0};
    vector<double> squat_impulse = {3521.5, 11227.1,0,0};
    vector<double> deadlift_impulse = {3555.3,0,0,8888.9};
    vector<double> fatigue = {7434.2447, 20779.4819, 8094.164286, 14503.4989};

    //2026-06-28,2935.8,3521.5,3555.3,7434.244705593276
    //2026-06-29,4033.5,11227.1,0.0,20779.48190631083
    //2026-06-30,6525.8,0.0,0.0,8094.164286472148
    //2026-07-01,0.0,0.0,8888.9,14503.498929088928

    vector<double> parameters = {1,1,1,1,1};
    int num = t.size();

    GP.setMeanFunction(t, bench_impulse, squat_impulse, deadlift_impulse, fatigue, parameters.data(), num);
    GP.computeMaternKernel(t);

    GP.showGP();
    vector<double> mean = GP.mean;
    matrix K = GP.variance;

    vector<double> X_star = {5,6,7,8};
    matrix k_star = GP.compute_K_star(t, X_star);
    matrix k_star_star = GP.compute_K_star_star(X_star);

    cout << "Matrix K: \n";
    showMatrix(K);
    cout << "k_star: \n";
    showMatrix(k_star);
    cout << "k_star_star: \n";
    showMatrix(k_star_star);

    // matrix B = multiply(A, transpose(A));
    // showMatrix(B);

    

    return 0;
}