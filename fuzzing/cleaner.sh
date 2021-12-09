#!/bin/bash

while [[ 1 ]]; do
	echo "Cleaning..."
	find . -maxdepth 1 -delete
	sleep 10
done
