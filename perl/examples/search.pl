#!/usr/bin/perl -w

use Dataparksearch;

# Init Dataparksearch
#my $search = new Dataparksearch('DBAddr' => 'pgsql://user:passwd@localhost/search/?dbmode=cache');
my $search = new Dataparksearch('template' => '/usr/local/dpsearch/etc/search.htm');

# With two words, mode all,sort by rate, page size at 10 and page number at 1.

         $search->query('query' => 'apache',
                        'mode' => 'all',
                        'sort' => 'IRD',
                        'ps' => '10',
                        'pn' => 1);

# Using results.
         print 'Total found   :', $search->total_found,"\n";
         print 'Page Number   :', $search->page_number,"\n";
         print 'Pages Number  :', $search->num_pages,"\n";
         print 'Is next ?     :', $search->is_next, "\n";
         print 'work time     :', $search->work_time, " seconds\n";
         print 'word info     :', $search->word_info, "\n";

         print sprintf('Results from %d to %d ', $search->first, $search->last),"\n\n";

         foreach my $result ( $search->records ) {

           print sprintf('[%d] %s -> %s [%dKo]',
                         $result->{'DP_ID'},
                         $result->{'url'},
                         $result->{'title'},
                         $result->{'Content-Length'}/1024),"\n";
         }

# Free allocations
	$search->Free();

