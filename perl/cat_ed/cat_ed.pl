#!/usr/bin/perl -w
##
# cat_ep.pl based on cat_ed.php v.1.7 (C) M.Zakharov <maxime@sochi.net.ru>
# BugFixes & some additions : Sergey Kartashoff <gluke@biosys.net>
# $Author: maxime $
# $Revision: 1.1 $
# $Log: cat_ed.pl,v $
# Revision 1.1  2003/01/17 17:08:30  maxime
# cat_ed added into CVS
#
# Revision 1.4  2000-10-25 20:00:12+04  maxime
##
use CGI qw(:standard);
use DBI;
use strict;
use vars qw(
	$dbh $sth $PHP_SELF $DEBUG $path_base $sort
	$dbhost $dbtype $dbname $dbuser $dbpass
	$root_name $fill $path_base $images_path $res @row $res2 @row2
	$path $new_path $temp $query $i
	$sql_sort $cur_path $link $name $depth $bgcolor $old_name $islink $link_name $count
	$action $sel_path $old_path $sort $new_link $new_name $new_cat $new_path $rec_id $id
	);

require('config.pl');

# write this DBI connect string for your db
if ($dbtype eq 'psql') {
    $dbh = DBI->connect("DBI:Pg:dbname=$dbname;host=$dbhost", "$dbuser", "$dbpass")
    or die "connecting: $DBI::errstr\n";
} elsif ($dbtype eq 'mysql') {
    $dbh = DBI->connect("DBI:mysql:database=$dbname;host=$dbhost", "$dbuser", "$dbpass")
    or die "connecting: $DBI::errstr\n";
} elsif ($dbtype eq 'oracle') {
    $dbh = DBI->connect("DBI:Oracle:$dbname", "$dbuser", "$dbpass")
    or die "connecting: $DBI::errstr\n";
}

##

$action = param("action") || '';
$sel_path = param("sel_path") || '';
$old_path = param("old_path") || '';
$new_path = param("new_path") || '';
$id = param("id") || '';
$sort = param("sort") || '';
$new_link = param("new_link") || '';
$new_cat = trim(param('new_cat') || '');
$new_link = trim(param('new_link') || '');
$new_name = trim(param('new_name') || '');

$PHP_SELF = $ENV{'SCRIPT_NAME'};

print "Content-type: text/html\r\n";
print "\r\n";


print <<E1;
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
<html>
<head>
<style type="text/css">
BODY {font-family: arial, helvetica, sans-serif;}
</style>
</head>
<body bgcolor="#888888">
<script language="JavaScript">
function test(x,y) {
    		if (confirm("Sure?")) {window.location.href=x}
		else {window.location.href=y} 
	}
</script>
E1


# -----------------------------------------------------
#  PHP functions
# -----------------------------------------------------
sub _php_math_basetolong {
	my ($arg, $base) = @_;
	my ($mult, $num, $digit, $i, $c, $s);
	
	$mult = 1;
	$num = 0;
	
	if ( ($base < 2) || ($base > 36) ) {
		return 0;
	}
	
	$s = uc($arg);
	
	for ($i = length($s) - 1; $i >= 0; $i--) {
		$c = substr($s, $i, 1);
		$digit = index("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", $c);
		if (($digit < 0) || ($digit >= $base)) {
			next;
		}
		$num += $mult * $digit;
		$mult *= $base;
	}
	return $num;
}


sub _php_math_longtobase {
	my ($arg, $base) =@_;
	my ($result, $digit, $value);
	
	$value = $arg;
	
	
	if ($base < 2 || $base > 36) {
		return '';
	}
	
	$result = '';
	
	do {
		$digit = $value % $base;
		$result = substr("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ", $digit, 1) . $result;
		$value = int ($value / $base);
		} 
	while ($value > 0);
	
	return $result;
}	

sub base_convert {
	my ($num, $from_base, $to_base) = @_;
	
	return _php_math_longtobase(
		_php_math_basetolong($num, $from_base),
		$to_base
		);
}

