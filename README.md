# gdd_test

## Overview

This repo contains test tools for global deadlock detection of of PG14 and later.

Author: Koichi Suzuki

Organization: EnteririseDB

Copyright (c) 2021 EnterpriseDB

## Important directory

### `sql_functions`

Collection of C-based sql functions to drive and setup information.

This directory is dependent on original PostgreSQL source directory, at present, it is set to
`/hdd2/koichi/postgres-folk/postgres`, which is folked repository of community postgreSQL.

## Database itself

Test database (`pg14_gdd_database`) is not included in GIT repo.   See `.gitignore` for details.
You can build test database using shell scripts in this directory.

## Outline of Global Deadlock Detection

As mentioned in most of Transaction Management textbooks, Global Deadlock is a situation where transaction
clinches among more than one newwork nodes.   It is not specific to database and can occur in any distributed
applications.

For more information about implementation architecture, see [here](https://docs.google.com/presentation/d/1pf_BLeJ3h38EN-oHpy8ymgelRh7iPoq6R2WgttDN6DY/edit?usp=sharing)

## Shell scripts for the test environment

This directory contains the following shell scripts useful to build the test environment and run them.
The author understands that these scripts are written as the test progresses and there are still many things which need improvement for portability and flexibility.

### `build.sh`

Build necessary modules:

* PostgreSQL with global deadlock detection code.  This is currently based upon REL\_14\_0.
* Additional SQL functions to use GDD from libpq.
* Additional SQL test functions
* Global deadlock scenarion generator

See `build.sh` source for options.

clean.sh
conf.sh
deploy.sh
init.sh
start.sh
stop.sh

### Global deadlock scenario generator

