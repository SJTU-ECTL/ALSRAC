# Circuit
This is a personal C++ wrapper of [ABC](https://people.eecs.berkeley.edu/~alanmi/abc/),
A System for Sequential Synthesis and Verification.

## Requirements
To compile [ABC](https://github.com/berkeley-abc/abc), you need:
- gcc
- make
- libreadline

To compile the `circuit` project, further need:
- libboost
- ctags

## Getting Started
### Build ABC
In Ubuntu, you can simply run:
```
sudo chmod +x configure.sh
```
```
sh configure.sh
```
It will automatically clone `ABC` to `./abc/`,
compile it and generate a static library.

### Build Circuit
To generate a executable program, just use:
```
make
```
And it will generate a program named `main`.

Use `./main -?` to get help.
- usage: ./main [options] ... 
- options:
-   -f, --file      Original Circuit file (string [=data/sop/c880.blif])
-   -a, --approx    Approxiamte Circuit file (string [=])
-   -g, --genlib    Map libarary file (string [=data/genlib/mcnc.genlib])
-   -n, --nFrame    Frame nFrame (int [=10240])
-   -?, --help      print this message
The benchmarks are saved in `BLIF` format in the folder `./data/map/` and `.data/sop/`,
which are circuit in gate netlist format and SOP format, respectively.

The standard cell libraries are saved in `GENLIB` format in the folder `./data/genlib/`.

You can change the error rate for approximate computing with the argument `-e`,
and set the simulation frame number with the argument `-n`.

In `approx`, there are some approximate circuits. To measure the error rate, you can run like this:

```
./main -f data/sop/alu4.blif -a approx/alu4_0.0475.blif -n 10000
```
