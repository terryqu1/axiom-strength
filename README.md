# Axiom Strength

**A research prototype for adaptive athletic periodization using probabilistic state estimation, uncertainty-aware dynamics, and model-predictive control.**

Axiom Strength explores whether strength-training periodization can be reframed as a **closed-loop control problem under uncertainty**: estimate an athlete's hidden physiological state from noisy observations, model how that state may evolve, and eventually use a controller to select future training trajectories while respecting fatigue and safety constraints.

The project was developed through the **Trustworthy, Intelligent, and Explainable Robotics (TIER) Lab at Hunter College, CUNY**, under the mentorship of **Dr. Raj Korpan**.

---

## 1. Problem

Most training programs are static templates. They prescribe future workouts without continuously inferring how an athlete's underlying physiological state changes in response to training, recovery, sleep, and day-to-day variability.

Axiom Strength asks:

> **Can athletic periodization become a closed-loop controller that adapts after each observed set?**

The underlying challenge is that quantities such as fatigue, neural efficiency, and tissue adaptation are not directly observable. Instead, the system must reason from noisy signals such as force, velocity, HRV, sleep, and completed training.

The long-term objective is not simply to generate workouts. It is to build an **inspectable, uncertainty-aware, and eventually formally constrained system** for reasoning about sequential physical interventions.

---

## 2. Architecture

### Intended Full Architecture

The research architecture is:

```text
Athlete observations
force / velocity / HRV / sleep
        │
        ▼
Particle Filter
latent-state estimation
        │
        ▼
Gaussian-Process Dynamics
probabilistic forward model
        │
        ▼
CEM-MPC
trajectory optimization
        │
        ▼
Training Prescription
load / reps / rest
        │
        └──────────► new observations
```

The overall architecture can be described as Bayesian state estimation, Gaussian-process dynamics, and Cross-Entropy Method Model Predictive Control.

### Current Implementation

The **current implementation is deliberately not fully connected**.

```text
STATE-ESTIMATION BRANCH

Observations
    │
    ▼
Particle Filter
    │
    ▼
Gaussian-Process Dynamics
    │
    ╳
    │
    ╳  intentionally detached
    │


CONTROL BRANCH

Controlled / provisional inputs
    │
    ▼
AI-generated CEM-MPC prototype
    │
    ▼
Candidate training trajectories
```

The CEM-MPC remains detached from the particle-filter / Gaussian-process subsystem because the upstream physiological model is not yet sufficiently calibrated.

Passing uncalibrated latent-state estimates directly into the controller would allow state-estimation and model error to propagate into trajectory optimization. A sufficiently aggressive optimizer can exploit errors in an inaccurate model, producing plans that look optimal mathematically while having little physical meaning.

Integration is therefore intentionally deferred until the upstream model has been calibrated and validated.

---

## 3. Mathematical Formulation and Core Algorithms

### POMDP Formulation

The broader system can be viewed as a partially observable Markov decision process.

The research formulation includes:

#### State (S)

Latent athlete variables such as:

* muscle mass / capacity,
* neural efficiency,
* fatigue,
* estimated peak force.

#### Actions (A)

Possible actions include:

* bench press,
* squat,
* deadlift,
* rest,

with continuous intensity choices.

#### Observations (O)

Observable signals may include:

* peak force,
* mean force,
* mean velocity,
* velocity loss,
* HRV,
* sleep.

Conceptually:

$$
o_t
\rightarrow
b_t(s)
\rightarrow
p(s_{t+1}\mid s_t,a_t)
\rightarrow
\text{trajectory optimization}
$$

where an observation updates a belief over hidden physiological state, a transition model predicts future state distributions, and a controller eventually selects actions.

---

### Particle Filter

The particle filter is the current mechanism for representing uncertainty over hidden physiological state.

A collection of particles represents possible athlete states:

```math
x^{(i)}
=
[
\text{muscle},
\text{neural},
\text{fatigue},
\text{force},
\dots
]
```

The research prototype uses **1,000 state hypotheses**.

Given an observation, each particle receives a likelihood weight based on how well its predicted measurement agrees with the observed value.

A representative Gaussian observation model is:

$$
w_i
\propto
\exp
\left(
-\frac{
(y_{\text{obs}}-\hat y_i)^2
}{
2\sigma_{\text{obs}}^2
}
\right)
$$

The resulting weights are normalized and used for resampling, producing an updated belief over latent state.

