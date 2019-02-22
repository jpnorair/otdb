# OTDB

"OTDB" is a program that utilizes libotfs and libhbuilder to represent a database of OpenTag Filesystems.  

OTDB has a command-line style interface that can work interactively, over a pipe, or via a socket.  The protocol is described in the OTDB API & Socket Protocol document that should be in the same directory as this ReadMe.

## Building OTDB

### Dependencies

OTDB has two kinds of dependencies

* Local Dependencies: **You only need to worry about these with source distributions of OTDB**
* External Dependencies: **If you receive a binary distribution, you still need to install talloc.**

#### External Dependencies

External Dependencies are packages of commony distributed libraries.  For licensing or practical reasons, they need to be installed separately from OTDB.

1. talloc

**Mac, via Homebrew:**

```
brew install talloc
```

**Ubuntu, via apt-get**:

```
sudo apt-get install libtalloc-dev
```

**Source Download (use 2.1.14 or later)**

https://www.samba.org/ftp/talloc/

#### Local Dependencies

Local Dependencies are libraries that are curated and distributed together with OTDB.  These libraries may be downloaded from the Git Repository system (e.g. Bitbucket Project Directory) that OTDB also exists inside.  Here is a list of these repositories you'll need to download.  They should be stored in the same parent directory as OTDB (just like the way they are arranged as repositories).

* argtable
* bintex
* cJSON
* cmdtab
* _hbsys
* hbuilder-lib
* libjudy
* libotfs
* m2def
* OTEAX

### Build Commands

The build process uses GNU build tools, namely `make`.  OTDB implements the HBuilder package management model.  The method for building and install OTDB into the HBuilder package manager is:

```
$ make deps
$ make all
$ make install
```

### Build Results

`make deps` will build all the requisite dependencies, `make all` will build OTDB, and `make install` will package OTDB into the \_hbpkg and \_hbsys directories.

**\_hbpkg** is where all headers, libraries, and binaries are copied, on a per-project organization model.

**\_hbsys** is what you should use during linking, including, and to put in your `$PATH`.  `_hbsys/${MACHINE_TYPE}/bin` can be put in your `$PATH` to simplify running OTDB (and other tools) from the command line.

## Running OTDB

### TODO

TODO