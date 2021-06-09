#!/bin/bash
export LD_LIBRARY_PATH=/build/build/src/k2/connector/common/:/build/build/src/k2/connector/entities/
export K2PG_ENABLED_IN_POSTGRES=1
export K2PG_TRANSACTIONS_ENABLED=1
export K2PG_ALLOW_RUNNING_AS_ANY_USER=1
export K2_CONFIG_FILE=/build/pgtest/k2config_initdb.json

export GLOG_logtostderr=1 # log all to stderr
export GLOG_v=5 #
export GLOG_log_dir=/tmp

rm -rf pgroot/data
rm -rf /tmp/___cpo_dir
mkdir -p pgroot/data

./run_k2_platform.sh
sleep 5

/build/src/k2/postgres/bin/initdb -E UTF8 --locale=C -D pgroot/data
