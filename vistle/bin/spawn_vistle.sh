#! /bin/bash

echo SPAWN "$@"

case $(hostname) in
   viscluster*)
      HOSTS=$(echo viscluster{50..60} viscluster{71..75}|sed -e 's/ /,/g')
      if [ "$MPISIZE" = "" ]; then
         MPISIZE=16
      fi
      exec mpirun -np ${MPISIZE} -hosts ${HOSTS} "$@"
      ;;
   *)
      #echo mpirun "$@"
      #exec mpirun -np 1 "$@"
      #echo EXEC: "$@"
      #exec mpirun -np 2 "$@"
      #exec xterm -e gdb --args "$@"
      exec "$@"
      ;;
esac