sub urlencode {
	my ($s) = @_;
	$s =~ s/([^a-zA-Z0-9_\-.])/uc sprintf("%%%02x",ord($1))/eg;
	return $s;
};

sub urldecode {
	my ($s) = @_;
	$s =~ s/%([a-fA-F0-9][a-fA-F0-9])/pack("C", hex($1))/eg;
	return $s;
};


sub htmlspecialchars {
	my $arg = $_[0];
	my ($i, $r, $c, $o);
	
	$r = '';
	for($i = 0; $i < length($arg); $i++) {
		$c = substr($arg, $i, 1);
		$o = ord($c);
		if ($o == 38) { $r .= '&amp;'; }
		elsif ($o == 34) { $r .= '&quot;'; }
		elsif ($o == 39) { $r .= '&#039;'; }
		elsif ($o == 60) { $r .= '&lt;'; }
		elsif ($o == 62) { $r .= '&gt;'; }
		else { $r .= $c }
	}
	return $r;
}


# -----------------------------------------------------
#  print_error_local($msg)
# -----------------------------------------------------
sub print_error_local {
	my $msg = shift;
	
	print "<center><font color=\"#d00000\" size=\"+2\"><b>Error: </b></font><font size=\"+2\">$msg</font></center><br>\n";
	exit;
}


#<?                        
#require('config.inc');      
#require('db_func.inc');


# -----------------------------------------------------
#  get_new_path($path)
# -----------------------------------------------------
sub get_new_path {
	my $path = shift;
	my ($query, $res, @row, $temp);
	
	$query="SELECT path
	FROM categories
	WHERE path LIKE '$path"."__'"."
	AND path NOT LIKE '$path"."\@\@'
	ORDER BY path ASC";
	
	if($DEBUG) {print "$query<BR><HR>"};
	
	$res = $dbh->prepare($query);
	if (!$res->execute) {
		print_error_local("Query error:".$dbh->err);
	}
	
	if ($fill == 0) {
		$new_path='00';
		while ((@row)=$res->fetchrow) {
			$temp = uc(base_convert(substr($row[0],length($row[0])-2,2),$path_base,10));
			if ($new_path < $temp) {
				$new_path = $temp;
			}
		} # while
		$new_path = base_convert($new_path+1,10,$path_base);
	} else {
		$new_path=1;
		while ((@row)=$res->fetchrow) {
			$temp = uc(base_convert(substr($row[0],length($row[0])-2,2),$path_base,10));
			if ($temp > $new_path) {
				last;
			} else {
				$new_path++;
			}
		} # while
		$new_path = base_convert("$new_path",10,$path_base);
	} # if 
	
	$res->finish;
	
	if (length($new_path)==1) {
		$new_path = '0' . $new_path;
	}
	
	$new_path = uc($path.$new_path);
	return($new_path);
	
}

