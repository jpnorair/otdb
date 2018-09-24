# OTDB API & Socket Protocol

OTDB uses a socket protocol to communicate with clients.  For ease of integration with client code, OTDB also include a function-driven C API.  The API could be ported to other languages in the future (e.g. Python), but for now it is C only.

## API Functions

All API functions either take a handle as input or generate a handle. The "handle" is equivalent to an OTDB Client object.

### Client API Management Functions

Used for the purpose of creating and destroying an OTDB Client communication session.

* otdb_init()
* otdb_deinit()
* otdb_connect()
* otdb_disconnect()

#### otdb_init()

Creates and initializes a client socket to OTDB server.  This should be called before any other OTDB API functions.

```
void* otdb_init(sa_family_t socktype, const char* sockpath, size_t rxbuf_size);
```

* socktype: (sa\_family\_t) Use AF\_UNIX or AF\_INET
* sockpath: (const char*) path or address of OTDB server socket
* rxbuf_size: (size\_t) maximum size of protocol receive buffer
* returns: (void*) pointer to handle.  NULL on error.

A typical usage of rxbuf_size is 1024.  Protocol writes and reads beyond 512 bytes are not generally supported, but may be possible.

#### otdb_deinit()

Destroy and deinitialize the OTDB client socket.  This should be called after app is finished with OTDB.

```
void otdb_deinit(void* handle);
```

#### otdb_connect()

Connect the OTDB Client to the OTDB Server.  This function should only be used if your application is implementing the socket without the API communication functions.  If you are using the API communication functions, you should __not__ use otdb\_connect().

```
int otdb_connect(void* handle);
```

* handle: (void*) OTDB Handle as returned by otdb_init()
* returns: (int) zero on success.

#### otdb_disconnect()

Disconnect the OTDB Client from the OTDB Server.  This function should only be used if your application is implementing the socket without the API communication functions.  If you are using the API communication functions, you should __not__ use otdb\_disconnect().

```
int otdb_disconnect(void* handle);
```

* handle: (void*) OTDB Handle as returned by otdb_init()
* returns: (int) zero on success.


### Database File Functions

Database File Functions operate on the level of Database Files, which have a 1:1 relationship with the physical inverters.  Each inverter that's connected to a gateway is represented by a Database File.  Thus the Database is a collection of files, one per inverter.

Database File Functions are not necessarily REST-oriented, however they all are blocking calls with error code responses.

* otdb_newdevice()
* otdb_deldevice()
* otdb_setdevice()
* otdb_opendb()
* otdb_savedb()

#### otdb_newdevice()

Loads new device FS file in the database, based on JSON template input.  The JSON template format is a topic of its own.

If `tmpl_path` is NULL, then a new entry will be created for the default FS using the latest data.

If `device_id` is 0, the device ID(s) will be taken from the device FS. Situations where `device_id`=0 and `tmpl_path`=NULL can often fail, as the default ID(s) are likely to exist in the DB already.

```
int otdb_newdevice(void* handle, uint64_t device_id, const char* tmpl_path);
```

* handle: (void*) Handle to otdb client instance
* device\_id: (uint64\_t) Device ID for new device
* tmpl\_path: (const char*) Path to saved device FS template file
* returns: Returns 0 on success

#### otdb_deldevice()

Deletes a device FS file from the database.

```
int otdb_deldevice(void* handle, uint64_t device_id);
```

* handle: (void*) Handle to otdb client instance
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* Returns: 0 on success


#### otdb_setdevice()

Sets the active device ID.  This function is unecessary, but it can be used to improve performance in cases where a client will repeatedly access data from a single device in the database.

If **otdb\_setdevice()** is used, then all API functions using the `device_id` input parameter can have `device_id=0`.  These API calls will reference the last device to be set with **otdb\_setdevice()**.

```
int otdb_setdevice(void* handle, uint64_t device_id);
```

* handle: (void*) Handle to otdb client instance
* device_id: (uint64_t) 64 bit Device ID.
* returns: Returns 0 on success

#### otdb_opendb()

Opens a database file and loads into memory.  The `input_path` parameter points to a saved database.  The saved database is either a directory (which may contain further subdirectories), or a 7z-compressed archived of such a directory.

```
int otdb_opendb(void* handle, const char* input_path);
```

* handle: (void*) Handle to otdb client instance
* input\_path: (const char*) Path to saved database file 
* Returns: 0 on success


#### otdb_savedb()

Saves the active database to file.  The `output_path` input parameter specifies a directory structure which will contain the saved database files (JSON).  The `use_compression` input parameter, if true, will do 7z compression on the output directory.

```
int otdb_savedb(void* handle, bool use_compression, const char* output_path);
```

* handle: (void*) Handle to otdb client instance
* use\_compression: (bool) Enables compression of save file
* output\_path: (const char*) Path to save file
* Returns: 0 on success


### Database Editing Functions

Databased Editing Functions are CRUD (Create, Read, Update, Delete) oriented.  They are blocking calls, which return to the client after a response has been received from the Server (all calls have responses).

* otdb_delfile()
* otdb_newfile()
* otdb_restore()
* otdb_readperms()
* otdb_readhdr()
* otdb_read()
* otdb_readall()
* otdb_writeperms()
* otdb_write()



#### otdb_delfile()

Delete a file within a device.

```
int otdb_delfile(void* handle, uint64_t device_id, 
                 otdb_fblock_enum block, unsigned int file_id);
```

* handle: (void\*) Handle to otdb client instance
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* Returns 0 on success


#### otdb_newfile()

Create a new file within a device.

```
int otdb_newfile(void* handle, uint64_t device_id, 
                 otdb_fblock_enum block, unsigned int file_id,
                 unsigned int file_perms, unsigned int file_alloc);
```

* handle: (void\*) Handle to otdb client instance
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb_fblock_enum) File Block (usually BLOCK\_isf)
* file_id: (unsigned int) File ID
* file_perms: (unsigned int) New File Permissions
* file_alloc: (unsigned int) New File Allocation in Bytes
* Returns 0: on success


#### otdb_restore()

Restore a file on a device to its defaults
  
```
int otdb_restore(void* handle, uint64_t device_id, otdb_fblock_enum block, 
                 unsigned int file_id);
```

* handle: (void\*) Handle to otdb client instance
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* Returns: 0 on success


#### otdb_readperms()

Read permissions from a file

```
int otdb_readperms(void* handle, unsigned int* output_perms, 
                   uint64_t device_id, otdb_fblock_enum block, 
                   unsigned int file_id);
```

* handle: (void\*) Handle to otdb client instance
* output\_perms: (unsigned int\*) result parameter for read permissions
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* Returns: 0 on success


#### otdb_readhdr()

Read the header from a file on a device

```
int otdb_readhdr(void* handle, otdb_filehdr_t* output_hdr, uint64_t device_id, 
                 otdb_fblock_enum block, unsigned int file_id);
```

* handle: (void\*) Handle to otdb client instance
* output\_hdr: (otdb\_filehdr\_t\*) result parameter for read header
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* Returns 0: on success


#### otdb_read()

Read a file from a device

```
int otdb_read(void* handle, otdb_filedata_t* output_data, 
        uint64_t device_id, otdb_fblock_enum block, unsigned int file_id,
        unsigned int read_offset, int read_size);
```

* handle: (void\*) Handle to otdb client instance
* output\_data: (otdb\_filedata\_t\*) result parameter for read data
* device_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* read\_offset: (unsigned int) file byte offset to begin read
* read\_size: (int) number of bytes to read, from offset. -1 reads entire file.
* Returns 0 on success


#### otdb_readall()

Read a file and file headers from a device.

```
int otdb_readall(void* handle, otdb_filehdr_t* output_hdr, 
                 otdb_filedata_t* output_data, uint64_t device_id, 
                 otdb_fblock_enum block, unsigned int file_id,
                 unsigned int read_offset, int read_size);
```        

* handle: (void\*) Handle to otdb client instance
* output\_hdr: (otdb\_filehdr\_t\*) result parameter for read header
* output\_data: (otdb\_filedata\_t\*) result parameter for read data
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* read\_offset: (unsigned int) file byte offset to begin read
* read\_size: (int) number of bytes to read, from offset. -1 reads entire file.
* Returns 0 on success


#### otdb_writeperms()

Write permissions to a file on a device.

```
int otdb_writeperms(void* handle, uint64_t device_id, otdb_fblock_enum block, 
                    unsigned int file_id, unsigned int file_perms);
```

* handle: (void\*) Handle to otdb client instance
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* file\_perms: (unsigned int) New File Permissions
* Returns: 0 on success


#### otdb_write()

Write data to a file on a device.

```
int otdb_writedata(void* handle, uint64_t device_id, otdb_fblock_enum block, 
                   unsigned int file_id, unsigned int data_offset, 
                   unsigned int data_size, uint8_t* writedata);
```