The particle filter should currently be understood as an **experimental state-estimation subsystem**, not as a validated source of controller inputs.

---

### Physiological Transition Model

Training actions influence modeled physiological state.

The prototype represents bench, squat, and deadlift training in terms involving:

$$
\text{load}
\times
\text{repetitions}
\times
\text{RPE}
$$

These actions generate modeled impulses affecting quantities such as:

* hypertrophy,
* neural adaptation,
* fatigue.

The numerical parameters governing these effects remain subject to empirical calibration.

---

### Gaussian-Process Dynamics

A Gaussian process is used to represent uncertainty in forward physiological dynamics.

The current research formulation uses:

* a fitness-fatigue / Banister-style mean model,
* a Matérn (5/2) covariance structure.

A standard Matérn (5/2) kernel has the form:

```math
k(x,x')
=
\sigma^2
\left(
1+\sqrt{5}r+\frac{5}{3}r^2
\right)
e^{-\sqrt{5}r}
```

where

```math
r=\frac{\|x-x'\|}{\ell}
```

The intended role of the GP is to model uncertain state transitions:

$$
p(s_{t+1}\mid s_t,a_t)
$$

At present, however, the numerical state predictions from this subsystem are **not fed directly into CEM-MPC** because they have not yet been calibrated sufficiently against longitudinal athlete data.

---

### CEM-MPC Prototype

The repository also contains a prototype **Cross-Entropy Method Model Predictive Controller** intended to explore long-horizon training optimization.

The prototype searches candidate 100-day bench / squat / deadlift trajectories. The optimization process is as follows:

1. sample candidate action sequences,
2. simulate and score them,
3. select elite plans,
4. refit the sampling distribution,
5. repeat.

The research configuration includes approximately **50 × 500 rollout evaluations** during CEM search.

The objective favors projected force while penalizing undesirable behavior such as:

* erratic loading,
* excessive heavy training,
* unsafe volume.

#### Important implementation provenance

The **current CEM-MPC prototype was generated entirely using generative-AI coding tools**.

I did not hand-write the CEM-MPC implementation.

My contribution to this component was primarily:

* defining its intended role in the architecture,
* specifying the training-action space and optimization purpose,
* evaluating how it should fit into the research system,
* deciding that it should remain detached from the uncalibrated state-estimation branch,
* using it as an exploratory prototype for future closed-loop control research.

The CEM-MPC should therefore be understood as an **AI-generated research prototype under human-directed system design**, not as independently authored implementation.

---

## 4. Current Status

### Implemented / Prototyped

#### State-estimation and dynamics

* [x] C++ particle-filter implementation
* [x] Latent physiological-state representation
* [x] Observation likelihoods
* [x] 1,000-particle belief representation
* [x] Gaussian-process forward model
* [x] Matérn (5/2) covariance structure
* [x] Fitness-fatigue-inspired dynamics
* [x] Bench / squat / deadlift biomechanical transitions

#### Optimization

* [x] AI-generated CEM-MPC prototype
* [x] Bench / squat / deadlift / rest action space
* [x] Long-horizon trajectory search
* [x] Objective penalties for undesirable loading patterns
* [x] Candidate training-plan generation

The documented research prototype can generate future candidate training prescriptions.

### Intentionally Not Integrated

* [ ] Particle Filter → Gaussian Process → CEM-MPC live coupling

This is **intentionally withheld**, not merely unfinished.

The particle-filter and GP outputs are not yet sufficiently calibrated for their numerical estimates to be treated as trustworthy controller inputs.

---

## 5. Repository Structure

Conceptually, the main modules are:

```text
Particle State
Observation Likelihood
Force Estimation
Biomechanical Transition
Gaussian-Process Dynamics
Correlated GP Noise
CEM-MPC Trajectory Search
POMDP / System Coordination
```


---
## 6. Build and Run

Axiom Strength is written in **C++20**, uses **CMake**, and has no external library dependencies beyond the C++ standard library.

### Requirements

* C++20-compatible compiler
* `g++` or `clang++`
* CMake
* standard C++ library
* no external libraries

```text
Language:      C++20
Compiler:      g++ or clang++
Build system:  CMake
Dependencies:  Standard library only
Executable:    Proj
Build folder:  build/
```

### Configure and Build

From the repository root:

```bash
mkdir -p build
cd build
cmake ..
cmake --build .
```

