# ALSRAC: Approximate Logic Synthesis by Resubstitution with Approximate Care Set

## Requirements
To compile [ABC](https://github.com/berkeley-abc/abc), you need:
- gcc
- make
- libreadline

To compile the whole project, you also need:
- libboost
- ctags

## Getting Started
### Clone & Build ABC
Clone `ABC` into `abc/`.
```
git clone git@github.com:berkeley-abc/abc.git
```

Build a static library `libabc.a`.
```
make libabc.a
```

### Clone & Build ESPRESSO
Clone `ESPRESSO` into `espresso/`.
```
git clone git@github.com:dbzxysmc/espresso.git
```

Build a static library `libespresso.a`.
```
make libespresso.a
```

### Build ALSRAC
Generate a executable program:
```
make
```
It will generate a program named `main`.

Use `./main -h` to get help.

The benchmarks are in the `BLIF` format in the folder `data/`.

The standard cell libraries are in the `GENLIB` format in the folder `data/genlib/`.

Example:
```
./main -i data/su/c880.blif -l data/genlib/mcnc.genlib -m er -o appntk/ -t 0 -f 64 -b 0.05
```

In this case, approximate circuits are saved into `appntk/` .
