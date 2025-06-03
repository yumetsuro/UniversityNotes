#!/bin/bash
#SBATCH --partition=normal
#SBATCH -o slurm_output_scaling.log
#SBATCH -e slurm_error_scaling.log
#SBATCH --nodes=8
#SBATCH --ntasks=8   

# Comprehensive Strong and Weak Scaling Tests for MergeSort FF+MPI
echo "=== Starting Comprehensive Scaling Tests ==="
echo "Timestamp: $(date)"
echo "=============================================="

# =============================================================================
# STRONG SCALING TESTS - Fixed problem size, varying number of nodes
# Testing how performance scales when keeping workload constant
# =============================================================================

echo ""
echo "=== STRONG SCALING TESTS ==="
echo "Fixed problem size, increasing nodes"
echo ""

# Strong Scaling - Small Dataset (25M elements)
echo "--- Strong Scaling: 25M elements ---"
for N in 2 4 8; do
    echo "Strong scaling: 25M elements on $N nodes (threads=8)"
    srun --nodes=$N \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 25M -r 0 -t 8
done

# Strong Scaling - Medium Dataset (50M elements)
echo "--- Strong Scaling: 50M elements ---"
for N in 2 4 8; do
    echo "Strong scaling: 50M elements on $N nodes (threads=8)"
    srun --nodes=$N \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 50M -r 0 -t 8
done

# Strong Scaling - Large Dataset (100M elements)
echo "--- Strong Scaling: 100M elements ---"
for N in 2 4 8; do
    echo "Strong scaling: 100M elements on $N nodes (threads=8)"
    srun --nodes=$N \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 100M -r 0 -t 8
done

# =============================================================================
# WEAK SCALING TESTS - Problem size grows proportionally with nodes
# Testing how performance scales when workload per node remains constant
# =============================================================================

echo ""
echo "=== WEAK SCALING TESTS ==="
echo "Problem size scales with number of nodes"
echo ""

# Weak Scaling - Base workload: 12.5M elements per node
echo "--- Weak Scaling: 12.5M elements per node ---"
echo "Weak scaling: 25M elements on 2 nodes (12.5M per node, threads=8)"
srun --nodes=2 \
     --ntasks-per-node=1 \
     --time=00:05:00 \
     --mpi=pmix \
     ./mergesort_ff_mpi -s 25M -r 0 -t 8

echo "Weak scaling: 50M elements on 4 nodes (12.5M per node, threads=8)"
srun --nodes=4 \
     --ntasks-per-node=1 \
     --time=00:05:00 \
     --mpi=pmix \
     ./mergesort_ff_mpi -s 50M -r 0 -t 8

echo "Weak scaling: 100M elements on 8 nodes (12.5M per node, threads=8)"
srun --nodes=8 \
     --ntasks-per-node=1 \
     --time=00:05:00 \
     --mpi=pmix \
     ./mergesort_ff_mpi -s 100M -r 0 -t 8

# Weak Scaling - Base workload: 25M elements per node
echo "--- Weak Scaling: 25M elements per node ---"
echo "Weak scaling: 50M elements on 2 nodes (25M per node, threads=8)"
srun --nodes=2 \
     --ntasks-per-node=1 \
     --time=00:05:00 \
     --mpi=pmix \
     ./mergesort_ff_mpi -s 50M -r 0 -t 8

echo "Weak scaling: 100M elements on 4 nodes (25M per node, threads=8)"
srun --nodes=4 \
     --ntasks-per-node=1 \
     --time=00:05:00 \
     --mpi=pmix \
     ./mergesort_ff_mpi -s 100M -r 0 -t 8

# =============================================================================
# THREAD SCALING ANALYSIS - Testing different thread configurations
# =============================================================================

echo ""
echo "=== THREAD SCALING ANALYSIS ==="
echo "Testing different thread configurations"
echo ""

# Thread scaling on 2 nodes
echo "--- Thread Scaling: 2 nodes ---"
for T in 4 8 16; do
    echo "Thread scaling: 25M elements on 2 nodes (threads=$T)"
    srun --nodes=2 \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 25M -r 0 -t $T
done

# Thread scaling on 4 nodes
echo "--- Thread Scaling: 4 nodes ---"
for T in 4 8 16; do
    echo "Thread scaling: 50M elements on 4 nodes (threads=$T)"
    srun --nodes=4 \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 50M -r 0 -t $T
done

# Thread scaling on 8 nodes
echo "--- Thread Scaling: 8 nodes ---"
for T in 4 8 16; do
    echo "Thread scaling: 100M elements on 8 nodes (threads=$T)"
    srun --nodes=8 \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 100M -r 0 -t $T
done

# =============================================================================
# PAYLOAD ANALYSIS - Testing with different payload sizes
# =============================================================================

echo ""
echo "=== PAYLOAD ANALYSIS ==="
echo "Testing different payload sizes (load balancing and communication efficiency)"
echo ""

# Payload analysis on 2 nodes
echo "--- Payload Analysis: 2 nodes ---"
for R in 0 16 32; do
    echo "Payload test: 25M elements on 2 nodes (payload=$R, threads=8)"
    srun --nodes=2 \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 25M -r $R -t 8
done

# Payload analysis on 4 nodes
echo "--- Payload Analysis: 4 nodes ---"
for R in 0 16 32; do
    echo "Payload test: 50M elements on 4 nodes (payload=$R, threads=8)"
    srun --nodes=4 \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 50M -r $R -t 8
done

# Payload analysis on 8 nodes
echo "--- Payload Analysis: 8 nodes ---"
for R in 0 16 32; do
    echo "Payload test: 100M elements on 8 nodes (payload=$R, threads=8)"
    srun --nodes=8 \
         --ntasks-per-node=1 \
         --time=00:05:00 \
         --mpi=pmix \
         ./mergesort_ff_mpi -s 100M -r $R -t 8
done

echo ""
echo "=== ALL SCALING TESTS COMPLETED ==="
echo "Timestamp: $(date)"
echo "Check slurm_output_scaling.log for detailed results"
echo "=========================================="