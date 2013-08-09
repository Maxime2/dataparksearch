#!/usr/bin/perl

use Cwd 'abs_path';

$prefix="/usr/local/dpsearch";

$builtin='no';
$auto='yes';
$db='';
$db_string='';

$build_shared='yes';
$build_static='no';

$syslog='yes';
$syslog_facility='default';

$pthreads='yes';
$parser='yes';
$mp3='yes';
$dmalloc='no';
$zlib='yes';
$aspell='no';
$extra_charsets='no';

$file='yes';
$http='yes';
$ftp='yes';
$news='yes';
$ssl='no';
$ssl_dir='';

$conf_file='install.options';

$|=1;

print "DataparkSeach installation script version 0.4\n\n";

set_layout();
set_db(); print "\n";
set_libs(); print "\n";
set_syslog(); print "\n";
set_shema(); print "\n";
set_other(); print "\n";

configure();

# ---------------------------
# configure
# ---------------------------
sub configure {
	push @arg,"./configure";
	push @arg,"--prefix=$prefix";
        push @arg,"--bindir=$bindir";
	push @arg,"--sbindir=$sbindir";
        push @arg,"--sysconfdir=$sysconfdir";
        push @arg,"--localstatedir=$localstatedir";
        push @arg,"--libdir=$libdir";
	push @arg,"--includedir=$includedir";
        push @arg,"--mandir=$mandir";

        if ($build_shared eq 'yes') {
        	push @arg,"--enable-shared";
        } elsif ($build_shared eq 'no') {
        	push @arg,"--disable-shared";
	}

        if ($build_static eq 'yes') {
        	push @arg,"--enable-static";
        } elsif ($build_shared eq 'no') {
        	push @arg,"--disable-static";
	}


        if ($syslog eq 'yes') {
        	if ($syslog_facility ne 'default') {
        		push @arg,"--enable-syslog=$syslog_facility";
        	} else {
        		push @arg,"--enable-syslog";
        	}
        } elsif ($syslog eq 'no') {
        	push @arg,"--disable-syslog";
	}

        if ($ssl eq 'yes') {
        	if ($ssl_dir ne '') {
        		push @arg,"--with-openssl=$ssl_dir";
        	} else {
        		push @arg,"--with-openssl";
        	}
        }

        if ($pthreads eq 'yes') {
        	push @arg,"--enable-pthreads";
        } elsif ($pthreads eq 'no') {
        	push @arg,"--disable-pthreads";
	}

        if ($dmalloc eq 'yes') {
        	push @arg,"--enable-dmalloc";
        } elsif ($pthreads eq 'no') {
        	push @arg,"--disable-dmalloc";
	}

        if ($parser eq 'yes') {
        	push @arg,"--enable-parser";
        } elsif ($parser eq 'no') {
        	push @arg,"--disable-parser";
	}

        if ($mp3 eq 'yes') {
        	push @arg,"--enable-mp3";
        } elsif ($mp3 eq 'no') {
        	push @arg,"--disable-mp3";
	}

        if ($zlib eq 'no') {
        	push @arg,"--without-zlib";
        }

        if ($aspell eq 'no') {
        	push @arg,"--without-aspell";
        }

        if ($extra_charsets eq 'yes') {
                push @arg,"--with-extra-charsets=all";
        }

        if ($file eq 'yes') {
        	push @arg,"--enable-file";
        } elsif ($file eq 'no') {
        	push @arg,"--disable-file";
	}

        if ($http eq 'yes') {
        	push @arg,"--enable-http";
        } elsif ($http eq 'no') {
        	push @arg,"--disable-http";
	}

        if ($ftp eq 'yes') {
        	push @arg,"--enable-ftp";
        } elsif ($ftp eq 'no') {
        	push @arg,"--disable-ftp";
	}

        if ($news eq 'yes') {
        	push @arg,"--enable-news";
        } elsif ($news eq 'no') {
        	push @arg,"--disable-news";
	}

	$db_string=~s/^\s+//;
	$db_string=~s/\s+$//;
	@db_string=split(/\s+/,$db_string);
	
        push @arg,@db_string;

        print "@arg\n\n";
	
	open(CONF,">$conf_file") || die "Cannot create $conf_file\n";
	foreach $arg (@arg) {
		print CONF "$arg ";
	}
	print CONF "\n";
	close(CONF);	
	chmod(0755,$conf_file);
	
	system(@arg) == 0 or die "configure failed: $?";
	
	print "DataparkSearch configured successfully\n";
	print "Now you should run 'make' then 'make install' to finish installation.\n";
	print "To configure package with the same options you can run ./$conf_file\n\n";
}

