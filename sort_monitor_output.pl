#!/usr/bin/perl -w

@lines = <STDIN>;

@sorted = sort compare @lines;

print @sorted;


sub compare {
  my ($left,$right)=($a,$b);
  my ($left_time, $right_time, $ret);

#  print STDERR "compare $left to $right:";

  $left =~/Time\(sec=(\d+),\s+usec=(\d+),/;
  $left_time=$1 + $2/1e6;  
  $right =~/Time\(sec=(\d+),\s+usec=(\d+),/;
  $right_time=$1 + $2/1e6;
  
  if ($left_time<$right_time) { 
    $ret=-1;
  } elsif ($left_time>$right_time) { 
    $ret=1;
  } else {
    $ret=0;
  }
#  print STDERR "$ret\n";
  return $ret;
} 
