#PBS -N Work04P
#PBS -l nodes=1:ppn=4
#PBS -l walltime=0:20:00
#PBS -q q64p48h@raptor
#PBS -o /dev/null
#PBS -e /dev/null
#PBS -r n
#PBS -V
cd $PBS_O_WORKDIR
#ulimit -c 0
lamboot
mpirun -np 4 project1 hard_sample.dat sol_hard.04 >& results.04p
lamhalt
