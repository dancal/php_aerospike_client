
<?php
	//Demonstration of shared memory usage

	//Reading the config parameters
	require_once("../cl_example_config.php");

	//To read the response codes
	require_once("../../citrusleaf_enums.php");
	
	$passcount = 0;
	$expected_passcount = 4;
	
	$cl= new CitrusleafClient();
	//arguments to use shm. No of server nodes and key for shared memory(type key_t).
	//Default (0,0) sets no of nodes to 64 and key to 788721985
	$result=$cl->use_shm(0,0);
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
	
	print "Demonstrate multi-bin record inserts <br>\n";
    $result = $cl->set_default_namespace($citrusleaf_config["namespace"]);
    $ns = $cl->get_default_namespace();
    print "namespace used = $ns <br>\n";

    // Demo 1 add a multi-bin record
    $key = "multi-bin-key";
    print "Deleting previous key $key. Deletion may fail. <br>\n";
    $result = $cl->delete($key);
    $values = array("first_bin" => 'first_value', "second_bin" => 2);
    $result = $cl->putmany($key,$values);
    print "(1) multi-bin object inserted into Citrusleaf: response code = $result <br>\n";
    if($result==0){
        $passcount++;
    }

    // read record back
    $response = $cl->get($key);
    print "reading the row ".$key." back ... <br>\n";
    // this should be checked against CitrusleafResult to check success or failure
    $code = $response->get_response_code();
    print "response_code = $code <br>\n";
    if($result==0){
        $passcount++;
    }

    $read_bin_vals = $response->get_bins();
    print "bins:";
    var_dump($read_bin_vals);
    $response->free_bins();

    // Demo 2 adding single-bin serializable php object, an "array"
    $key = "single-bin-php-object";
    print "Deleting previous key $key. Deletion may fail. <br>\n";
    $result = $cl->delete($key);
	 $myarray = array("element1" => 'value1', "element2" => 555);
    $result = $cl->set_default_bin('single-bin');
    $result = $cl->put($key,$myarray);
    print "(2) single-bin object, value a php object (array) inserted into Citrusleaf: response code = $result <br>\n";
    if($result==0){
        $passcount++;
    }

    // reading record back
    $response = $cl->get($key);
    print "reading the row ".$key." back ... <br>\n ";
    // this should be checked against CitrusleafResult to check success or failure
    $code = $response->get_response_code();
    print "response_code = $code <br>\n";
    if($result==0){
        $passcount++;
    }

    $read_bin_vals = $response->get_bins();
    print "bins:";
    var_dump($read_bin_vals);

    $response->free_bins();

	print $passcount;
	print $expected_passcount;
    if ($passcount == $expected_passcount) {
        print "SUCCESS";
    }
    else{
        print "FAILURE";
    }

?>