# ---------------------------
# set_db
# ---------------------------
sub set_db {
	print "Database settings\n";
	print "-----------------\n";
	while () {
		print "Try to autodetect databases at known locations? (yes/no) [$auto]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $auto)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$auto=$temp;
			last;
		}
	}
	
	if ($auto eq 'yes') {
		print "\n";
		if ((-f "/usr/local/mysql/include/mysql/mysql.h") ||
		    (-f "/usr/include/mysql/mysql.h") ||
		    (-f "/usr/local/include/mysql/mysql.h") ||
		    (-f "/usr/include/mysql.h") ||
		    (-f "/usr/local/include/mysql.h")) {
		    	push @db_found2,1;
			$db{mysql}=1;
			print "MySQL server found\n";
		}
		
		if ((-f "/usr/include/pgsql/libpq-fe.h") ||
		         (-f "/usr/lib/libpq.a") ||
			 (-f "/usr/local/pgsql/include/libpq-fe.h") ||
		         (-f "/usr/local/pgsql/lib/libpq.a") ||
			 (-f "/usr/local/include/libpq-fe.h") ||
		         (-f "/usr/local/lib/libpq.a")) {
			push @db_found2,2;
			$db{pgsql}=1;
			print "PostgreSQL server found\n";
		}
		
		if ((-f "/usr/local/Hughes/include/msql.h") ||		
		         (-f "/usr/Hughes/include/msql.h") ||
			 (-f "/usr/local/include/msql.h") ||		
			 (-f "/usr/include/msql.h")) {
			push @db_found2,3;
			$db{msql}=1;
			print "mSQL server found\n";
		}
		
		if (-f "/opt/sybase/include/ctpublic.h") {
			push @db_found2,4;
			$db{ctlib}=1;
			print "Ct-Lib stuff found\n";
		}
		
		if (-f "/usr/local/include/ctpublic.h") {
			push @db_found2,5;
			$db{freetds}=1;
			print "FreeTDS Ct-Lib stuff found\n";
		}
		
		if ((-f "/usr/interbase/include/ibase.h") ||
			 (-f "/usr/local/interbase/include/ibase.h")) {
			push @db_found2,6;
			$db{ibase}=1;
			print "Interbase server found\n";
		}
		
		if (-f "$ENV{ORA_HOME}/rdbms/demo/ocidfn.h") {
			push @db_found2,7;
			$db{ora7}=1;
			print "Oracle 7.x server found\n";
		}
		
		if (-f "/oracle8/app/oracle/product/8.0.5/rdbms/demo/oci.h") {
			push @db_found2,8;
			$db{ora8}=1;
			print "Oracle 8.0.x server found\n";
		}
		
		if (-f "$ENV{ORA_HOME}/rdbms/demo/oci.h") {
			push @db_found2,9;
			$db{ora8i}=1;
			print "Oracle 8.x server found\n";
		}
		
		if ((-f "/usr/local/include/cli0cli.h") ||
		         (-f "/usr/include/cli0cli.h")) {
			push @db_found3,1; 
			print "Solid server found\n";			
		}
		
		if ((-f "/usr/sapdb/incl/sql.h") ||
			 (-f "/usr/local/sapdb/incl/sql.h")) {
			push @db_found3,2;
			print "SAPDB server found\n";			
		}
		
		if ((-f "/usr/local/include/isql.h") ||
			 (-f "/usr/include/isql.h")) {
			push @db_found,1;
			print "iODBC stuff found\n";
		}
		
		if ((-f "/usr/local/include/sql.h") ||
			 (-f "/usr/include/sql.h")) {
			push @db_found,2;
			print "unixODBC stuff found\n";
		}
		
		if ((-f "/usr/local/virtuoso-ent/odbcsdk/include/isql.h") ||
			 (-f "/usr/virtuoso-ent/odbcsdk/include/isql.h") ||
			 (-f "/usr/local/virtuoso-lite/odbcsdk/include/isql.h") ||
			 (-f "/usr/virtuoso-lite/odbcsdk/include/isql.h") ||
			 (-f "/usr/local/virtuoso/odbcsdk/include/isql.h") ||
			 (-f "/usr/virtuoso/odbcsdk/include/isql.h")) {
			push @db_found,3;
			print "OpenLink stuff found\n";
		}
		
		if ((-f "/usr/local/easysoft/oob/client/include/sql.h") ||
		         (-f "/usr/easysoft/oob/client/include/sql.h")) {
			push @db_found,4;
			print "EasySoft stuff found\n";
		} 

		if ((-f "/usr/local/include/sqlcli1.h") ||
		    (-f "/home/db2/include/sqlcli1.h")) {			 
			push @db_found,5;
			print "IBM DB2 stuff found\n";
		} 
		
		$db_found=@db_found;
		$db_found2=@db_found2;
		$db_found3=@db_found3;
		if (($db_found == 0) && ($db_found2 == 0) && ($db_found3 == 0)) {
			print "Sorry, no SQL servers at known locations was found\n";
		}
	}
	
	while () {
		print "\nWhich ODBC-style database support to include?\n";
		print "Note, that you may choose only one from the following.\n";
		
		print "  1 - Include iODBC support.\n";
		print "  2 - Include unixODBC support.\n";
		print "  3 - Include OpenLink ODBC support.\n";
		print "  4 - Include EasySoft ODBC support.\n";		
		print "  5 - Include IBM DB2 support.\n";		
		print "  6 - None of these.\n";		
		
		if ($db_found == 0) {
			$db='6';
		} else {
			$db=$db_found[0];
		}
		print "\nChoose one from the mentioned ($db): ";
		
		$temp=read_in();
		if ($temp eq '') {
			$temp=$db;
		}
		
		if ($temp eq '1') {
			if ($temp eq $db) {
				print "Enter iODBC base install directory [autodetect]: ";
			} else {
				print "Enter iODBC base install directory [/usr/local]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-iodbc "; } 
			else { $db_string .= " --with-iodbc=$temp "; }
			
			last;
		} elsif ($temp eq '2') {
			if ($temp eq $db) {
				print "Enter unixODBC base install directory [autodetect]: ";
			} else {
				print "Enter unixODBC base install directory [/usr/local]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-unixODBC "; } 
			else { $db_string .= " --with-unixODBC=$temp "; }
			
			last;
		} elsif ($temp eq '3') {
			if ($temp eq $db) {
				print "Enter OpenLink ODBC base install directory [autodetect]: ";
			} else {
				print "Enter OpenLink ODBC base install directory [/usr/local/virtuoso/odbcsdk]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-openlink "; } 
			else { $db_string .= " --with-openlink=$temp "; }
			
			last;
		} elsif ($temp eq '4') {
			if ($temp eq $db) {
				print "Enter EasySoft ODBC base install directory [autodetect]: ";
			} else {
				print "Enter EasySoft ODBC base install directory [/usr/local/easysoft/oob/client]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-easysoft "; } 
			else { $db_string .= " --with-easysoft=$temp "; }
			
			last;
		} elsif ($temp eq '5') {
			if ($temp eq $db) {
				print "Enter IBM DB2 base install directory [autodetect]: ";
			} else {
				print "Enter IBM DB2 base install directory [/home/db2]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-db2 "; } 
			else { $db_string .= " --with-db2=$temp "; }
			
			last;
		} elsif ($temp eq '6') {	
		    last;
		}
	}


	while () {
		print "\nWhich database support to include?\n";
		print "Note, that you can choose only one from these.\n";
		
		print "  1 - Include Solid support.\n";		
		print "  2 - Include SAPDB support.\n";
		print "  3 - None of these.\n";		
		
		if ($db_found3 == 0) {
			$db='3';
		} else {
			$db=$db_found3[0];
		}
		
		print "\nChoose one from the mentioned ($db):";
		
		$temp=read_in();
		if ($temp eq '') {
			$temp=$db;
		}
		
		if ($temp eq '1') {
			if ($temp eq $db) {
				print "Enter Solid base install directory [autodetect]: ";
			} else {
				print "Enter Solid base install directory [/usr/local]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-solid "; } 
			else { $db_string .= " --with-solid=$temp "; }
			
			last;
		} elsif ($temp eq '2') {
			if ($temp eq $db) {
				print "Enter SAPDB base install directory [autodetect]: ";				
			} else {
				print "Enter SAPDB base install directory [/usr/sapdb]: ";
			}
			
			$temp=read_in_nolc();
			if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-sapdb "; } 
			else { $db_string .= " --with-sapdb=$temp "; }
			
			last;
		} elsif ($temp eq '3') {	
		    last;
		}
	}

	
        if ($db{mysql}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include MySQL support [$db_auto] ? ";	
	$temp=read_in();	
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{mysql}) {
			print "Enter MySQL base install directory [autodetect]: ";
		} else {
			print "Enter MySQL base install directory [/usr/local/mysql]: ";
		}
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-mysql "; } 
		else { $db_string .= " --with-mysql=$temp "; }
	}
	
	if ($db{pgsql}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include PostgreSQL support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{pgsql}) {
			print "Enter PostgreSQL base install directory [autodetect]: ";
		} else {			
			print "Enter PostgreSQL base install directory [/usr/local/pgsql]: ";
		}
			
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-pgsql "; } 
		else { $db_string .= " --with-pgsql=$temp "; }
	}			

	if ($db{msql}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include mSQL support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{msql}) {
			print "Enter mSQL base install directory [autodetect]: ";
		} else {
			print "Enter mSQL base install directory [/usr/local/Hughes]: ";
		}
			
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-msql "; } 
		else { $db_string .= " --with-msql=$temp "; }
	}
	
	if ($db{ibase}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include InterBase support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{ibase}) {
			print "Enter InterBase base install directory [autodetect]: ";
		} else {
			print "Enter InterBase base install directory [/usr/interbase]: ";
		}
			
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-ibase "; } 
		else { $db_string .= " --with-ibase=$temp "; }
	}			

	if ($db{ora7}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include Oracle7 support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		print "Enter Oracle home directory [\$ORA_HOME]: ";
			
		$temp=read_in_nolc();
		if ($temp eq '') { $db_string .= " --with-oracle7 "; } 
		else { $db_string .= " --with-oracle7=$temp "; }
	}		

	if ($db{ora8}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include Oracle8 support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{ora8}) {
			print "Enter Oracle home directory [autodetect]: ";
		} else {
			print "Enter Oracle home directory [/oracle8/app/oracle/product/8.0.5]: ";
		}
			
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-oracle8 "; } 
		else { $db_string .= " --with-oracle8=$temp "; }
	}
			
	if ($db{ora8i}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include Oracle8i support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		print "Enter Oracle home directory [\$ORA_HOME]: ";
			
		$temp=read_in_nolc();
		if ($temp eq '') { $db_string .= " --with-oracle8i "; } 
		else { $db_string .= " --with-oracle8i=$temp "; }
	} 

	if ($db{ctlib}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include Ct-Lib support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{ctlib}) {
			print "Enter Ct-Lib home directory [autodetect]: ";
		} else {
			print "Enter Ct-Lib home directory [/opt/sybase]: ";
		}
			
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-ctlib "; } 
		else { $db_string .= " --with-ctlib=$temp "; }
	}

	if ($db{freetds}) { $db_auto='yes'; }
	else {  $db_auto='no'; }
	print "Include FreeTDS Ct-Lib support [$db_auto] ? ";	
	$temp=read_in();			
	if (($temp eq 'yes') || (($temp eq '') && ($db_auto eq 'yes'))) {
		if ($db{freetds}) {
			print "Enter FreeTDS Ct-Lib home directory [autodetect]: ";
		} else {
			print "Enter FreeTDS Ct-Lib home directory [/usr/local]: ";
		}
			
		$temp=read_in_nolc();
		if (($temp eq '') || ($temp eq 'autodetect')) { $db_string .= " --with-freetds "; } 
		else { $db_string .= " --with-freetds=$temp "; }
	}
}

