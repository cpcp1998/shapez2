# Shapez 2 all possible shapes

This project lists all the possible shapes in Shapez 2 by bruteforce.

## Requirements
1. A C++ compiler supporting C++23.
2. Enumerating all the shapes for Normal and Hard scenarios (4 layers, 4 parts per layer) takes ~32MB memory and ~1.5 minutes.
3. Enumerating all the shapes for Insane scenario (5 layers, 4 parts per layer) takes ~8GB memory and ~2 hours.

## Usage
1. Init submodule and build

2. Enumerate all the shapes and store in a binary file
```
$ ./search4 dump4.bin
...
# shapes: 349728289
# halves: 8148
# shapes whose halves aren't stable: 2002457
# quarters: 152
```

3. Check whether a shape can be made in the game
```
$ ./lookup4 dump4.bin P---P---:P-------:cRCu--Cu:--------
The shape is creatable
```

4. We can do similar things for 5 layers
```
$ ./search5 dump5.bin
...
# shapes: 26929624298
# halves: 67669
# shapes whose halves aren't stable: 251172538
# quarters: 476
$ ./lookup5 dump5.bin P-------:P---P---:P-------:cRCu--Cu:--------
The shape is not creatable
```
