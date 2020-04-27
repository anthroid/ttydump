#!/bin/bash
dir=$(pwd -P);
cmd=$(basename $dir);
read -p "Remove /usr/local/bin/$cmd? " -n 1 -r REPLY;
echo;
if [[ $REPLY =~ ^[Yy]$ ]]
then
	rm /usr/local/bin/$cmd;
fi

