#PBS -N Debug04P
#PBS -A ERDCV00898NEW
#PBS -l select=1:ncpus=36:mpiprocs=02
#PBS -l walltime=0:02:00
#PBS -q debug
#PBS -o /dev/null
#PBS -e /dev/null
#PBS -r n
#PBS -V

#ulimit -c 0
cd /p/home/joshuaqc/dpa/project1
mpirun -np 2 ./project1 ./easy_sample.dat ./sol_easy.04 >& ./output/out.04p