# ---------------------------
# set_syslog
# ---------------------------
sub set_syslog {
	print "Logging settings\n";
	print "----------------\n";

	while () {
		print "Use syslog (yes) or stdout/stderr (no)? (yes/no) [$syslog]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $syslog)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$syslog=$temp;
			last;
		}
	}
	
	return if ($syslog eq 'no');
	
	print "Syslog facility (valid name from /usr/include/sys/syslog.h) [$syslog_facility]: ";
	$temp=read_in_nolc();
	if (($temp eq '') || ($temp eq $syslog_facility)) {
		return;
	} else {
		$syslog_facility=$temp;
		return;
	}
}

# ---------------------------
# set_other
# ---------------------------
sub set_other {
	print "Additional features\n";
	print "-------------------\n";
	while () {
		print "Enable Posix pthreads? (yes/no) [$pthreads]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $pthreads)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$pthreads=$temp;
			last;
		}
	}
	
	while () {
		print "Enable external parsers support? (yes/no) [$parser]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $parser)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$parser=$temp;
			last;
		}
	}

	while () {
		print "Enable MP3 tags support? (yes/no) [$mp3]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $mp3)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$mp3=$temp;
			last;
		}
	}

	while () {
		print "Enable HTTP Content-Encoding (zlib) support? (yes/no) [$zlib]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $zlib)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$zlib=$temp;
			last;
		}
	}
	
	while () {
		print "Enable aspell-based automatic word correction? (yes/no) [$aspell]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $aspell)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$aspell=$temp;
			last;
		}
	}

        while () {
                print "Enable support for extra charsets? (yes/no) [$extra_charsets]: ";
                $temp=read_in();
                if (($temp eq '') || ($temp eq $extra_charsets)) {
                        last;
                } elsif (($temp eq 'yes') || ($temp eq 'no')) {
                        $extra_charsets=$temp;
                        last;
                }
        }
	
	while () {
		print "Enable DMALLOC support ? (yes/no) [$dmalloc]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $dmalloc)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$dmalloc=$temp;
			last;
		}
	}

	while () {
		print "Enable OpenSSL support ? (yes/no) [$ssl]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $ssl)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')){
			$ssl=$temp;
			last;
		}
	}
	
	if ($ssl eq 'yes') {
		print "Enter SSL base install directory: [/usr/local/ssl]: ";
		$temp=read_in_nolc();
		$ssl_dir=$temp;
	}	
}

