#! /bin/bash

#echo mpirun "$@"
#exec mpirun -np 1 "$@"
echo EXEC: "$@"
#exec mpirun -np 2 "$@"
#exec xterm -e gdb --args "$@"
exec "$@"
