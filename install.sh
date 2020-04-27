#!/bin/bash
dir=$(pwd -P);
cmd=$(basename $dir);
read -p "Create symbolic link to $dir/bin/$cmd in /usr/local/bin/$cmd? " -n 1 -r REPLY;
echo;
if [[ $REPLY =~ ^[Yy]$ ]]
then
	ln -s $dir/bin/$cmd /usr/local/bin/$cmd;
fi

