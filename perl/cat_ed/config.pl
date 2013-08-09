##
# Database configuration parameters
# $dbport=3306;
$dbhost='localhost';
$dbname='udm';
$dbuser='udm';
$dbpass='udm';

# $dbtype can be mysql, psql or oracle
# $dbtype='psql';
$dbtype='mysql';


# name of root to add if database is empty
$root_name='Home'; 

# Relative path with leading slash where unknown.gif and folder.gif located maybe empty)
$images_path='/icons/'; 

# tree filling strategy : 0 - new records always inserted at the end of tree (old behavior)
#                         1 - new records inserted in first empty slot (new optimal strategy, DEFAULT) 
$fill=1; 

# debug output (0-no, 1-yes)
$DEBUG=0;

# arbitrary base for path and link field in categories table ,range: {9 to 36}
# for example:
# set to 16 (default) if you want 01..AA..FF values
# set to 36 (this is max allowed value) if you want 01..AA..ZZ values
$path_base=16;

1;
