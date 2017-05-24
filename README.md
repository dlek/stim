# stim: Simple Task Information Manager

Stim is a simple application for tracking time spent on various tasks.  Stim records context switches and session stops and provides a reporting mechanism.  While a simple command-line utility, Stim can integrate with the user environment and desktop tools to provide a fairly useful time clock.

Stim records to a basic log file where records can be easily added, modified, or removed.  Reports can be generated for a particular time period with some flexibilty in reporting.  See `man stim` for an overview, although the man page needs to be cleaned up.

## How to build

[![Build Status](https://travis-ci.org/dlek/stim.svg?branch=master)](https://travis-ci.org/dlek/stim)

```autoconf && ./configure && make```
