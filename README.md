# ALSRAC: Approximate Logic Synthesis by Resubstitution with Approximate Care Set

## Publication of ALSRAC
* Chang Meng, Weikang Qian, and Alan Mishchenko, "[ALSRAC: approximate logic synthesis by resubstitution with approximate care set](http://umji.sjtu.edu.cn/~wkqian/papers/Meng_Qian_Mishchenko_ALSRAC_Approximate_Logic_Synthesis_by_Resubstitution_with_Approximate_Care_Set.pdf)", in *Proceedings of the 2020 Design Automation Conference (DAC)*, 2020, pp. 187:1-187:6.

## Requirements of ALSRAC
1. Basic requirements:

   - gcc (>=10.3.0)

   - make (>=4.1)

   - libreadline

   - libboost (>=1.71)

*Note: ALSRAC only supports 64-bit machines, since the logic simulator uses 64-bit parallel simulation.*

2. Additionally, you can use the docker image at https://hub.docker.com/r/changmeng/als_min

## Download ALSRAC
```shell
git clone https://github.com/SJTU-ECTL/ALSRAC.git --recursive
```

*Note: ALSRAC relies on [abc](https://github.com/changmg/abc) and [espresso](https://github.com/changmg/espresso) as sub-modules. They will be automatically cloned along with the ALSRAC project if you use the **--recursive** option.*

## Build ALSRAC

```shell
mkdir build
cd build
cmake .. 
make
cd ..
```
A program named `alsrac.out` will be generated at the root directory of the ALSRAC project.

Use `./alsrac.out -h` to get help.

The original circuits (fully accurate) are in the [BLIF](https://www.cs.uic.edu/~jlillis/courses/cs594/spring05/blif.pdf) format in the folder `data/`.

The standard cell libraries are in the folder `data/library/`.

Example:
```
./alsrac.out -i data/su/c880.blif -l data/library/mcnc.genlib -m ER -o appntk/ -t 0 -f 64 -b 0.05
```

In this case, approximate circuits are saved into `appntk/` .
