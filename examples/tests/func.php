#!/usr/bin/php -q
<?php
   // a functional test of just about everything supported

	require_once("../cl_example_config.php");
	require_once("../../citrusleaf_enums.php");
	include '../mylib.php';

	function int_test($cl) {
		$key = mt_rand();
		$val = mt_rand();

		$rv = $cl->put($key, $val);
		if ($rv!=CitrusleafResult::CITRUSLEAF_OK) {
			print(" int put failed: rv  <br>\n");
			var_dump($rv);
			return false;
		}

		$rv = $cl->get($key);
		if ($rv->get_response_code()!=CitrusleafResult::CITRUSLEAF_OK) {
			print(" int get failed  <br>\n" );
		}

		$read_bin_vals = $rv->get_bins();
		if ($read_bin_vals["binname"] != $val) {
			printf(" int get failed: expected %d instead   <br> \n",$val);
			var_dump($read_bin_vals);
			return false;
		}
		$rv->free_bins();	
		return true;
	}

	function str_test($cl) {
		$key = 'my test string foobaz\n\r';
		$val = $key . $key;

		$rv = $cl->put($key, $val);
		if ($rv!=CitrusleafResult::CITRUSLEAF_OK) {
			printf(" string put failed: rv <br>\n");
			var_dump($rv);
			return false;
		}

		$rv = $cl->get($key);
		if ($rv->get_response_code()!=CitrusleafResult::CITRUSLEAF_OK) {
			print(' string get failed' );
		}

		$read_bin_vals = $rv->get_bins();
		if ($read_bin_vals["binname"] != $val) {
			printf(" string get failed: expected %s instead  <br>\n",$val);
			var_dump($read_bin_vals);
			return false;
		}
		$rv->free_bins();	
		return true;
	}

	
	function binary_str_test($cl) {
		$key = 'php_bkey';
		$val = "\0\1\2\3\4\5\6";

		$rv = $cl->put($key, $val);
		if ($rv!=CitrusleafResult::CITRUSLEAF_OK) {
			printf(" string put failed: rv <br>\n");
			var_dump($rv);
			return false;
		}

		$rv = $cl->get($key);
		if ($rv->get_response_code()!=CitrusleafResult::CITRUSLEAF_OK) {
			printf(" string get failed  <br>" );
		}

		$read_bin_vals = $rv->get_bins();
		if ($read_bin_vals["binname"] != $val) {
			printf(" string get failed: expected %s instead  <br>\n",$val);
			var_dump($read_bin_vals);
			return false;
		}
		var_dump($read_bin_vals);
		$rv->free_bins();	
		return true;
	}

	function del_test($cl) {
		$key = mt_rand();
		$val = mt_rand();

		$rv = $cl->put($key, $val);
		if ($rv!=CitrusleafResult::CITRUSLEAF_OK) {
			print(" del put1 failed:  <br>\n");
			print_r($rv);
			return false;
		}

		$rv = $cl->get($key);

		if ($rv->get_response_code()!=CitrusleafResult::CITRUSLEAF_OK) {
			print(" del get failed   <br>\n" );
		}
		$read_bin_vals = $rv->get_bins();
		if ($read_bin_vals["binname"] != $val) {
			printf(" del get1 failed: value should exist expected %d got  <br>\n",$val);
			var_dump($read_bin_vals);
			return false;
		}
		$rv->free_bins();	
		$rv = $cl->delete($key);
		if ($rv!=CitrusleafResult::CITRUSLEAF_OK) {
		   	print(" del delete failed, should return true \n");
			var_dump($rv);
			return false;
		}
		
		$rv = $cl->get($key);
		if ($rv->get_response_code()!=CitrusleafResult::CITRUSLEAF_KEY_NOT_FOUND_ERROR) {
			print("  del get should have failed ,   <br>\n");
			return false;
		}
		$rv->free_bins();	
		return true;
	}


	function array_test($cl) {
		
		$key = mt_rand(); 
		$val = array(1 => 'brian', 2 => 'bulkowski', 'name' => '1', 'value' => 2 );

		$rv = $cl->put($key, $val);
		if ($rv!=CitrusleafResult::CITRUSLEAF_OK) {
			print(" array put failed: rv   <br>\n");
			var_dump($rv);
			return false;
		}

		$rv = $cl->get($key);
		$read_bin_vals = $rv->get_bins();
		//var_dump($read_bin_vals);
		$rv->free_bins();	
		// compare two arrays....
		$dif = array_diff_assoc($read_bin_vals["binname"], $val);
		if ($dif) {
			print(" array get failed: the difference array was not empty, instead <br>");
			var_dump($dif);
			return false;
		}
		return true;
	}

	function add_test($cl) {
		
		$key = mt_rand();
		$start_val = mt_rand();
		$n_incr = 4;
		
		$cl->delete($key);
		
		// simplest test
		$cl->put($key, $start_val);
		$cl->add($key, 1);
		$response = $cl->get($key);
		$read_bin_vals = $response->get_bins();
		if ($read_bin_vals["binname"]  != $start_val + 1) {
			printf ( " sum did not add one, was %d should be %d    <br>\n",$read_bin_vals["binname"], $start_val+1);
			return false;
		}
		$response->free_bins();
		$cl->delete($key);

		// slightly more complicated
		$cl->put($key, $start_val);
		$expected = $start_val;

		for ( $i=0 ; $i < $n_incr ; $i++) {
			$cl->add($key, $i);
			$expected += $i;
		}

		$response = $cl->get($key);
		$read_bin_vals = $response->get_bins();
		if ($read_bin_vals["binname"]  != $expected) {
			printf ( " summing does not add up should be %d   <br>\n",$expected);
			return false;
		}
		$response->free_bins();
		return true;
	}


	//
	// MAINLINE MAINLINE MAINLINE
	//
	// connect to citrusleaf
	
	$cl = new CitrusleafClient();
	$url = "citrusleaf://".$citrusleaf_config["host"].":".$citrusleaf_config["port"];
	$result = $cl->connect($url);
	print "Connecting to $url ...";
	if ($result == CitrusleafResult::CITRUSLEAF_OK) {
		print "succeed!  ";
		print "response code = $result <br>\n";
	} else {
		print "failed with $result <br>\n";
		return;
	}

	$result = $cl->set_default_namespace($citrusleaf_config["namespace"]);
	$ns = $cl->get_default_namespace();
	print "namespace used = $ns  <br>\n";
	$cl->set_default_bin("binname");    
	$start = get_time();

	// run tests
	if ( int_test($cl)) {
		print "int_tests passed <br>\n";
	} else {
		print( "FAIL FAIL FAIL int_test  <br>\n");
		return;
	}

	if ( str_test($cl)) {
		print "str_tests passed <br>\n";
	} else {
		print( "FAIL FAIL FAIL str_test   <br>\n");
		return;
	}
	if ( binary_str_test($cl)) {
		print "binary_str_tests passed <br>\n";
	} else {
		print( "FAIL FAIL FAIL str_test   <br>\n");
		return;
	}

	if ( del_test($cl)) {
		print "del_tests passed <br>\n";
	} else {
		print( "FAIL FAIL FAIL del_test   <br>\n");
		return;
	}
	if ( array_test($cl)) {
		print "array_tests passed <br>\n";
	} else {
		print( "FAIL FAIL FAIL array_test  <br>\n");
		return;
	}
	if ( add_test($cl)) {
		print "add_tests passed <br>\n";
	} else {
		print( "FAIL FAIL FAIL add_test   <br>\n");
		return;
	}
	$delta = get_time() - $start;
	
	if ($delta > 0.5)
		printf("warning: this script should be very quick, not $f secs   <br> FAILURE \n",$delta);
	else
		print("All test ran in $delta secs  <br>\n");

		print("SUCCESS");
	
?>

