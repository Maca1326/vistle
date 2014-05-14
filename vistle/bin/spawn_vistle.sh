#! /bin/bash

#echo mpirun "$@"
#exec mpirun -np 1 "$@"
echo EXEC: "$@"
#exec xterm -e gdb --args "$@"
exec "$@"