The project's `CMakeLists.txt` creates an executable named:

```text
Proj
```

inside the `build/` directory.

### Runtime Data Requirement

The public repository **does not include the private longitudinal training logs used by the current prototype**.

As a result, the project can be configured and compiled from the public repository, but the current executable cannot complete its normal data-driven execution without the required training-log input.

This data was intentionally excluded to avoid publishing private personal training records.

### Run

With the required local data available in the expected format/location:

```bash
./build/Proj
```

or, from inside the build directory:

```bash
./Proj
```

Without the private training data, the executable should not be expected to reproduce the full current research run.

### Reproducibility Status

The repository currently provides:

* public source code,
* CMake build configuration,
* dependency-free C++20 compilation,
* the implemented research architecture and algorithms.

It does **not currently provide**:

* the private athlete training logs used by the prototype,
* a public synthetic replacement dataset,
* a fully reproducible end-to-end example run.

A future improvement is to add a small synthetic or anonymized example dataset so that users can exercise the full pipeline without access to private records.

---

## Reproducibility Limitation

The current public repository is **build-reproducible but not yet fully run-reproducible**.

The executable depends on private longitudinal training logs that have been deliberately removed from version control.

This means an external user can inspect and compile the implementation, but cannot reproduce the complete current execution path from the public repository alone.

Planned improvements include:

* adding a synthetic athlete dataset,
* documenting the expected input schema,
* providing a minimal public example configuration,
* adding a demo mode that does not require private data.

The private dataset will remain excluded from the repository.

---

## 7. Tests and Validation

Validation is intentionally staged so that failures in one subsystem are not confused with failures in another.

### Stage 1 — Numerical Unit Testing

Planned / relevant tests include:

* particle resampling,
* observation weighting,
* kernel symmetry,
* covariance / Cholesky stability,
* action clamping,
* rollout scoring.

### Stage 2 — Physiological Calibration

Fit the state-estimation and dynamics parameters using longitudinal athlete data.

Possible observable signals include:

* force,
* bar velocity,
* velocity loss,
* load,
* repetitions,
* RPE,
* HRV,
* sleep.

The goal is to determine whether latent-state estimates have sufficiently stable **scale, behavior, and physical meaning** for downstream use.

### Stage 3 — Estimator Validation

Evaluate:

* held-out prediction error,
* uncertainty calibration,
* sensitivity to initialization,
* sensitivity to observation noise,
* parameter identifiability,
* long-horizon stability.

Only after this stage should the state estimator be considered as a source of MPC inputs.

### Stage 4 — MPC Validation in Isolation

The controller should be evaluated independently under controlled assumptions.

Relevant questions include:

* Does increasing modeled fatigue reduce future loading?
* Do tighter safety bounds produce more conservative trajectories?
* Does removing fatigue cost produce predictably more aggressive programs?
* Does changing the reward alter the elite-action distribution as expected?
* Is CEM convergence stable across random seeds?

This isolates optimizer behavior from physiological-model error.

### Stage 5 — Closed-Loop Integration

Only after upstream calibration:

```text
observations
      │
      ▼
particle-filter belief
      │
      ▼
validated forward dynamics
      │
      ▼
CEM-MPC
      │
      ▼
prescription
      │
      ▼
new observations
```

The integrated system can then be tested for stability and sensitivity to state-estimation error.

---

## 8. Results and Example Output

The current research artifact demonstrates the components necessary to explore:

* latent-state estimation,
* probabilistic biological simulation,
* long-horizon trajectory optimization.

A conceptual output format might look like:

```text
STATE ESTIMATION
----------------
particle count:         1000
estimated latent state: ...
uncertainty:            ...

CONTROL PROTOTYPE
-----------------
planning horizon:       100 days
candidate trajectories: ...
objective:              force - fatigue - safety penalties

SELECTED PLAN
-------------
Day 1: ...
Day 2: ...
Day 3: ...
...
```

Replace this example with literal output from the repository before publication.

No empirical claim should currently be made that Axiom Strength:

* improves strength faster than existing programs,
* outperforms expert coaches,
* accurately predicts long-term human adaptation,
* or can safely prescribe autonomous real-world training.

Those remain validation questions.

---

## 9. Limitations and Next Work

### Uncalibrated Physiological State

The largest current limitation is that the particle-filter / GP subsystem has not yet been sufficiently calibrated against real longitudinal data.

