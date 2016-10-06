Linux/MacOS: [![Build Status](https://travis-ci.org/ctuning/ck-crowdnode.svg?branch=master)](https://travis-ci.org/ctuning/ck-crowdnode)
Windows: [![Windows Build status](https://ci.appveyor.com/api/projects/status/587t15ai8bocr7t4?svg=true)](https://ci.appveyor.com/project/gfursin/ck-crowdnode)

Collective Knowledge Node for experiment crowdsourcing (on Windows devices)
===========================================================================

Standalone, thin and portable server to let users participate in experiment
crowdsourcing via Collective Knowledge. It unifies remote execution on Windows
similar to Android ADB (experiment crowdsourcing via Linux and Android
platforms is already supported by CK). It can also be used to create farms
of machines for collaborative benchmarking and tuning (crowd-benchmarking).

Note that both server and client should run Windows. 

Project homepage: 
* http://cknowledge.org
* http://cTuning.org

License
=======
* Permissive 3-clause BSD license. (See `LICENSE.txt` for more details).

Status
======

Relatively stable - testing phase

Usage: server side
==================
On Windows:

1. Download the installer from [Appveyor](https://ci.appveyor.com/project/gfursin/ck-crowdnode/build/artifacts)

2. Install and start "CK crowd-node server".

3. Write down "[INFO for CK client]" - you will require this info to configure this target machine on a client.

Usage: client side
==================
Install [CK framework](http://github.com/ctuning/ck). 
If you have PIP, you can install it simply as following:

```
 $ pip install ck
```

Pull ck-autotuning repository (including dependencies):

```
 $ ck pull repo:ck-autotuning
```

Prepare local file with a secret key (see [INFO for CK client]),
for example in C:\secret-key.txt

Register target machine with ck-crowdnode-server via
(substitute ''my-remote-target'' with any other user-friendly name)

```
 $ ck add machine:my-remote-target
```

Select 4) CK: remote Windows machine accessed via CK crowd node.
Then 4) windows-64

Then enter hostname, port, path to public key (C:\secret-key.txt),
and full path to files on a target machine (all info is available
via [INFO for CK client] - we later plan to automate this process).

Now you can check that you machine is connected and online via

```
 $ ck show machine
```

or

```
 $ ck browse machine
```

Now you should be able to compile and run sample program using this target. 
You need to have Microsoft C compilers and Microsoft SDK installed 
(there is a free edition available). You can also download and install
[LLVM for Windows](http://llvm.org/releases/download.html), 
but remember that it also requires Visual C compiler and Microsoft SDK.

Try to compile susan benchmark (during first compilation, CK will attempt
to automatically detect installed compilers and SDK while asking
you extra questions, if needed):

```
 $ ck compile program:cbench-automotive-susan --speed --target=my-remote-target
```

Finally, you can try to run it:

```
 $ ck run program:cbench-automotive-susan --target=my-remote-target
```

If everything is configured correctly, this code will be executed several
times on a required target and execution time will be reported!

If you have any problems, questions or comments, do not hesitate to get in touch
with the CK community via [our public mailing list](https://groups.google.com/forum/#!forum/collective-knowledge open CK mailing list)!

Further details
===============
* https://github.com/ctuning/ck
* https://github.com/ctuning/ck/wiki/Publications
* https://github.com/ctuning/ck/wiki/Farms-of-CK-machines
