#!/usr/bin/perl -w

# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..1\n"; }
END { print "not ok 1\n" unless $loaded; }

use Dataparksearch;
$loaded = 1;
print "ok 1\n";

{

  use strict;
  use POSIX qw( strftime );
  use ExtUtils::MakeMaker;

  my $user = $ENV{'USER'} || 'nobody';

  print "Using dataparksearch-perl $Dataparksearch::VERSION with dataparksearch " . Dataparksearch::DpsVersion() . "\n" ;
  
  my $DBAddr = "pgsql://$user:\@localhost/search/?dbmode=single";
  my $query  = "postgresql documentation";

  my $ans;

  do { 

    $DBAddr = prompt('What is your DBAddr ?', $DBAddr);
    $query  = prompt('What is your Query  ?', $query);

  print<<EOF;

Test will be made with :  

 DBAddr : $DBAddr
 Query  : $query

EOF

    $ans = prompt('Values are ok ?', 'y');

  } while lc $ans ne 'y';

 
  my $search = new Dataparksearch('DBAddr' => $DBAddr, 
#                                'LocalCharset'=>'iso-8859-1',
#                                'Affix'       => {'en' => 'en.aff'  },
#                                'Spell'       => {'en' => 'en.dict' },
			       );
  
  print 'N Docs      : ', $search->DpsGetDocCount, "\n";

  #print $search->ShowStatistics; exit;
  #print $search->ShowReferers; exit;
  
  unless ( $search ) { print "An error has occured!\n"; exit }
  
  $search->query('query' => $query, 
                 'mode'  => 'phrase',  # all / bool /any
		 'sort'  => 'rate', # rate / date
		 'ps'    => '10',    # page size
		 'pn'    =>  0 );     # page number

  my $tformat = '%a %b %e %Y';

  unless ( $search->total_found )
  { print "No result found :",$search->word_info,"\n"; exit }

  print 'Total found : ', $search->total_found,"\n";
  print 'Page Number : ', $search->page_number,"\n";
  print 'Pages Number: ', $search->num_pages,"\n";
  print 'Is next ?   : ', $search->is_next, "\n";
  printf ("work time   : %0.2f seconds\n", $search->work_time / 1000);
  print 'word info   :',  $search->word_info, "\n";

  print sprintf('Results from %d to %d ', $search->first, $search->last), "\n\n";

 foreach my $result ( $search->records ) {

    print sprintf('[%d] %s -> %s [%dKo]', 
                  $result->{'url_id'}, 
		  $result->{'url'}, 
		  $result->{'title'}, 
		  $result->{'size'}/1024
		 ),"\n";
    print sprintf('  last_mod_time [%s]', strftime($tformat, localtime($result->{'last_mod_time'}))),"\n";
    print sprintf('  last_index_time [%s]', strftime($tformat, localtime($result->{'last_index_time'}))),"\n";

    print "\n";

  }

}


