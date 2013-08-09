package Dataparksearch;

# Copyright (C) 2004-2007, Datapark corp.
# Copyright (C) 2000, Gecko. http://www.gecko.fr/
# Copyright (C) 1999,2000 Udmsearch developers team <devel@search.udm.net>

# You may distribute under the terms of either the GNU General Public
# License or the Artistic License, as specified in the Perl README file.

use strict;
use vars qw( $VERSION @ISA $AUTOLOAD );

require DynaLoader;

@ISA     = qw( DynaLoader );
$VERSION = '4.53';

bootstrap Dataparksearch;

my %methods = map { $_ => 1 } qw( total_found page_number W WE
                                  num_pages is_next work_time 
				  word_info word_info_ext first last );

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub AUTOLOAD {

  my $self = shift;
  return if $AUTOLOAD =~ /::DESTROY$/;
  $AUTOLOAD =~ s/^.*:://;

  return $self->value( $AUTOLOAD ) if $methods{$AUTOLOAD};
  
  warn(sprintf('%s error : "%s" is not a valid method !',ref($self||$self), $AUTOLOAD));

}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub new {

  my $class  = shift;
  my $self   = {};
  my %params = @_;

  unless( $params{'config'} || $params{'template'} )
    { warn 'Need a "config" or "template" argument'; return; }

	if ( $params{'config'} ) {
		$self->{'config'}       = $params{'config'};
	} elsif ( $params{'template'} ) {
		$self->{'template'}     = $params{'template'};
	}

#  $self->{'LocalCharset'} = $params{'LocalCharset'} if $params{'LocalCharset'};
#  $self->{'BrowserCharset'} = $params{'BrowserCharset'} if $params{'BrowserCharset'};

	return bless $self, $class;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub query { 
 
  my $self = shift;
  my %params = @_;

  # clean up
  foreach ( qw( mode sort ps ul pn wf wm result ) ){ delete $self->{$_} if exists $self->{$_} }

  $self->{'mode'}  = $params{'mode'}  if $params{'mode'};
  $self->{'mode'}  = $params{'m'}     if $params{'m'};
  $self->{'sort'}  = $params{'sort'}  if $params{'sort'}; 
  $self->{'ps'}    = $params{'ps'}    if $params{'ps'};
  $self->{'ul'}    = $params{'ul'}    if $params{'ul'};

  $self->{'wf'}    = $params{'wf'}    if $params{'wf'};
  $self->{'wm'}    = $params{'wm'}    if $params{'wm'};

  $self->{'pn'}    = $params{'pn'}    if defined $params{'pn'};

  $self->{'result'} = $self->_DpsQuery($params{'query'});

  return $self->{'result'};

}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub records {

  my $self    = shift;
  
  unless ( exists $self->{'result'} )
  { warn 'You Must query before.'; return; }
  
  @{$self->{'result'}{'records'}};

}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub value {

  my $self    = shift;
  my $request = shift;

  unless ( exists $self->{'result'} )
  { warn 'You Must query before.'; return; }
  
  $self->{'result'}{$request};

}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub ShowReferers {

  my $self   = shift;
  my $stats  = $self->GetReferers();
  my $return = <<EOF;

          URLs and referers 

EOF

  foreach my $line ( @$stats ){
    $return .= join(' ', @$line) . "\n";
  }

  return $return;
  
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub ShowStatistics {

  my $self   = shift;
  my $stats  = $self->GetStatistics();
  my $return = <<EOF;

          Database statistics

   -----------------------------
EOF

  foreach my $line ( @$stats ){
    if ( $line->[0] >= 0 ){
      $return .= sprintf('%10d %10d %10d %s', @$line)."\n"
    } else {
      $return .= "   -----------------------------\n";
      $return .= sprintf("%10s %10d %10d\n\n\n","Total",$line->[1],$line->[2]);
    }
  }

  return $return;

}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub GetSEA {
    my $self = shift;
    my $url = shift;

    $self->{'URL'} = $url;
    return $self->_GetSEA($url);
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub GetSEAbyId {
    my $self = shift;
    my $dp_id = shift;

    $self->{'DP_ID'} = $dp_id;
    return $self->_GetSEAbyId($dp_id);
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub WordNormalize {
    my $self = shift;
    my $wrd = shift;
    return $self->_WordNormalize($wrd);
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub Finish {
  	my $self   = shift;
	$self->Finish();
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

1;

__END__

=head1 NAME

Dataparksearch - Perl extension for DataparkSearch Engine

=head1 SYNOPSIS

  use Dataparksearch;

  # Init Dataparksearch
  my $search = new Dataparksearch('DBAddr' => 'pgsql://user:password@hostname/dbname/?dbmode=single');

  # With two words, mode all,sort by rate, page size at 10 and page number at 1.
  $search->query('query' => 'perl apache',
                 'mode' => 'all',
		 'sort' => 'rate', 
		 'ps' => '10',
		 'pn' => 1);

  # Using results.
  print 'Total found : ', $search->total_found,"\n";
  print 'Page Number : ', $search->page_number,"\n";
  print 'Pages Number: ', $search->num_pages,"\n";
  print 'Is next ?   : ', $search->is_next, "\n";
  print 'work time   : ', $search->work_time, " seconds\n";
  print 'word info   :',  $search->word_info, "\n";
		 
  print sprintf('Results from %d to %d ', $search->{'first'}, $search->{'last'}),"\n\n";

  foreach my $result ( $search->records ) {

    print sprintf('[%d] %s -> %s [%dKo]', 
                  $result->{'url_id'}, 
		  $result->{'url'}, 
		  $result->{'title'},
		  $result->{'size'}/1024),"\n";
  }

=head1 DESCRIPTION

Dataparksearch is a simple frontend to libdpsearch.

=head2 Creating New Dataparksearch

  $DBAddr = 'pgsql://user:pass@hostname/database/?dbmode=single'

  $search = new Dataparksearch(DBAddr=>$DBAddr);

Constructor.

If you want to use TrackQuery, you can use it like this:

  $search = new Dataparksearch(DBAddr=>$DBAddr,
                                TrackQuery=>yes);


Also Affix & Spell are supported and LocalCharset may be specified.

  $search = new Dataparksearch(DBAddr=>$DBAddr,
                                LocalCharset=>'iso-8859-1',
                                Affix=>{'en'=>'en.aff',
                                        'de'=>'/usr/local/dicts/de.aff'},
                                Spell=>{'en'=>'en.dict',
                                        'de'=>'/usr/local/dicts/de.dict'});


=head2 Querying Dataparksearch

I<$search>->query(query=>'word1 word2', param1 => value1 );

=head2 Query params

=over 4

=item query

'query' is the search string.

=item mode

'mode' argument can be 'all', 'any' or 'bool'.

=item sort

'sort' argument can be 'date' or 'rate'.

=item ps

'ps' define the number of results by page

=item pn

'pn' define the page number.

=back

=head2 Using results infos

=over 4

=item total_found

I<$search>->total_found : return the total of results found.

=item num_pages

I<$search>->num_pages : return the number of pages.

=item is_next

I<$search>->is_next : if current page is the last one return 0, else 1.

=item work_time

I<$search>->work_time : return the work time in seconds.

=item word_info

I<$search>->word_info : 

=item first 

I<$search>->first : return the first result number of the current page.

=item last

I<$search>->last : return the last result number of the current page.

=back

=head2 Using results values

my @results = $search->records;

Return a sorted array of hash ref. Each hash have the following keys defined :

category content_type description hops indexed,
keywords last_index_time last_mod_time next_index_time
order, rating referrer size status tag text title
url url_id

=head1 AUTHOR

Maxime Zakharov E<lt>maxime@sochi.net.ruE<gt>

=head1 SEE ALSO

perl(1).

=cut
