#!/bin/bash
export K2_CPO_ADDRESS=tcp+k2rpc://0.0.0.0:9000
export K2_TSO_ADDRESS=tcp+k2rpc://0.0.0.0:13000

export CPODIR=/tmp/___cpo_dir
export EPS="tcp+k2rpc://0.0.0.0:10000 tcp+k2rpc://0.0.0.0:10001 tcp+k2rpc://0.0.0.0:10002 tcp+k2rpc://0.0.0.0:10003"
export PERSISTENCE=tcp+k2rpc://0.0.0.0:12001

rm -rf ${CPODIR}
export PATH=${PATH}:/usr/local/bin

# start CPO
cpo_main -c1 --tcp_endpoints ${K2_CPO_ADDRESS} --data_dir ${CPODIR} --reactor-backend epoll --prometheus_port 63000 --txn_heartbeat_deadline=2s --thread-affinity false --assignment_timeout=10s --nodepool_endpoints ${EPS} --tso_endpoints ${K2_TSO_ADDRESS} --persistence_endpoints ${PERSISTENCE} --heartbeat_interval=1s &
cpo_child_pid=$!

# start nodepool
nodepool -c4 --tcp_endpoints ${EPS} --k23si_persistence_endpoint ${PERSISTENCE} --prometheus_port 63001 -m16G --thread-affinity false &
nodepool_child_pid=$!

# start persistence
persistence -c1 --tcp_endpoints ${PERSISTENCE} --prometheus_port 63002 --thread-affinity false &
persistence_child_pid=$!

# start tso
tso -c1 --tcp_endpoints ${K2_TSO_ADDRESS} --reactor-backend epoll --prometheus_port 63003 --tso.error_bound=100us --thread-affinity false &
tso_child_pid=$!
