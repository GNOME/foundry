Title: Overview

# Overview

Foundry is a [GNOME](https://www.gnome.org/) library and command line tool
to implement IDE functionality across a variety of packging and build systems,
language servers, linters, debuggers, simulators, devices, and more.

##  pkg-config name

To build a program that uses Foundry, you can use the following command to get
the cflags and libraries necessary to compile and link.

```sh
gcc hello.c `pkg-config --cflags --libs libfoundry-1` -o hello
```
