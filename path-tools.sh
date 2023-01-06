#!/bin/bash

# simple shell function wrappers to manipulate paths in
# the current environment using the "path" tool by David I. Bell
#
# Copyright (c) 2022 Hartmut Holzgraefe <hartmut@php.net>
# Permission is granted to use, distribute, or modify this source,
# provided that this copyright notice remains intact.

# source this from within your .profile or .bashrc 

# by default manipulates $PATH, but can also be used to
# e.g. manipulate the Java $CLASSPATH, e.g.:
#
# pathfront /opt/bin -> add /opt/bin to front of $PATH
# pathadd $HOME/my_classes CLASSPATH -> add to end of $CLASSPATH
#
# requires that the "path" utility can already be found
# within $PATH
# 
# source code for the "path" tool can be found here:
# http://members.canb.auug.org.au/~dbell/

# append directory to path if not already present
# "pathadd dir" -> append to $PATH
# "pathadd MY_PATH dir" -> append to $MY_PATH
function pathadd() 
{ 
  if [ $# = 2 ] 
  then 
    export $1=`path -var $1 $2`;
  else
    export PATH=`path -var PATH $1`; 
  fi
}

# prepend directory to path if not already present
# "pathadd dir" -> prepend to $PATH
# "pathadd MY_PATH dir" -> prepend to $MY_PATH
function pathfront()
{ 
  if [ $# = 2 ] 
  then 
    export $1=`path -var $1 -rb $2`;
  else
    export PATH=`path -var PATH -rb $1`; 
  fi
}

# append directory to path or move to end if already present
# "pathback dir" -> append to $PATH
# "pathback MY_PATH dir" -> append to $MY_PATH
function pathback()
{ 
  if [ $# = 2 ] 
  then 
    export $1=`path -var $1 -ra $2`;
  else
    export PATH=`path -var PATH -ra $1`; 
  fi
}

# remove directory from path
# "pathdel dir" -> remove from $PATH
# "pathdel MY_PATH dir" -> remove from $MY_PATH
function pathdel()
{ 
  if [ $# = 2 ] 
  then 
    export $1=`path -var $1 -r $2`;
  else
    export PATH=`path -var PATH -r $1`; 
  fi
}

# remove all relative and invalid pathes from PATH
# "pathclean dir" -> remove from $PATH
# "pathclean MY_PATH dir" -> remove from $MY_PATH
function pathclean()
{ 
  if [ $# = 1 ] 
  then 
    export $1=`path -var $1 -ri -rr`;
  else
    export PATH=`path -ri -rr`; 
  fi
}

# list path contents
# "pathlist" -> lists $PATH
# "pathlist MY_PATH" -> lists $MY_PATH
function pathlist()
{ 
  if [ $# = 1 ] 
  then 
    path -var $1 -l
  else
    path -l
  fi
}

# validate path, show errors if any
# "pathcheck" -> lists $PATH
# "pathcheck MY_PATH" -> lists $MY_PATH
function pathcheck()
{ 
  if [ $# = 1 ] 
  then 
    path -var $1 -ci -cr
  else
    path -ci -cr
  fi
}




