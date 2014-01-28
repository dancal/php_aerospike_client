#!/usr/bin/php -q

<?php
    
    // configuration
    require_once("../cl_example_config.php");

    // response codes
    require_once("../../citrusleaf_enums.php");

    // possible unique options
    $option_unique = 1;
    $option_unique_bin = 2;
    $option_use_generation = 3;


    $expected_passcount = 6;
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

    $key = 'opt_test_key';

    print "Attempt to delete previous key of: $key (may fail) <br>\n";
    $code = $cl->delete($key);

    print "<br>\n";
    print "Insert with 'unique' option -- key must not pre-exist in DB";
    $cl->set_default_bin('bin1');
    $value = array ('name1' => 'value1');
    $code = $cl->put($key,$value,$option_unique);
    if($code==0){
        $passcount++;
    }
    check_insert_rc($code);

    print "<br>\n<br>\n";
    print "Insert with 'unique_bin' option -- key must not pre-exist in DB <br>\n";
    print "Expect first insert fails due to duplicate bin name <br>\n";
    $value = array ('name2' => 'value2');
    $code = $cl->put($key,$value,$option_unique);
    if($code!=0){
        $passcount++;
    }
    check_insert_rc($code);
			
    print "<br>\n"; 
    print "set bin name to something else and it should succeed <br>\n";
    $cl->set_default_bin('bin2');
    $code = $cl->put($key,$value,$option_unique_bin);
    if($code==0){
        $passcount++;
    }
    check_insert_rc($code);
    print "<br>\n";
    print "Insert with 'use_generation' option. <br>\n";
    $generation = 1;
    print "Expect failure when generation = $generation <br>\n";
    $cl->set_default_bin('bin3');
    $code = $cl->put($key,$value,$option_use_generation, $generation);
    if($code!=0){
        $passcount++;
    }
    check_insert_rc($code);

    print "<br>\n";
    $generation = 2;
    print "Expect sucess when generation = $generation <br>\n";
    $code = $cl->put($key,$value,$option_use_generation, $generation);
    if($code==0){
        $passcount++;
    }
    check_insert_rc($code);

    print "<br>\n";
    $citrusResponse = $cl->get($key);
    $code = $citrusResponse->get_response_code();
    if($code==0){
        $passcount++;
    }
    print "Reading row back... Response code = $code <br>\n";
    $read_bin_vals = $citrusResponse->get_bins();
    print "bins in key: <br>\n";
    var_dump($read_bin_vals);
    $citrusResponse->free_bins();
    $generation = $citrusResponse->get_generation();
    print "generation = $generation <br>\n";
    if($passcount == $expected_passcount){
        print "SUCCESS";
    }
    else{
        print "FAILURE";
    }

function check_insert_rc($code) {
	if ($code == 0) {
		print "insert success <br>\n";
	}
	else {
		print "insert failed.  Response code = $code <br>\n";
	}
}








?>
