#!/usr/bin/php -q

<?php
    // Demonstrates connect, put, and get

    require_once("../cl_example_config.php");
    require_once("../../citrusleaf_enums.php");
    include '../mylib.php';

    print "Calling into Citrusleaf Extension ... ";
    $extstr = citrusleaf();
    if (strncmp($extstr,"Citrusleaf lives!",10)==0) {
        print "success <br>\n";
    } else {
	print "failed. Extention string got = $extstr<br>\n";
	return;
    }

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

    $test_result = "SUCCESS";

    $result = $cl->set_default_namespace($citrusleaf_config["namespace"]);
    $ns = $cl->get_default_namespace();
    $n = 10000;
    echo "namespace used = $ns <br>\n";
    $cl->put("1234", "abc");
    print("Reading one same key $n times <br>\n");
    $start = get_time();
    for ($i=0;$i<$n;$i++) {
	$r = $cl->get("1234" );
	$read_bin_vals = $r->get_bins();
	if ($read_bin_vals["binname"] != "abc") {
		printf('***error***; %s should be same as %s<br>\n',$s,$read_bin_vals);
		$test_result = "FAILURE";
	}
	$r->free_bins();
    }


    $delta = get_time() - $start;
    printf("took: %f tps: %0.1f\n",$delta, 10000.0 / $delta);
    printf("%s\n", $test_result);

?>

