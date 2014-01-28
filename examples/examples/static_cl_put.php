#!/usr/bin/php -q

<?php

/*Testing as command line - variables are static*/ 
require_once("../cl_example_config.php");
require_once("../../citrusleaf_enums.php");

// creating a Citrusleaf Client
$cl = new CitrusleafClient();
// connect to the Citrusleaf cluster
$url = "citrusleaf://".$citrusleaf_config["host"].":".$citrusleaf_config["port"];
//$url = "citrusleaf://localhost2:3000,citrusleaf://127.0.0.1:3000";
print "this is url::".$url."\n";
$result = $cl->connect($url);

// check to make sure the result is ok
print "Connecting to citrusleaf...<br>\n";
if ($result == CitrusleafResult::CITRUSLEAF_OK) {
	print "succeed!  <br>\n";
	print "response code = $result <br>\n";
} else {
	print "failed with $result  <br>\n";
	return;
}
$result = $cl->set_default_namespace($citrusleaf_config["namespace"]);
$ns = $cl->get_default_namespace();
print "namespace used = $ns   <br>\n";
$test_result = "SUCCESS";
for($i=1;$i<=10000;$i++) {	
	// adding the requested row, plus an integer type
	$bin = 'bin1';
	$bin .= $i;
	$result = $cl->set_default_bin($bin);
	$key = 'key1'+strval($i);
	$key .= $i;
	$value = 'value1';
	$value .= $i;
	$binarray =  array($bin);
	//var_dump($binarray);
	$result = $cl->put($key,$value);
	print "data inserted into Citrusleaf: response code = $result   <br>\n";

	//reading the key back
	$response = $cl->get($key);

	// "get" functions return an actual object instead of just a response_code. Please see 
	// print "Now reading the row ",$key," back... <br>\n";

	// this should be checked against CitrusleafResult to check success or failure
	$code = $response->get_response_code();
	if($code != 0){
		$test_result = "FAILURE";
	}
	// print "response_code=$code <br>\n";

	// "genearation" can be used on writes to achieve optimistic locking 
	$generation = $response->get_generation();
	// print "generation = $generation   <br>\n";
	$outputfile='test.txt';
	$read_bin_vals = $response->get_bins();
	$response->free_bins();
	// print "bins in key: <br>\n";
	print "<br>\n ---Bin array--- <br>\n";
	var_dump($read_bin_vals);
	$binarray = array($bin);
	print "---Bin names array--- <br>\n";
	var_dump($binarray);
	$response = $cl->get($key,$binarray);
	print "Now reading the key back  <br>\n ";
	$code = $response->get_response_code();
	if($code != 0){
		$test_result = "FAILURE";
	}
	print "response code = $code   <br>\n";
	$read_bin_vals = $response->get_bins();
	var_dump($read_bin_vals);
	$response->free_bins();
}
print $test_result;
?>
