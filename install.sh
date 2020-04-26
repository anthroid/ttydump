#!/bin/bash
dir=$(pwd -P);
cmd=$(basename $dir);
ln -s $dir/bin/$cmd /usr/local/bin/$cmd;

