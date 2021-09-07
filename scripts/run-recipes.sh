#!/bin/bash

HOLON_LIBS=${1}
BIN_PATH=${2}
LOG_PATH=${3}
NPEERS=${4}
RECIPE_FILE=${5}
APP_TYPE=${6}
LDFLAGS=${7}
CFLAGS=${8}
LIBRARY_PATH=${9}
GO_PATH=${10}

export ANSIBLE_LOOKUP_PLUGINS=$HOLON_LIBS
export PYTHONPATH=$HOLON_LIBS
export NIOVA_BIN_PATH=$BIN_PATH
export CGO_LDFLAGS=$LDFLAGS
export CGO_CFLAGS=$CFLAGS
export LD_LIBRARY_PATH=$LIBRARY_PATH
export PATH=$GO_PATH

while IFS= read -r line; do
   recipe_list+=("$line")
done <$RECIPE_FILE

for recipe in "${recipe_list[@]}"
do
   ansible-playbook -e 'srv_port=4000' -e npeers=$NPEERS -e dir_path=$LOG_PATH -e 'client_port=14000' -e recipe=$recipe -e 'backend_type=pumicedb' -e app_name=$APP_TYPE holon.yml
   if [ $? -ne 0 ]
   then
      echo "Recipe: $recipe failed"
      exit 1
   fi
   echo "Recipe: $recipe completed successfully!"
   rm -rf $LOG_PATH/*
done
