#!/bin/sh

# Usage: % tag3.sh < test.in > test.out

# Change following DIR where you installed MXPOST

DIR=/usr/local/tools/MXPOST/mxpost
CLASSPATH=$DIR; export CLASSPATH

$DIR/mxpost $DIR/tagger.project