# -----------------------------------------------------
#  optimize_tree($path)
# -----------------------------------------------------
sub optimize_tree {
	my $path = shift;
	
	my ($query, $new_path, $res, @row, $res2, @row2, $temp_16,
		$new_path_16, $rec_id, $old_path, $new_path_to_insert);
	
	$query = "SELECT path FROM categories WHERE path LIKE '$path"."__'".
	" AND path NOT LIKE '$path"."\@\@' ORDER BY path";
	
	if($DEBUG) {
		print "$query<BR><HR>";
	}
	
	$res = $dbh->prepare($query);
	if (!$res->execute) {
		print_error_local("Query error:".$dbh->err);
	}
	
	$new_path = 1;
	
	while ((@row)=$res->fetchrow) {
		if (get_link_count($row[0])) {
			optimize_tree($row[0]);
		}
		
		$temp_16 = uc(substr($row[0],length($row[0])-2,2));
		$temp=base_convert($temp_16,$path_base,10);
		
		if ($new_path != $temp) {
			$new_path_16 = uc(base_convert($new_path,10,$path_base));
			
			if (length($new_path_16)==1) {
				$new_path_16 = '0' . $new_path_16;
			}
			if (length($temp_16)==1) {
				$temp_16 = '0' . $temp_16;
			}
			
			$query="SELECT rec_id, path FROM categories WHERE path LIKE '$path$temp_16%'";
			
			if($DEBUG) {
				print "$query<BR><HR>";
			}
			
			$res2 = $dbh->prepare($query);
			if(!$res2->execute) {
				print_error_local("Query error:".$dbh->err);
			}
			
			while((@row2)=$res2->fetchrow) {
				$rec_id=$row2[0];
				$old_path=$row2[1];
				
				
				
#				$new_path_to_insert=uc(eregi_replace("^$path$temp_16","$path$new_path_16",$old_path));
				$new_path_to_insert = $old_path;
#				my $eexpr = "$new_path_to_insert =~ s/^$path$temp_16/$path$new_path_16/i;";
#				eval ($eexpr);

			        $new_path_to_insert =~ s/^$path$temp_16/$path$new_path_16/i;
				$new_path_to_insert = uc($new_path_to_insert);
				
				$query="UPDATE categories SET path='$new_path_to_insert' WHERE rec_id=$rec_id";
				
				if($DEBUG) {print "$query<BR><HR>"};
				
				if (!($dbh->do($query))) {
					print_error_local("Query error:".$dbh->err);
				}
				
				$query="UPDATE categories SET link='$new_path_to_insert' WHERE link='$old_path'";
				
				if($DEBUG) {print "$query<BR><HR>"};
				
				if (!($dbh->do($query))) {
					print_error_local("Query error:".$dbh->err);
				}					
			}
			$res2->finish;
		}
		$new_path++;
	}
	$res->finish;
}


# -----------------------------------------------------
#  get_link_count($path)
# -----------------------------------------------------
sub get_link_count {
	my $path = shift;
	my ($query, $res2, @row2);
	
	$query="SELECT count(*) FROM categories WHERE path LIKE '$path"."__'";
	
	if($DEBUG) {print "327: $query<BR><HR>";}
	
	$res2 = $dbh->prepare($query);
	if (!($res2->execute)) {
		print_error_local("Query error:".$dbh->err);
	}
	
	@row2=$res2->fetchrow;
	$res2->finish;
	
	return($row2[0]);
}

# -----------------------------------------------------
#  get_link_name($link)
# -----------------------------------------------------
sub get_link_name {
	my $link = shift;
	my ($link_path, $link_name, $query, $res, @row);
	
	$link_path=$link;
	$link_name='';
	
	while ($link_path ne '') {
		$query="SELECT name FROM categories WHERE path='$link_path'";
		
		if($DEBUG) {print $query,"<BR><HR>";}
		
		$res = $dbh->prepare($query);
		if (!($res->execute)) {
			print_error_local("Query error:".$dbh->err);
		}
		
		@row=$res->fetchrow;
		$res->finish;
		
		if (!@row) {
			$link_path=substr($link_path,0,length($link_path)-2);
			last;
		}
		
		$link_name = " / $row[0]".$link_name;
		
		$link_path=substr($link_path,0,length($link_path)-2);
	}
	
	$query="SELECT name FROM categories WHERE path='' OR path IS NULL";
	
	if($DEBUG) {print "357: $query<BR><HR>";}
	
	$res = $dbh->prepare($query);
	if (!($res->execute)) {
		print_error_local("Query error:".$dbh->err);
	}
	
	@row=$res->fetchrow;
	$res->finish;
	
	if (@row) {
		$link_name = " / $row[0]".$link_name;
	} else {
		$link_name='';
	}
	
	return $link_name;
}

