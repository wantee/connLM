# connLM Toolkit
connLM is a Connectionist Language Modelling Toolkit, implementing several neural network based language models.

[![Build Status](https://travis-ci.org/wantee/connLM.svg)](https://travis-ci.org/wantee/connLM)
[![License](http://img.shields.io/:license-mit-blue.svg)](https://github.com/wantee/connLM/blob/master/LICENSE)

## Features
### Models
* Recurrent Neural Network Language Model(RNN)[<sup> 1 </sup>](#rnn)
* Log-Bilinear Language Model(LBL)[<sup> 2 </sup>](#lbl)
* Feed-Forward Neural Network Language Model(FFNN)[<sup> 3 </sup>](#ffnn)
* (Hash-Based) Maximum Entropy Model(maxEnt)[<sup> 4 </sup>](#maxent)

### Training Algorithms
* Asynchronous SGD (HOGWILD!)[<sup> 5 </sup>](#hogwild)
* Hierarchical SoftMax[<sup> 6 </sup>](#hogwild)

## Benchmark

Main performance metrics of connLM is shown in following table. Details for testings can be found in the documentation.

```
+-----+-------------+------------+-------+---------------+---------------------------------+-------+
|     |             |            |       |               |           Entropy/PPL           |       |
+ No. +    Corpus   +    Model   + Algo. +   Time/Speed  +---------------------------------+ Iter. +
|     |             |            |       |               |  Train |  Valid |      Test     |       |
+-----+-------------+------------+-------+---------------+--------+--------+---------------+-------+
|  2  | librispeech | RNN+MaxEnt |  ASGD | 32hr9m9s/95k | 6.8693 | 7.1583 | 7.1008/137.27 |   13  |
+-----+-------------+------------+-------+---------------+--------+--------+---------------+-------+
```

## Usage
### Build

Run following commands to build this project:

```shell
$ git clone https://github.com/wantee/connLM.git
$ cd connLM/src
$ ./autogen.sh
$ make
```

Quick tests:

```shell
$ cd $CONNLM_HOME/src
$ make clean
$ make test
$ CONNLM_NO_OPT=1 make
$ cd ..
$ source tools/shutils/shutils.sh
$ cd egs/tiny
$ shu-testing
```

Normally the tests should be passed. However, there may be differences between the output and the expected, which may be caused by floating point operations. If the differences are small, this should not be a problem.

After tests, one should rebuild the binaries for speed up: `make clean && make`.

## License

**connLM** is open source software licensed under the MIT License. See the `LICENSE` file for more information.

## References
1. <a name="rnn"></a> Mikolov Tomas. "[Statistical Language Models based on Neural Networks](http://www.fit.vutbr.cz/~imikolov/rnnlm/thesis.pdf)." PhD thesis, Brno University of Technology, 2012.
2. <a name="lbl"></a> Mnih, Andriy, and Geoffrey Hinton. "[Three new graphical models for statistical language modelling](http://machinelearning.wustl.edu/mlpapers/paper_files/icml2007_MnihH07.pdf)." Proceedings of the 24th international conference on Machine learning. ACM, 2007.
3. <a name="ffnn"></a> Bengio, Yoshua, et al. "[A neural probabilistic language model](http://machinelearning.wustl.edu/mlpapers/paper_files/BengioDVJ03.pdf)." The Journal of Machine Learning Research 3 (2003): 1137-1155.
4. <a name="maxent"></a> Mikolov, Tomas, et al. "[Strategies for training large scale neural network language models](http://www.fit.vutbr.cz/~imikolov/rnnlm/asru_large_v4.pdf)." Automatic Speech Recognition and Understanding (ASRU), 2011 IEEE Workshop on. IEEE, 2011.
5. <a name="hogwild"></a> Recht, Benjamin, et al. "[Hogwild: A lock-free approach to parallelizing stochastic gradient descent](http://papers.nips.cc/paper/4390-hogwild-a-lock-free-approach-to-parallelizing-stochastic-gradient-descent.pdf)." Advances in Neural Information Processing Systems. 2011.
6. <a name="hs"></a> Mnih, Andriy, and Geoffrey E. Hinton. "[A scalable hierarchical distributed language model](https://www.cs.toronto.edu/~amnih/papers/hlbl_final.pdf)." Advances in neural information processing systems. 2009.

## Related Tools
1. [RNNLM Toolkit](http://rnnlm.org/)
2. [word2vec](https://code.google.com/p/word2vec/)
3. [NPLM Toolkit](http://nlg.isi.edu/software/nplm/)
4. [LBL4word2vec](https://github.com/qunluo/LBL4word2vec)
5. [neural_lm](https://github.com/ddahlmeier/neural_lm/)
