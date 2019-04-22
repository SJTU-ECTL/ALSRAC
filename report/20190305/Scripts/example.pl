#!/usr/bin/perl

use strict;
use warnings;

for (my $i = 1; $i < 100; $i++) {
	if ($i == 99) {
		print $i." Luftballons reached!\n\n";
	} else {
		print $i." Luftballons...\n";
	}
}

exit;