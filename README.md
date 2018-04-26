# OTDB

"OTDB" is a program that utilizes libotfs and libhbuilder to represent a database of OpenTag Filesystems.  

OTDB has a POSIX pipe interface, and it uses the "file" protocol described in libhbuilder to move information into and out-of the database.

OTDB does not have a query feature at present, just load, store, synchronize.

## Building OTDB

The build process uses GNU build tools, namely `make`.  OTDB implements the HBuilder package management model.  The method for building and install OTDB into the HBuilder package manager is:

```
$ make all
$ make install
```