# ---------------------------
# set_shema()
# ---------------------------
sub set_shema {	
	print "URL parser settings\n";
	print "-------------------\n";

	while () {
		print "Enable file:// URL scheme support? (yes/no) [$file]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $file)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')){
			$file=$temp;
			last;
		}
	}
	
	while () {
		print "Enable http:// URL scheme support? (yes/no) [$http]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $http)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')){
			$http=$temp;
			last;
		}
	}

	while () {
		print "Enable ftp:// URL scheme support? (yes/no) [$ftp]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $ftp)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')){
			$ftp=$temp;
			last;
		}
	}

	while () {
		print "Enable news:// URL schema support? (yes/no) [$news]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $news)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')){
			$news=$temp;
			last;
		}
	}
	

}

# ---------------------------
# set_libs
# ---------------------------
sub set_libs {
	print "Compilation settings\n";
	print "--------------------\n";

	while () {
		print "Build shared libraries? (yes/no) [$build_shared]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $build_shared)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$build_shared=$temp;
			last;
		}
	}
	
	while () {
		print "build static libraries? (yes/no) [$build_static]: ";
		$temp=read_in();
		if (($temp eq '') || ($temp eq $build_static)) {
			last;			
		} elsif (($temp eq 'yes') || ($temp eq 'no')) {
			$build_static=$temp;
			last;
		}
	}
}

