# [≡](#contents) [Bargain.C++](#) [![Gitter](https://badges.gitter.im/per-framework/community.svg)](https://gitter.im/per-framework/community) [![Build Status](https://travis-ci.org/per-framework/bargain.cpp.svg?branch=v0)](https://travis-ci.org/per-framework/bargain.cpp) [![Code Coverage](https://img.shields.io/codecov/c/github/per-framework/bargain.cpp/v0.svg)](https://codecov.io/gh/per-framework/bargain.cpp/branch/v0)

A transactional locking implementation for C++. Based on basic ideas from

<dl>
<dt><a href="https://perso.telecom-paristech.fr/kuznetso/INF346/papers/tl2.pdf">Transactional Locking II</a></dt>
<dd>Dave Dice, Ori Shalev, and Nir Shavit</dd>
</dl>

this simple variation uses per-object locks and a monadic interface to build a
top-down stack allocated splay tree to maintain an access set and attempts locks
in address order.

See [`synopsis.hpp`](provides/include/bargain_v0/synopsis.hpp) for the API.
