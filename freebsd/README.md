# FreeBSD kernel module

## Very simple kernel module, hello world 
- Simple module, only load and unload function

## Null Module
- null_module, create device /dev/null2

## Syscall kernel module
- Syscall_module, its implementation syscall

## Character kernel module
- Character_module, the most common type of device driver, 
  copy data from user space to kernel space, create device in /dev/echo

## Race kernel module 
- Race with fix race condition with uses mutex


```
 1. Checkout svn freebsd kernel svn checkout https://svn0.eu.freebsd.org/base/head /usr/src
 2. make
 3. Run sample # kldload -v ./module.ko
```
