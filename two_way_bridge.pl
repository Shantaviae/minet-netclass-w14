#!/usr/bin/perl -w

#
# Bridges local to remote network bidirectionally.  Intended to be 
# run on the local network.  Assumption is that bridge
# is in the locations noted below and must run as root
#

$localbridge="./bridge";
$remotebridge="./bridge";

$sleepsecs=30;
$device="eth0";

$#ARGV>=1 or die "usage: two_way_bridge.pl remote_account addresses+\n";

$account=shift;
foreach $arg (@ARGV) {
  chomp($arg);
  $addrs.=" ".$arg;
}

$cmd="(sleep $sleepsecs; export MINET_ETHERNETADDR=junk; $localbridge $device local $addrs ) | ssh $account \"(export MINET_ETHERNETADDR=junk; $remotebridge eth0 remote $addrs) | (sleep $sleepsecs; export MINET_ETHERNETADDR=junk; $localbridge $device local $addrs > /dev/null)\"";

print "Executing '$cmd'\nPlease type password within $sleepsecs seconds\n";

system $cmd;



