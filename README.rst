#################
flat-network-data
#################

flat-network-data is intended to simplify network interface configuration via
Nova's `network_data.json`_.

.. _`network_data.json`: https://docs.openstack.org/nova/latest/user/metadata.html#openstack-format-metadata

Compared to heavier alternatives like cloud-init, flat-network-data is:

- **low footprint**; adding flat-network-data and all runtime dependencies to a
  root filesystem image increases image size by less than 64KB.

- **compositional**; flat-network-data is very singular in purpose: read
  network_data.json from stdin, and write a tool-agnostic "flattened"
  translation on stdout.

- **easy to parse**; contrast to network_data.json, flat-network-data output is
  trivially usable from shell scripts (the primary use-case)

Example usage:

.. code:: bash

   ./flat-network-data < examples/network_data.json

stdout:

.. code:: text

   route 10.0.0.0 8 11.0.0.1
   route 0.0.0.0 0 23.253.157.1
   address 10.184.0.244 20
   route :: 0 fd00::1
   route :: 48 fd00::1:1
   address 2001:cdba::3257:9652 56
   resolver 8.8.8.8
   resolver 8.8.4.4

As a more complete example, when composed with ``examples/linux-iproute2.sh``
as:

.. code:: bash

   ./flat-network-data < examples/network_data.json
     | sort \
     | sh examples/linux-iproute2.sh

stdout:

.. code:: text

   ip address add 10.184.0.244/20 dev eth0
   ip address add 2001:cdba::3257:9652/56 dev eth0
   echo nameserver 8.8.4.4 >> /etc/resolv.conf
   echo nameserver 8.8.8.8 >> /etc/resolv.conf
   ip route add 0.0.0.0/0 via 23.253.157.1 dev eth0
   ip route add ::/0 via fd00::1 dev eth0
   ip route add 10.0.0.0/8 via 11.0.0.1 dev eth0
   ip route add ::/48 via fd00::1:1 dev eth0

Output format
=============

flat-network-data produces one "unit of configuration" per newline, with
space-separated fields. The first field of each line always identifies the type
of the following fields.

Types:

- ``address <address> <prefix length>``

- ``route <address> <prefix length> <next hop>``

- ``resolver <address>``

Why not use jq, or other generic json transformation tools?
===========================================================

Why not indeed.

The main issue is prefix length calculation. network_data.json schema designers
made the unfortunate decision to provide prefix lengths as address masks, while
common tools like iproute2 expect/require the former.

Doing this translation from a shell script context would be ridiculously
complicated and fragile at best. flat-network-data's main value is in providing
this critical translation robustly and simply via POSIX's ``inet_pton`` and
gcc's ``__builtin_popcount*``.
