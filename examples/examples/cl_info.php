#!/usr/bin/php -q

<?php
    // Demonstrate cl_info

    // reading in configuration
    require_once("../cl_example_config.php");

    // reading in response codes
    require_once("../../citrusleaf_enums.php");
   
    $passcount = 0;
    $expected_passcount = 2;

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

    $names = "build,service,status";
    $result = $cl->info($citrusleaf_config["host"], $citrusleaf_config["port"], $names);
    if(is_array($result)){
        $passcount++;
    }

    if($result){
        $passcount++;
    }

    printResult($result, $names);
    
    if ($passcount == $expected_passcount) {
        print "SUCCESS";
    }
    else{
        print "FAILURE";
    }

function printResult($result, $name) {
	if (is_array($result)) {
		if ($result) {
			print "Info ($name): <br>\n";
			var_dump($result);
		}
		else {
			echo "<p><br>No info for $name<br>\n";
		}
	}
	else if ($result) {
		print "Got error - got $result <br>\n";
	}
}

?>

