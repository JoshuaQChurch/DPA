#PBS -N Debug16P
#PBS -l nodes=4:ppn=4
#PBS -l walltime=0:02:00
#PBS -q q64p48h@raptor
#PBS -o /dev/null
#PBS -e /dev/null
#PBS -r n
#PBS -V
cd $PBS_O_WORKDIR
#ulimit -c 0
mpirun -np 16 project1 easy_sample.dat sol_easy.16 >& out.16p
