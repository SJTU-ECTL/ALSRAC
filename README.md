# ALSRAC: Approximate Logic Synthesis by Resubstitution with Approximate Care Set

## Reference
Initial conference version:
* Chang Meng, Weikang Qian, and Alan Mishchenko, "[ALSRAC: approximate logic synthesis by resubstitution with approximate care set](http://umji.sjtu.edu.cn/~wkqian/papers/Meng_Qian_Mishchenko_ALSRAC_Approximate_Logic_Synthesis_by_Resubstitution_with_Approximate_Care_Set.pdf)", in *Proceedings of the 2020 Design Automation Conference (DAC)*, 2020, pp. 187:1-187:6.

For the latest journal extension, please refer to the GitHub repository: [https://github.com/changmg/ResubALS](https://github.com/changmg/ResubALS).
Publication for the journal extension:
* Chang Meng, Alan Mishchenko, Weikang Qian, and Giovanni De Micheli,
"[Efficient resubstitution-based approximate logic synthesis](https://ieeexplore.ieee.org/abstract/document/10772636/)", in *IEEE Transactions on Computer-Aided Design of Integrated Circuits and Systems (TCAD)*, 2024.

## Requirements
Currently, ALSRAC only supports 64-bit machines, since the logic simulator uses 64-bit parallel simulation.

To compile [ABC](https://github.com/berkeley-abc/abc), you need:
- gcc (>=10.3.0)
- make (>=4.1)
- libreadline

To compile the whole project, you also need:
- libboost (>=1.71)
- ctags

Additionally, you can use the docker image at https://hub.docker.com/r/changmeng/als_min

## Getting Started
### Clone & Build ABC
Clone `ABC` into `abc/`.
```
git clone git@github.com:changmg/abc.git
```

Build a static library `libabc.a`.
```
cd abc/
make libabc.a
cd ..
```

### Clone & Build ESPRESSO
Clone `ESPRESSO` into `espresso/`.
```
git clone git@github.com:changmg/espresso.git
```

Build a static library `libespresso.a`.
```
cd espresso/
make lib
cd ..
```

### Build ALSRAC
Generate a executable program:
```
make
```
It will generate a program named `main`.

Use `./main -h` to get help.

The benchmarks are in the `BLIF` format in the folder `data/`.

The standard cell libraries are in the folder `data/library/`.

Example:
```
./main -i data/su/c880.blif -l data/library/mcnc.genlib -m er -o appntk/ -t 0 -f 64 -b 0.05
```

In this case, approximate circuits are saved into `appntk/` .
