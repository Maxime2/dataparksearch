#!/usr/bin/perl -w

use strict;
use Dataparksearch;

print "Content-type: text/html\n\n";
print <<EOF;
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN" "http://www.w3.org/TR/REC-html40/loose.dtd">

<html>
<head>
<title>Stats</title>
</head>
<body>
<table border=1>
 <tr><td colspan=4 align=center>Database statistics</td></tr>
 <tr><th>Status</th><th>Expired</th><th>Total</th><th>&nbsp;</th></tr>
EOF

# EDIT - ME 

my $DBAddr = "pgsql://user:passwd\@localhost/search/?dbmode=cache";

# 

my $search = new Dataparksearch('DBAddr' => $DBAddr);
my $stats  = $search->GetStatistics();

foreach my $line ( @$stats ){
  if ( $line->[0] ne 'Total' ){
    printf("  <tr><td>\%d</td><td>\%d</td><td>\%d</td><td>\%s</td></tr>\n", $line->[0], $line->[1], $line->[3], $line->[5])
  } else {
    printf("  <tr><td>\%s</td><td>\%d</td><td>\%d</td><td>\&nbsp;</td></tr>\n", 'Total', $line->[1], $line->[3])
  }
}

print <<EOF;
</table>
</body>
</html>
EOF

# Free allocations
$search->Free();

exit;

