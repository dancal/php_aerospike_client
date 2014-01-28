#!/usr/bin/php -q

<?php

    require_once("../cl_example_config.php");
    require_once("../../citrusleaf_enums.php");
    include 'mylib.php';
    $expected_passcount = 2;
    $passcount = 0;
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

    $result = $cl->set_default_namespace('test');
    $ns = $cl->get_default_namespace();
    print "namespace used = $ns <br>\n";

    // checking that null keys are not accepted
    print "Test null key is not acceptable to get()<br>\n";
    $res = $cl->get(null, "something");
    $code = $res->get_response_code();
    print "response_code=[$code] <br>\n";
    if ($code!=CitrusleafResult::CITRUSLEAF_INVALID_API_PARAM) {
        print "failed null test <br>\n";
	return;
    }

    // for put
    print "Test null key is not acceptable to put()<br>\n";
    $code = $cl->put(null,"sampleKey");
    print "response_code= $code  <br>\n";
    if ($code!=CitrusleafResult::CITRUSLEAF_INVALID_API_PARAM) {
        print "failed null test  <br>\n";
    }
    else {
        $passcount++;
    }

    // checking extra parameters
    print "Test an extra paramter is not acceptable to get()<br>\n";
    $res = $cl->get("key", "something", "another");
    $code = $res->get_response_code();
    print "response_code = $code   <br>\n";
    if ($code!=CitrusleafResult::CITRUSLEAF_INVALID_API_PARAM) {
        print "failed null test <br>\n";
    }
    else {
        $passcount++;
    }

    if ($passcount == $expected_passcount) {
        print "SUCCESS";
    }
    else {
        print "FAILURE";
    }

?>