* handle: (void\*) Handle to otdb client instance
* device\_id: (uint64\_t) 64 bit Device ID.  Use 0 to specify last-used Device.
* block: (otdb\_fblock\_enum) File Block (usually BLOCK\_isf)
* file\_id: (unsigned int) File ID
* data\_offset: (unsigned int) Byte offset into file
* data\_size: (unsigned int) size of data to write
* writedata: (uint8\_t\*) Bytewise data to write to file
* Returns: 0 on success



## Socket Protocol

The socket protocol is text-based and it follows the basic idea of shell command line inputs.  Binary data elements are represented as HEX.  All of the API functions map 1:1 to socket protocol commands.

### Database File Commands

* **dev-new**: used by otdb_newdevice()
* **dev-del**: used by otdb_deldevice()
* **dev-set**: used by otdb_setdevice()
* **open**: used by otdb_opendb()
* **save**: used by otdb_savedb()

#### dev-new

```
dev-new ID infile
```

ID: HEX input of device ID to add a new file for.

infile: Input file.  This is either a directory or a compressed archive of the directory.

#### dev-del

```
dev-del ID
```

ID: HEX input of device ID to delete file.

#### dev-set

```
dev-set ID
```

ID: HEX input of device ID to set as persistent Device.

#### open

```
open infile
```

infile: Input file.  This is either a directory or a compressed archive depending on the way it is saved (-c option or not).

#### save

```
save [-c] outfile
```

-c: Optional argument to compress output.  Compression is 7z type.

outfile: File name of the saved output. If compression is not used, the output will be a directory with this name, with an internal structure of subdirectories and JSON files.

### Database Editing Commands

Databased Editing Functions are CRUD (Create, Read, Update, Delete) oriented.  They are blocking calls, which return to the client after a response has been received from the Server (all calls have responses).

* **del**: used by otdb_delfile()
* **new**: used by otdb_newfile()
* **z**: used by otdb_restore()
* **rp**: used by otdb_readperms()
* **rh**: used by otdb_readhdr()
* **r**: used by otdb_read()
* **r\***: used by otdb_readall()
* **wp**: used by otdb_writeperms()
* **w**: used by otdb_write()

#### del (delete internal file)

```
del [-i ID] [-b block] file_id
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

file\_id: integer of internal file ID.

#### new (create internal file)

```
new [-i ID] [-b block] file_id perms alloc
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

file\_id: integer of internal file ID.

perms: Octal permission string, with two permission digits.  E.g. 66.

alloc: Integer. The maximum data contained by the file in bytes

#### z (restore internal file to defaults)

```
z [-i ID] [-b block] file_id
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

file\_id: integer of internal file ID.

#### rp (read permissions of internal file)

```
rp [-i ID] [-b block] file_id
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

file\_id: integer of internal file ID.

#### rh (read header of internal file)

```
rh [-i ID] [-b block] file_id
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

file\_id: integer of internal file ID.

#### r (read data from internal file)

```
r [-i ID] [-b block] [-r range] file_id
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

range: optional argument. A byte range such as `0:16` (first 16 bytes), `:16` (first 16 bytes), `8:` (all bytes after 7th), etc.  Defaults to `0:`

file\_id: integer of internal file ID.

#### r\* (read header and data from internal file)

```
r* [-i ID] [-b block] [-r range] file_id
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

range: optional argument. A byte range such as `0:16` (first 16 bytes), `:16` (first 16 bytes), `8:` (all bytes after 7th), etc.  Defaults to `0:`

file\_id: integer of internal file ID.

#### wp (write permissions to internal file)

```
wp [-i ID] [-b block] file_id perms
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

file\_id: integer of internal file ID.

perms: Octal permission string, with two permission digits.  E.g. 66.

#### w (write data to internal file)

```
w [-i ID] [-b block] [-r range] file_id writedata
```

ID: optional argument, specifies Device ID in HEX.  If unused, the Device ID from the last usage of `dev-set` will be used.

block: file block.  If unused, defaults to `isf`.

range: optional argument. A byte range such as `0:16` (first 16 bytes), `:16` (first 16 bytes), `8:` (all bytes after 7th), etc.  Defaults to `0:`  If range is supplied and if the writedata goes beyond the range, it will be clipped to fit within the range.

file\_id: integer of internal file ID.

writedata: a bintex string of the data to write to the internal file.  Bintex is a lightweight markup format.  The easiest way to create a bintex datastream is to use HEX encoded data with square brackets around it.  E.g. `[01020304AABBCCDD]`.