A mathematically valid latent variable does not automatically correspond to a reliable physiological quantity.

Potential issues include:

* incorrect state scale,
* poorly identified parameters,
* biased dynamics,
* unrealistic uncertainty,
* long-horizon drift.

### Model Error Can Be Amplified by Optimization

This is the reason the CEM-MPC remains detached.

An optimizer searches aggressively for states and actions that maximize its objective.

If the underlying world model is inaccurate, the optimizer can exploit **model error** instead of discovering genuinely useful strategies.

> **The upstream model must earn integration with the optimizer through calibration and validation.**

### Gaussian-Process Biological Fidelity

The research abstract notes that a purely data-driven model handles uncertainty but lacks strict grounding in biological constraints.

One proposed direction is replacing or augmenting the GP with a **Physics-Informed Neural Network** encoding mechanisms related to:

* tissue capacity,
* fatigue recovery,
* neuromuscular efficiency.

### Formal Verification

Another ongoing direction is a **Lean 4 safety layer**.

The research goal is to eventually:

* bound admissible actions,
* bound objective values,
* prove unsafe states unreachable under specified assumptions,
* map mathematical safety statements to implementation invariants.

### Real-World Data

Future work includes:

* streaming actual LPT data,
* integrating wearable signals,
* calibrating parameters per athlete,
* comparing predicted and observed trajectories.

### Comparative Benchmarking

The system eventually needs direct comparison against:

* static training programs,
* conventional periodization,
* autoregulated coaching.

---

## 10. Attribution and Development Provenance

### Research Context

**Terry Qu**
Research Fellow

**Mentor:** Dr. Raj Korpan

**Trustworthy, Intelligent, and Explainable Robotics (TIER) Lab**
Hunter College, City University of New York

The project was conducted under Dr. Korpan's mentorship through the TIER Lab.

### My Contributions

My contributions include the research and system-level direction of Axiom Strength, including work on:

* framing adaptive athletic periodization as a probabilistic control problem,
* the broader system architecture,
* latent physiological-state modeling,
* uncertainty-aware dynamics,
* determining how state estimation and trajectory optimization should eventually interact,
* identifying the danger of coupling an uncalibrated estimator directly to MPC,
* defining staged calibration and validation requirements,
* investigating physics-informed modeling,
* investigating formal verification as a future safety layer.

Specific implementation provenance should be documented component-by-component rather than assuming all code in the repository was authored manually.

### CEM-MPC Provenance

The current **CEM-MPC prototype was generated entirely using generative-AI coding tools**.

I do **not** claim to have manually implemented the CEM-MPC algorithm.

My role in that component was primarily:

* selecting and specifying its role within the research architecture,
* defining its intended optimization problem,
* evaluating the generated prototype,
* determining its relationship to the rest of the system,
* deliberately withholding integration with the uncalibrated PF/GP branch.

It should therefore be regarded as:

> **an AI-generated prototype used within a human-directed research and system-design process.**

### AI-Assisted Software Development

Generative-AI coding tools were used as part of the project's development workflow.

Where relevant, this repository should distinguish between:

1. **research conception,**
2. **system architecture and specification,**
3. **human-authored code,**
4. **AI-assisted code,**
5. **fully AI-generated prototype code.**

The presence of generated code should not be interpreted as evidence of manual implementation.

Likewise, AI assistance does not replace responsibility for:

* understanding component behavior,
* validating outputs,
* identifying architectural failure modes,
* determining whether components are appropriate to integrate,
* accurately describing the provenance of the work.

---

## Development Philosophy

The system is intentionally being built in stages:

```text
FORMULATE
    ↓
PROTOTYPE
    ↓
TEST COMPONENTS
    ↓
CALIBRATE
    ↓
VALIDATE
    ↓
INTEGRATE
    ↓
TEST CLOSED LOOP
    ↓
FORMALIZE SAFETY PROPERTIES
```

The objective is not to connect the largest possible number of sophisticated algorithms.

It is to determine **when those connections are scientifically and mathematically justified**.

---

## Research Direction

The current project supports the broader hypothesis that:

> **Adaptive training can be represented as an inspectable probabilistic control problem.**

The present work explores the pieces of that architecture separately—state estimation, uncertain dynamics, and long-horizon optimization—while deliberately avoiding premature integration.

The next major milestone is therefore not simply “connect the modules.”

It is:

> **calibrate the physiological model well enough that closing the loop becomes defensible.**
