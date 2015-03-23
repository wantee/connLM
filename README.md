# connLM Toolkit
connLM is a Connectionist Language Modelling Toolkit, implementing several neural network based language models.

## Features
### Models
* Recurrent Neural Network Language Model(RNN)[<sup> 1 </sup>](#rnn)
* Log-Bilinear Language Model(LBL)[<sup> 2 </sup>](#lbl)
* Feed-Forward Neural Network Language Model(FFNN)[<sup> 3 </sup>](#ffnn)
* (Hash-Based) Maximum Entropy Model(maxEnt)[<sup> 4 </sup>](#maxent)

### Training Algorithms
* Asynchronous SGD (HOGWILD!)[<sup> 5 </sup>](#hogwild)
* Hierarchical SoftMax[<sup> 6 </sup>](#hogwild)

## Usage
### Build
Get the `stutils` library from [GitHub](https://github.com/wantee/stutils), following the instruction to build the library and setting the proper environment variables to make your `gcc` can find them.

Then build the toolkit,

```shell
$ git clone https://github.com/wantee/connLM.git
$ cd connLM/src
$ make
```

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