# -----------------------------------------------------
#  get_link_href_name($link)
# -----------------------------------------------------
sub get_link_href_name {
	my $link = $_[0];
	my ($link_path, $link_name, $query, $lres, @row, $nrow);
	
	$link_path=$link;
	$link_name='';
	
	while ($link_path ne '') {
		$query="SELECT name FROM categories WHERE path='$link_path'";
		
		if($DEBUG) {print $query,"<BR><HR>";}
		
		$lres = $dbh->prepare($query);
		if (!($lres->execute)) {
			print_error_local("Query error:".$dbh->err);
		}
		
		@row=$lres->fetchrow;
		$lres->finish;
		
		if (!@row) {
			$link_path=substr($link_path,0,length($link_path)-2);
			last;
		}
		
		$row[0]=htmlspecialchars($row[0]);
		$link_name = " / <a href=\"$PHP_SELF?sel_path=$link_path&sort=$sort\">$row[0]</a>".$link_name;
		
		$link_path=substr($link_path,0,length($link_path)-2);
	}
	
	$query="SELECT name FROM categories WHERE path='' OR path IS NULL";
	
	if($DEBUG) {
		print "412: $query <BR><HR>";
	}
	
	$lres = $dbh->prepare($query);
	if (!($lres->execute)) {
		print_error_local("Query error:".$dbh->err);
	}
	
	@row = $lres->fetchrow;
	$lres->finish;
	
	$nrow = @row;
	if ($nrow > 0) {
		$row[0]=htmlspecialchars($row[0]);	
		$link_name = " / <a href=\"$PHP_SELF?sel_path=&sort=$sort\">$row[0]</a>".$link_name;
	} else {
		$link_name='';
	}
	
	return $link_name;
}

# -----------------------------------------------------
#  trim - strip spaces
# -----------------------------------------------------
sub trim {
	my @out = @_;
	for (@out) {
		s/^\s+//;
		s/\s+$//;
	}
	return wantarray ? @out : $out[0];
}


# -----------------------------------------------------
#  M A I N
# -----------------------------------------------------

#initdb();

if (($path_base<9) || ($path_base>36)) {
	$path_base=16;
}

$sel_path = urldecode($sel_path);
$old_path = urldecode($old_path);

$sel_path=trim($sel_path);
$old_path=trim($old_path);
$new_path=trim($new_path);
$new_name=trim($new_name);

if ($sort eq '') {
	$sort='name';
}

if ($DEBUG) {
	print "action: $action<b><br>";
}

