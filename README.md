pg_plugins, set of plugin templates for PostgreSQL
==================================================

This repository contains a collection of plugins and extensions for
PostgreSQL that can be used as templates for more extensive work, like:

- Background workers
- Logical decoding plugins
- Custom logging formats
- Extensions
- Table access methods
- Micro-benchmarking functions.
- WAL reader.
- etc.

Installation
------------

Simply run the following command with pg_config in PATH:

    make install

All the sub-commands of PGXS are available as well.  Note that some modules
may fail to compile because of missing dependencies or a version of
PostgreSQL code incompatible with the module.  You may want to compile
individually modules if that is the case.  Each module is maintained to be
compatible with the latest version of PostgreSQL.

LICENSE
-------

pg_plugins is free software distributed under the PostgreSQL Licence.

Copyright (c) 1996-2026, PostgreSQL Global Development Group

Developer
---------

The modules stored in this repository have been written and are being
maintained by Michael Paquier, PostgreSQL committer.

Links
-----

- [Blog](http://paquier.xyz)
- [Bug Reports](https://github.com/michaelpq/pg_plugins/).
