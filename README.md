### TCP/IP network stack implementation
_Reference: https://cs144.stanford.edu_
This imp
#### Network stacks architecture
![Screenshot](https://github.com/TCP-IP-Protocol-Stack/image/01.png)

To set up the build system: `cmake -S . -B build`

To compile: `cmake --build build`

To run tests: `cmake --build build --target test`

To run speed benchmarks: `cmake --build build --target speed`

To run clang-tidy (which suggests improvements): `cmake --build build --target tidy`

To format code: `cmake --build build --target format`
