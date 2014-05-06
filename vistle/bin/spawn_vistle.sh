#! /bin/bash

case $(hostname) in
   viscluster*)
      echo spawn_vistle.sh "$@"
      HOSTS=$(echo viscluster{50..60} viscluster{71..75}|sed -e 's/ /,/g')
      exec mpirun -np 16 -H ${HOSTS} "$@"
      ;;
   *)
      #echo mpirun "$@"
      #exec mpirun -np 1 "$@"
      echo EXEC: "$@"
      exec mpirun -np 2 "$@"
      #exec xterm -e gdb --args "$@"
      exec "$@"
      ;;
esac
