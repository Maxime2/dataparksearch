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
 <tr><td colspan=3 align=center>URLs and referers</td></tr>
EOF

# EDIT - ME 

my $DBAddr = "pgsql://user:passwd\@localhost/search/?dbmode=cache";

# 

my $search = new Dataparksearch('DBAddr' => $DBAddr);
my $stats  = $search->GetReferers();

foreach my $line ( @$stats ){
  printf("  <tr><td>\%d</td><td>\%s</td><td>\%s</td></tr>\n", @$line)
}

print <<EOF;
</table>
</body>
</html>
EOF

# Free all allocations
$search->Free();

exit;

