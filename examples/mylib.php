#!/usr/bin/php -q

<?php

	# not sure why there isn't a simple library function that returns doubles...
	function get_time() {
		list($u,$s) = explode(' ',microtime());
		return( (float)$u + (float)$s);
	}

	# pass in a microtime, print the difference
	function microtime_delta($start, $end) {
		list($start_usec, $start_sec) = explode(' ',$start);
		list($end_usec, $end_sec) = explode(' ',$end);
		return( ( (float)$end_sec + (float)$end_usec) -
                ( (float)$start_sec + (float)$start_usec) );
	}	

?>

