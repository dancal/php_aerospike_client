#!/usr/bin/php -q

<?php
  // Demonstrates put, delete, and exists functions

require_once("../cl_example_config.php");
require_once("../../citrusleaf_enums.php");
   
$passcount = 0;
$expected_passcount = 6;


function putKey($key)
{
	global $cl;

	print "Putting key: \"$key\"....<br>\n";
	$code = $cl->set_default_bin('B1');
	if ($code != CitrusleafResult::CITRUSLEAF_OK) {
		print "set_default_bin() Failed! ($code) <br>\n";
		return $code;
	}

	$code = $cl->put($key, 'V1');
	if ($code == CitrusleafResult::CITRUSLEAF_OK) {
		print "Succeeded! ($code) <br>\n";
	} else {
		print "Failed! ($code) <br>\n";
	}
	return $code;
}

function getKey($key) 
{
	global $cl;

	print "Getting key: \"$key\"....<br> \n";

	$citrusleafResponse = $cl->get($key);
	$code = $citrusleafResponse->get_response_code();
	if ($code == CitrusleafResult::CITRUSLEAF_OK) {
		print "Succeeded! ($code) <br>\n";
	} else {
		print "Failed! ($code) <br> \n";
		return $code;
	}
	
	print "Key: $key  <br> \n"; 
	print "Value: ";
	$read_bin_vals = $citrusleafResponse->get_bins();
	var_dump($read_bin_vals);
	$citrusleafResponse->free_bins();
	return $code;
}

function existsKey($key)
{
	global $cl;

	print "Does key: \"$key\" exist....<br>\n";

	$citrusleafResponse = $cl->exists($key);
	$code = $citrusleafResponse->get_response_code();
	if ($code == CitrusleafResult::CITRUSLEAF_OK) {
		print "Yes! ($code) <br>\n";
	} else {
		print "No! ($code) <br>\n";
	}
	return $code;
}

function deleteKey($key)
{
	global $cl;

	print "Deleting key: \"$key\"....<br>\n";

	$code = $cl->delete($key);
	if ($code == CitrusleafResult::CITRUSLEAF_OK) {
		print "Succeeded! ($code) <br>\n";
	} else {
		print "Failed! ($code) <br>\n";
	}
	return $code;
}

print "Citrusleaf Key Exists Example<br>\n";

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


print "[Expected to fail.] <br>\n";
if(existsKey("K9")!=0){
	$passcount++;
}

if(putKey("K9")==0){
	$passcount++;
}

if(getKey("K9")==0){
	$passcount++;
}

print "[Expected to succeed.] <br>\n";
if(existsKey("K9")==0){
	$passcount++;
}

if(deleteKey("K9")==0){
	$passcount++;
}

print "<br>[Expected to fail.] <br> \n";
if(existsKey("K9")!=0){
	$passcount++;
}

if ($passcount == $expected_passcount) {
	print "SUCCESS";
}
else{
	print "FAILURE";
}   

?>