if ($action eq 'del') {
	$query="DELETE FROM categories
   	         WHERE path LIKE '$sel_path%'";
	
	if($DEBUG) {print $query,"<BR><HR>";}
	
	if (!($dbh->do($query))) {
		print_error_local("Query error:".$dbh->err);
	}
	
	$query="DELETE FROM categories
		WHERE link LIKE '$sel_path%'";
	
	if($DEBUG) {print $query,"<BR><HR>";}
	
	if (!($dbh->do($query))) {
		print_error_local("Query error:".$dbh->err);
	}
	
	$sel_path=substr($sel_path,0,length($sel_path)-2);
} elsif ($action eq 'del_link') {
	$query="DELETE FROM categories
   	         WHERE rec_id = $id";
	
	if($DEBUG) {print $query,"<BR><HR>";}
	
	if (!($dbh->do($query))) {
		print_error_local("Query error:".$dbh->err);
	}
	
	$sel_path=substr($sel_path,0,length($sel_path)-2);
} elsif ($action eq 'rename') {
	$query="UPDATE categories
		SET name='$new_name'
		WHERE path='$old_path'";
	
	if($DEBUG) {print $query,"<BR><HR>";}
	
	if (!($dbh->do($query))) {
		print_error_local("Query error:".$dbh->err);
	}
} elsif ($action eq 'rename_link') {
	$query="UPDATE categories
		SET name='$new_name'
		WHERE rec_id=$id";
	
	if($DEBUG) {print $query,"<BR><HR>";}
	
	if (!($dbh->do($query))) {
		print_error_local("Query error:".$dbh->err);
	}
} elsif ($action eq 'edit_link') {
	if ($new_path ne '') {
		$query="SELECT count(*)
	FROM categories
	WHERE path='$new_path'";
	} else {
		$query="SELECT count(*)
	FROM categories
	WHERE path='$new_path'
			OR path IS NULL";
	}
	if($DEBUG) {print $query,"<BR><HR>";}
	
	$res2 = $dbh->prepare($query);
	if (!($res2->execute)) {
		print_error_local("Query error:".$dbh->err);
	}
	
	@row2 = $res2->fetchrow;
	$res2->finish;
	
	if ($row2[0]) {
		$query="UPDATE categories
		SET link='$new_path'
		WHERE rec_id=$id";
		
		if ($DEBUG) {print $query,"<BR><HR>";}
		
		if (!($dbh->do($query))) {
			print_error_local("Query error:".$dbh->err);
		}
	} else {
		print "WARNING : INVALID LINK DESTINATION ENTERED. PLEASE RETRY.<br>\n";
	}
} elsif ($action eq 'optimize') {
	optimize_tree('');
} elsif ($action eq 'new_cat') {
	if ($DEBUG) {
		print "new_path: $new_path new_cat: $new_cat sel_path: $sel_path<hr>";
	}
	$query="SELECT count(*) FROM categories";
	
	if ($DEBUG) {print $query,"<BR><HR>";}
	
	$res2 = $dbh->prepare($query);
	if (!($res2->execute)) {
		print_error_local("Query error:".$dbh->err);
	}
	
	@row2 = $res2->fetchrow;
	if (!$row2[0]) {
		if (($dbtype eq 'oracle') || ($dbtype eq 'oracle7') || ($dbtype eq 'oracle8')) {
			$query="INSERT INTO categories (rec_id,path,link,name) VALUES ( SQ_category_id.nextval, '', '','$root_name')";
			
			if ($DEBUG) {print $query,"<BR><HR>";}
			
			if (!($dbh->do($query))) {
				print_error_local("Query error:".$dbh->err($res));
			}
		} else {
			$query="INSERT INTO categories (path,link,name) VALUES ( '', '','$root_name')";
			
			if ($DEBUG) {print $query,"<BR><HR>";}
			
			if (!($dbh->do($query))) {
				print_error_local("Query error:".$dbh->err($res));
			}
		}		       
	}
	
	$res2->finish;
	
	$new_path = get_new_path($sel_path);
	
	if (($dbtype eq 'oracle') || ($dbtype eq 'oracle7') || ($dbtype eq 'oracle8')) {
		$query="INSERT INTO categories (rec_id,path,link,name) VALUES ( SQ_category_id.nextval, '$new_path', '','$new_cat')";
	} else {
		$query="INSERT INTO categories (path,link,name) VALUES ( '$new_path', '','$new_cat')";
	}
	if ($DEBUG) {print $query,"<BR><HR>";}
	
	if (!($dbh->do($query))) {
		print_error_local("Query error:".$dbh->err($res));
	}
} elsif ($action eq 'new_link') {
	$query="SELECT count(*) FROM categories";
	
	if ($DEBUG) {print $query,"<BR><HR>";}
	
	$res2 = $dbh->prepare($query);
	if (!($res2->execute)) {
		print_error_local("Query error:".$dbh->err($res2));
	}
	
	@row2 = $res2->fetchrow;
	
	if (!$row2[0]) {
		if (($dbtype eq 'oracle') || ($dbtype eq 'oracle7') || ($dbtype eq 'oracle8')) {
			$query="INSERT INTO categories (rec_id,path,link,name)
    	       	        VALUES ( SQ_category_id.nextval, '', '','$root_name')";
		} else {
			$query="INSERT INTO categories (path,link,name)
			VALUES ( '', '','$root_name')";
		}		       
		if ($DEBUG) {print $query,"<BR><HR>";}
		
		if (!($dbh->do($query))) {
			print_error_local("Query error:".$dbh->err($res));
		}
	}
	
	$res2->finish;
	
	if (($dbtype eq 'oracle') || ($dbtype eq 'oracle7') || ($dbtype eq 'oracle8')) {
		$query="INSERT INTO categories (rec_id,path,link,name)
    	            VALUES ( SQ_category_id.nextval, '$sel_path\@\@','','$new_link')";
	} else {
		$query="INSERT INTO categories (path,link,name)
   	            VALUES ( '$sel_path\@\@','','$new_link')";
	}
	if ($DEBUG) {print $query,"<BR><HR>";}
	
	$res = $dbh->prepare($query);
	if (!$res->execute) {
		print_error_local("Query error:".$dbh->err);
	}
}

