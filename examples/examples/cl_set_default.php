#!/usr/bin/php -q
<?php
    require_once("cl_example_config.php");
    require_once("../citrusleaf_enums.php");

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

    $result = $cl->get_default_namespace();
    print "get default_namespace result [$result] <br>\n";
    $result = $cl->set_default_namespace('myspace');
    print "set_default_namespace result [$result] <br>\n";
    $result = $cl->get_default_namespace();
    print "get default_namespace result [$result] <br>\n";
 
    $result = $cl->get_default_set();
    print "get_default_set result [$result] <br>\n";
    $result = $cl->set_default_set('myset');
    print "set_default_set result [$result] <br>\n";
    $result = $cl->get_default_set();
    print "get_default_set result [$result] <br>\n";

    $result = $cl->get_default_bin();
    print "get_default_bin result [$result] <br>\n";
    $result = $cl->set_default_bin('mybin');
    print "set_default_bin result [$result] <br>\n";
    $result = $cl->get_default_bin();
    print "get_default_bin result [$result] <br>\n";


    $result = $cl->get_default_writepolicy();
    print "get_default_writepolicy result [$result] <br>\n";
    $result = $cl->set_default_writepolicy(CitrusleafWritePolicy::ONCE);
    print "set_default_writepolicy result [$result] <br>\n";
    $result = $cl->get_default_writepolicy();
    print "get_default_writepolicy result [$result] <br>\n";

    $result = $cl->get_default_timeout();
    print "get_default_timeout result [$result] <br>\n";
    $result = $cl->set_default_timeout(5000);
    print "set_default_timeout result [$result] <br>\n";
    $result = $cl->get_default_timeout();
    print "get_default_timeout result [$result] <br>\n";

?>

