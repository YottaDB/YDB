#!/bin/bash

PROCS=$(nproc)
INST=$(expr $PROCS - 2)

mkdir env
cd env

echo "Creating leader..."
tmux new-session -d -s Leader "afl-fuzz -D -t 5000+ -i ../inputs -o ../output -M Leader -- ../build-instrumented/yottadb -dir"

for i in $(seq $INST); do
	tmux new-session -d -s Follower$i "afl-fuzz -D -t 5000+ -i ../inputs -o ../output -S Follower$i -- ../build-instrumented/yottadb -dir"
done

tmux new-session -d -s Cleaner "../cleaner.sh"
echo "Fuzzers are running..."
