# gdd_test

## Overview

This repo contains test tools for global deadlock detection of of PG12 and later.

Author: Koichi Suzuki

Organization: 2ndQuadrant Ltd.,

Copyright (c) 2020 2ndQuadrant Ltd.,

## Important directory

### `sql_functions`

Collection of C-based sql functions to drive and setup information.

This directory is dependent on original PostgreSQL source directory, at present, it is set to
`/hdd2/koichi/postgres-folk/postgres`, which is folied repository of community postgreSQL.

## Outline of Global Deadlock Detection

As mentioned in most of Transaction Management textbooks, Global Deadlock is a situation where transaction
clinches among more than one newwork nodes.   It is not specific to database and can occur in any distributed
applications.

### Local deadlock scenario

### Local deadlock detection

### Global deadlock scenario

### Global deadlock detection
