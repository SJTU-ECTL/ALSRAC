#!/bin/bash

sudo apt update
sudo apt install libreadline-dev
sudo apt install ctags
git clone git@github.com:berkeley-abc/abc.git

cd abc/
make libabc.a
cd -