if ($sort eq 'path') {$sql_sort='ORDER BY path ASC,link ASC';}
elsif ($sort eq 'name') {$sql_sort='ORDER BY name ASC';}


$cur_path = get_link_href_name($sel_path);


$temp = substr($sel_path,0,length($sel_path)-2);

print <<E2;
<table border="1" cellspacing="0" cellpadding="3" width="100%">

<tr bgcolor="#dddddd">
	<td align="center"><a href="$PHP_SELF"><font size="+1"><b>&lt; Go to begin &gt;</b></font></td>
<td align="center"><a href="$PHP_SELF?action=optimize&sel_path=$sel_path&sort=$sort"><font size="+1"><b>&lt; Tree optimization (slow) &gt; </b></font></td>
<td align="center">&nbsp;</td>
</tr>

<form action="$PHP_SELF" method="post">

<tr bgcolor="#7070af">
	<td><font color="#FFFFFF"><b>New category</b></font></td>
<td><input type="text" size="40" name="new_cat">
<input type="hidden" name="sel_path" value="$sel_path">
<input type="hidden" name="sort" value="$sort">
<input type="hidden" name="action" value="new_cat">
</td>
<td>&nbsp;<input type="submit" value=" Insert ">&nbsp;</td>
</tr>

</form>

<form action="$PHP_SELF" method="post">
<tr bgcolor="#7070af">
	<td><font color="#FFFFFF"><b>New symlink</b></font></td>
<td><input type="text" size="40" name="new_link">
<input type="hidden" name="sel_path" value="$sel_path">
<input type="hidden" name="sort" value="$sort">
<input type="hidden" name="action" value="new_link">
</td>
<td>&nbsp;<input type="submit" value=" Insert ">&nbsp;</td>
</tr>

</form>
<td colspan="3" bgcolor="#dddddd">&nbsp;</td>
	</table>


<table border="0" cellspacing="1" cellpadding="2" width="100%">
<tr bgcolor="#c0d0ef">
	<td><b>&nbsp; $cur_path </b></td>
	<td align="right"><b>Sort by: [ &nbsp; </b> <a href="$PHP_SELF?sel_path=$sel_path&sort=name"><b>Name</b></a>&nbsp; <b>|</b> &nbsp;<a href="$PHP_SELF?sel_path=$sel_path&sort=path"><b>Path</b></a> &nbsp; <b> ] &nbsp;</b> </td>
	<td width="1%" align="center"><b>Path</b></td>
	<td width="1%" align="center"><b>Link path</b></td>
	<td width="1%" align="center"><b>Rename</b></td>
	<td width="1%" align="center"><b>Del</b></td>
	</tr>

<tr bgcolor="#ffffff">
	<td colspan="2">&nbsp;<img src="$images_path/folder.gif" border="0"> &nbsp; <a href="$PHP_SELF?sel_path=$temp&sort=$sort"><font size="+2"><b>..</b></font> (up one level)</a></td>
	<td>&nbsp;</td>
<td>&nbsp;</td>
	<td>&nbsp;</td>
<td>&nbsp;</td>
	</tr>

E2

$query="SELECT path, link, name, rec_id FROM categories WHERE path LIKE '$sel_path"."__'$sql_sort";

if ($DEBUG) {print $query,"<BR><HR>";}

$res = $dbh->prepare($query);
if (!($res->execute)) {
	print_error_local("Query error:".$dbh->err($res));
}

