#!/bin/sh

while IFS=' ' read -r type args ; do
    set -- $args
    case $type in
        address)
            addr=$1 ; prefix_len=$2
            echo ip address add $addr/$prefix_len dev eth0
            ;;
        route)
            addr=$1 ; prefix_len=$2 ; next_hop=$3
            echo ip route add $addr/$prefix_len via $next_hop dev eth0
            ;;
        resolver)
            addr=$1
            echo echo nameserver $addr \>\> /etc/resolv.conf
            ;;
    esac
done
