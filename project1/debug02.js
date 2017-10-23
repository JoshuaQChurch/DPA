#PBS -N Debug02P
#PBS -A ERDCV00898NEW
#PBS -l select=1:ncpus=36:mpiprocs=02
#PBS -l walltime=0:00:20
#PBS -q debug
#PBS -o /dev/null
#PBS -e /dev/null
#PBS -r n
#PBS -V

#ulimit -c 0
cd /p/home/joshuaqc/dpa/project1
mpirun -np 2 ./project1 ./easy_sample.dat ./sol_easy.02 >& ./output/out.02p
