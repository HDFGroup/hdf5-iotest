# Phase 1: Data Gathering
    Idea: The more data we can collect the better!
## Environment
    - [] Document the file system(s)
    - [] Document the build environment
      - Compilers
      - MPI version
      - HDF5 version and build options
      - Other software versions
## System Performance
    - [] Review specifications and acceptance test results
    - [] Run Fio
    - [] Run IOR
## Application Documentation
### Goals and expectations
    - [] Describe the application
    - [] Document performance goals
    - [] Perform back-of-the-envelope calculations
    - [] Document qualitative behavior expectations
### Baseline
    - [] Document the sequential application baseline
    - [] Document the parallel application baseline(s)
      - Strong and weak scaling
    - [] Capture the outputs of`h5dump -pBH` and `h5stat`
#### Profiles
    - [] Create a sequential/single-process parallel `gperf` + `kcachegrind`
         profile
    - [] Collect Darshan profiles
    - [] Collect Recorder outputs
### Symptoms
    - [] Document user concerns and observations
    - [] Confirm their reproducibility
# Phase 2: Analysis / Diagnosis
    - [] Visualize the data
    - [] If the data does not confirm your expectations document what you
         didn't expect.
    - [] Look for telltale signs indicative of performance problems
      - `gperf`
      - Darshan
      - Recorder
    - [] Document your observations
**No changes to the code up to this point!**
# Phase 3: Performance Improvement Plan
## Goal Setting
## Low-hanging Fruit
    - PFS Settings
    - MPI-IO Hints
    - HDF5 Settings
## Hypothesis Testing

# Phase 4: Execution
## Traps and Pitfalls
### Unobserved Variables
### Complex Explanations
    - Occam’s Razor
### Paths of Diminishing Returns
## Stopping Criteria
## Documentation
