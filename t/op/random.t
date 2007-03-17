#!perl
# Copyright (C) 2001-2005, The Perl Foundation.
# $Id$

use strict;
use warnings;
use lib qw( . lib ../lib ../../lib );
use Test::More;
use Parrot::Test tests => 1;

=head1 NAME

t/op/random.t - Random numbers

=head1 SYNOPSIS

        % prove t/op/random.t

=head1 DESCRIPTION

Tests random number generation

=cut

pasm_output_is( <<'CODE', <<OUT, "generate random int" );
    new P0, .Random
    set I0, P0
    print "Called random just fine\n"
    end
CODE
Called random just fine
OUT

# Local Variables:
#   mode: cperl
#   cperl-indent-level: 4
#   fill-column: 100
# End:
# vim: expandtab shiftwidth=4:
