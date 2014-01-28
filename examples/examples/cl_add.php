#!/usr/bin/php -q

<?php
    // Demonstrate numerically incrementing an integer bin";
    require_once("../cl_example_config.php");
    require_once("../../citrusleaf_enums.php");     // citrusleaf response codes

    $passcount = 0;
    $expected_passcount = 6;

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

    print "Demonstrates numerically incrementing an integer bin: <br>\n";

    $result = $cl->set_default_namespace($citrusleaf_config["namespace"]);
    $ns = $cl->get_default_namespace();
    print "namespace used = $ns <br>\n";

    $key = "key_numeric";
    print "Attempt to delete previous key of: $key <br>\n";
    $code = $cl->delete($key);

    $result = $cl->set_default_bin("integer_bin");
    $initial_value = 50;
    $code = $cl->put($key, $initial_value);
    print "(1) Inserted row with integer bin. Response code = $code <br>\n";
    if($code == 0) {
        $passcount++;
    }   

    $citrusResponse = $cl->get($key);
    $code = $citrusResponse->get_response_code();
    print "Reading row back... Response code = $code <br>\n";
    if($code == 0) {
        $passcount++;
    }   

    $read_bin_vals = $citrusResponse->get_bins();
    print "bins in key: <br>\n";
    var_dump($read_bin_vals);
    $citrusResponse->free_bins();
    $generation = $citrusResponse->get_generation();
    print "generation = $generation <br>\n";

    $inc = 20;
    $code=$cl->add($key, $inc);
    print "(2) Adding $inc to the bin ... Response code = $code <br>\n";
    if($code == 0) {
        $passcount++;
    }   

    $citrusResponse = $cl->get($key);
    $code = $citrusResponse->get_response_code();
    print "Reading row back... Response code = $code <br>\n";
    if($code == 0) {
        $passcount++;
    }   
    $read_bin_vals = $citrusResponse->get_bins();
    print "bins in key: <br>\n";
    var_dump($read_bin_vals);
    $citrusResponse->free_bins();
    $generation = $citrusResponse->get_generation();
    print "generation = $generation <br>\n";
    if($generation == 2) {
        $passcount++;
    }
    if($read_bin_vals["integer_bin"] == $initial_value + $inc){
        $passcount++;
    }

    if ($passcount == $expected_passcount) {
        print "SUCCESS";
    }   
    else {
        print "FAILURE";
    }


?>
