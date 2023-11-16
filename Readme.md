# libdie - a dice-calculation library.

This library parses a string in a format of a calculator, but additionally allows "dice" to be written in place of a number in a form common to TTRPGs: `<repeats>d<sides>`.

So an expression (referred to here as _dice-expression_) can be for example `3d10+5` which would mean, roll a 10-sided die 3 times and add 5.

This library was written for dic.

See `example/example.c` for usage, and the header `libdie.h` for more detail.

## License

This library is licensed under GPLv3.