$i=1;
while(@row = $res->fetchrow) {
	$path=$row[0];
	$link=$row[1];
	$name=$row[2];
	$rec_id=$row[3];
	
	$depth=length($path);
	
	if ($i==0) {$bgcolor='#ffffff';}
	else {$bgcolor='#dddddd';}
	
	print "<tr bgcolor=\"$bgcolor\">";
	
	$old_name=$name;
	$name=htmlspecialchars($name);
	if (substr($path,$depth-2,2) eq '@@') {
		$islink=1;
		$name = '@ '.$name;
		$link_name=get_link_name($link);
		$name = $name."  > <a href=\"$PHP_SELF?sel_path=$link&sort=$sort\">$link_name</a>";
	} else {$islink=0;}
	
	$path = urlencode($path);
	
	$count = get_link_count($path);
	
	if ($count) {
		if ($islink) {
			print "<td colspan=2>&nbsp;<img src=$images_path"."folder.gif border=0> &nbsp; $name</td>\n";
		} else {
			print "<td colspan=2>&nbsp;<img src=$images_path"."folder.gif border=0> &nbsp; <a href=\"$PHP_SELF?sel_path=$path&sort=$sort\">$name</a></td>\n";
		}
	} else {
		if ($islink) {
			print "<td colspan=2>&nbsp;<img src=$images_path"."unknown.gif border=0> &nbsp; $name</td>\n";
		} else {
			print "<td colspan=2>&nbsp;<img src=$images_path"."unknown.gif border=0> &nbsp; <a href=\"$PHP_SELF?sel_path=$path&sort=$sort\">$name</a></td>\n";
		}
	}
	
	print '<td width="1%" align="center">'.urldecode($path)."</td>\n";
	
        if ($islink) {
        	print "<td><form action=$PHP_SELF method=post>".
                      "<input type=hidden name=action value=edit_link>".
                      "<input type=hidden name=sel_path value=\"$sel_path\">".
                      "<input type=hidden name=old_path value=\"$path\">".
                      "<input type=hidden name=id value=\"$rec_id\">".
                      "<input type=hidden name=sort value=\"$sort\">".
                      "<input type=submit value=\" Ok \">&nbsp;<input type=text size=10 name=new_path value=\"$link\"></td></form>\n";

                print "<td><form action=$PHP_SELF method=post>".
                   "<input type=hidden name=action value=rename_link>".
                   "<input type=hidden name=sel_path value=\"$sel_path\">".
                   "<input type=hidden name=old_path value=\"$path\">".
                   "<input type=hidden name=id value=\"$rec_id\">".
                   "<input type=hidden name=sort value=\"$sort\">".
                   "<input type=submit value=\" Ok \">&nbsp;<input type=text size=14 name=new_name value=\"$old_name\"></td></form>\n";

                print "<td align=center width=1%><form><input type=reset value=\"Del\" onClick=\"test('$PHP_SELF?action=del_link&sel_path=$path&id=$rec_id&sort=$sort', '$PHP_SELF?sel_path=$sel_path&sort=$sort')\"></td></form>";
        } else {
        	print "<td>&nbsp;</td>\n";

                print "<td><form action=$PHP_SELF method=post>".
                   "<input type=hidden name=action value=rename>".
                   "<input type=hidden name=sel_path value=\"$sel_path\">".
                   "<input type=hidden name=old_path value=\"$path\">".
                   "<input type=hidden name=sort value=\"$sort\">".
                   "<input type=submit value=\" Ok \">&nbsp;<input type=text size=14 name=new_name value=\"$old_name\"></td></form>\n";

                print "<td align=center width=1%><form><input type=reset value=\"Del\" onClick=\"test('$PHP_SELF?action=del&sel_path=$path&sort=$sort', '$PHP_SELF?sel_path=$sel_path&sort=$sort')\"></td></form>";
        }	
	
	print "</tr>\n";
	$i=1-$i;
}

$res->finish;
#?>

$dbh->disconnect;

print <<E3;
</table>


</body>
</html>
E3
