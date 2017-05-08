# connLM Toolkit
connLM is a Connectionist Language Modelling Toolkit, with fast training speed and flexible model structure.

[![Build Status](https://travis-ci.org/wantee/connLM.svg?branch=master)](https://travis-ci.org/wantee/connLM)
[![License](http://img.shields.io/:license-mit-blue.svg)](https://github.com/wantee/connLM/blob/master/LICENSE)

## Features

* CPU-only, with reasonable speed
* Configurable neural network structure
* Tree-like output layer
* Asynchronous SGD training

## Benchmark

Main performance for som dataset are shown in following table. Detail results please see `RESULTS.txt` under the egs directories.

```
+-------------+------------+------------+---------------+----------+
|    Corpus   |   Model    | Parameters |   Time/Speed  | Test PPL |
+-------------+------------+------------+---------------+----------+
|    SWBD1    | RNN+MaxEnt | 200+4/128M |    -/400k     |     76   |
+-------------+------------+------------+---------------+----------+
|     PTB     | RNN+MaxEnt | 200+4/128M |    -/340k     |    132   |
+-------------+------------+------------+---------------+----------+
|   Tedlium   | RNN+MaxEnt | 400+4/2G   |  14hrs/47k    |    127   |
+-------------+------------+------------+---------------+----------+
| LibriSpeech | RNN+MaxEnt | 300+4/2G   |               |          |
+-------------+------------+------------+---------------+----------+
|  1-Billion  | RNN+MaxEnt |            |               |          |
+-------------+------------+------------+---------------+----------+
```

## Usage
### Build

Run following commands to build this project:

```shell
$ git clone https://github.com/wantee/connLM.git
$ cd connLM/src
$ ./autogen.sh
$ make -j 4
```

Quick tests:

```shell
$ cd $CONNLM_HOME
$ make -C src test
$ source tools/shutils/shutils.sh
$ cd egs/tiny
$ shu-testing
```

Normally the tests should be passed. However, there may be differences between the output and the expected, which may be caused by floating point operations. If the differences are small, this should not be a problem.

## License

**connLM** is open source software licensed under the MIT License. See the `LICENSE` file for more information.


## Contributing

1. Fork it ( https://github.com/wantee/connLM.git )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request