# ---------------------------
# set_layout
# ---------------------------
sub set_layout {
	print "Layout settings\n";
	print "---------------\n";

	while() {
	    print "Please set installation path [$prefix]: ";
	    $temp=read_in_nolc();
	    last if ($temp eq '');
	    if (abs_path($temp) eq abs_path('.')) {
		print "Cannot install in current directory\n";
	    } else {
		$prefix=$temp if ($temp ne '');
		last;
	    }
	}

	update_dirs($prefix);
	while() {
		print_layout();
		print "\nChange layout ? (yes/no) [no]: ";		
		$temp=read_in();
		print "\n";
		if (($temp eq 'no') || ($temp eq '')) {
			last;
		} elsif ($temp eq 'yes') {
			print "Please set User executables DIR [$bindir]: ";
			$temp=read_in_nolc();
			$bindir=$temp if ($temp ne '');
		
			print "System executables DIR [$sbindir]: ";
			$temp=read_in_nolc();
			$sbindir=$temp if ($temp ne '');
			
			print "Configuration data DIR [$sysconfdir]: ";
			$temp=read_in_nolc();
			$sysconfdir=$temp if ($temp ne '');
		
			print "Modifiable data DIR: [$localstatedir]: ";
			$temp=read_in_nolc();
			$localstatedir=$temp if ($temp ne '');
		
			print "Object code libraries DIR [$libdir]:";
			$temp=read_in_nolc();
			$libdir=$temp if ($temp ne '');
		
			print "C header files DIR [$includedir]: ";
			$temp=read_in_nolc();
			$includedir=$temp if ($temp ne '');
		
			print "Man documentation DIR [$mandir]: ";
			$temp=read_in_nolc();
			$mandir=$temp if ($temp ne '');
		} else {
			next;
		}
	}
}

# -------------------------------
# read_in
# -------------------------------
sub read_in {
	my $temp;
	chomp($temp=<STDIN>);
	return lc($temp);
}

# -------------------------------
# read_in_nolc
# -------------------------------
sub read_in_nolc {
	my $temp;
	chomp($temp=<STDIN>);
	return $temp;
}

# -------------------------------
# update_dirs(prefix)
# -------------------------------
sub update_dirs {
	my $prefix=$_[0];
	
	$bindir="$prefix/bin";
	$sbindir="$prefix/sbin";
	$sysconfdir="$prefix/etc";
	$localstatedir="$prefix/var";
	$libdir="$prefix/lib";
	$includedir="$prefix/include";
	$mandir="$prefix/man";
}


# -------------------------------
# print_layout
# -------------------------------
sub print_layout {
	print "\nConfigured layout:\n";
	print " Installation path: $prefix\n";
	print " User executables DIR: $bindir\n";
	print " System executables DIR: $sbindir\n";
	print " Configuration data DIR: $sysconfdir\n";
	print " Modifiable data DIR: $localstatedir\n";
	print " Object code libraries DIR: $libdir\n";
	print " C header files DIR: $includedir\n";
	print " Man documentation DIR: $mandir\n";
}
