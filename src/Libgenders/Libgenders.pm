package Libgenders;

use 5.006;
use strict;
use warnings;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

our @ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Libgenders ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS = ( 'all' => [ qw(
	GENDERS_ERR_FREEMEM
	GENDERS_ERR_INTERNAL
	GENDERS_ERR_ISFREE
	GENDERS_ERR_MAGIC
	GENDERS_ERR_NOTCLOSED
	GENDERS_ERR_NOTFOUND
	GENDERS_ERR_NOTOPEN
	GENDERS_ERR_NULLPTR
	GENDERS_ERR_OPEN
	GENDERS_ERR_OUTMEM
	GENDERS_ERR_OVERFLOW
	GENDERS_ERR_PARAMETERS
	GENDERS_ERR_PARSE
	GENDERS_ERR_READ
	GENDERS_ERR_SUCCESS
) ] );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

our @EXPORT = qw(
	GENDERS_ERR_FREEMEM
	GENDERS_ERR_INTERNAL
	GENDERS_ERR_ISFREE
	GENDERS_ERR_MAGIC
	GENDERS_ERR_NOTCLOSED
	GENDERS_ERR_NOTFOUND
	GENDERS_ERR_NOTOPEN
	GENDERS_ERR_NULLPTR
	GENDERS_ERR_OPEN
	GENDERS_ERR_OUTMEM
	GENDERS_ERR_OVERFLOW
	GENDERS_ERR_PARAMETERS
	GENDERS_ERR_PARSE
	GENDERS_ERR_READ
	GENDERS_ERR_SUCCESS
);
our $VERSION = '0.01';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "& not defined" if $constname eq 'constant';
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/ || $!{EINVAL}) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    croak "Your vendor has not defined Libgenders macro $constname";
	}
    }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
	if ($] >= 5.00561) {
	    *$AUTOLOAD = sub () { $val };
	}
	else {
	    *$AUTOLOAD = sub { $val };
	}
    }
    goto &$AUTOLOAD;
}

bootstrap Libgenders $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__
# Below is stub documentation for your module. You better edit it!

=head1 NAME

Libgenders - Perl extension for blah blah blah

=head1 SYNOPSIS

  use Libgenders;
  blah blah blah

=head1 DESCRIPTION

Stub documentation for Libgenders, created by h2xs. It looks like the
author of the extension was negligent enough to leave the stub
unedited.

Blah blah blah.

=head2 EXPORT

None by default.

=head2 Exportable constants

  GENDERS_ERR_FREEMEM
  GENDERS_ERR_INTERNAL
  GENDERS_ERR_ISFREE
  GENDERS_ERR_MAGIC
  GENDERS_ERR_NOTCLOSED
  GENDERS_ERR_NOTFOUND
  GENDERS_ERR_NOTOPEN
  GENDERS_ERR_NULLPTR
  GENDERS_ERR_OPEN
  GENDERS_ERR_OUTMEM
  GENDERS_ERR_OVERFLOW
  GENDERS_ERR_PARAMETERS
  GENDERS_ERR_PARSE
  GENDERS_ERR_READ
  GENDERS_ERR_SUCCESS


=head1 AUTHOR

A. U. Thor, E<lt>a.u.thor@a.galaxy.far.far.awayE<gt>

=head1 SEE ALSO

L<perl>.

=cut