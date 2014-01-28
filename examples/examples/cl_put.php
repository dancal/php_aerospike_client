<html>
 <head>
  <title>PHP Test</title>
 </head>
 <body>
<?php
require_once("cl_example_config.php");
require_once("../citrusleaf_enums.php");
$cl = new CitrusleafClient();
$url = "citrusleaf://".$citrusleaf_config["host"].":".$citrusleaf_config["port"];
$result = $cl->connect($url);
// check to make sure the result is ok
echo "<p>Connecting to citrusleaf...</p>";
if ($result == CitrusleafResult::CITRUSLEAF_OK) {
	echo "succeed!  ";
	echo "response code = $result";
} else {
	echo "failed with ".$result,"\n";
	return;
}
if(isset($_POST['key'])&&isset($_POST['binName1'])&&isset($_POST['value1'])) {

	$result = $cl->set_default_namespace($citrusleaf_config["namespace"]);
	$ns = $cl->get_default_namespace();
	echo "<p>namespace used = $ns </p>";

	// adding the requested row, plus an integer type
	$result = $cl->set_default_bin($_POST['binName1']);
	$result = $cl->put($_POST['key'],$_POST['value1']);
	echo "<p>data inserted into Citrusleaf: response code = $result </p>";

	// reading the key back
	$response = $cl->get($_POST['key']);

	// "get" functions return an actual object instead of just a response_code. Please see 
	echo "<p>Now reading the row ".$_POST['key']." back... </p>";

	// this should be checked against CitrusleafResult to check success or failure
	$code = $response->get_response_code();
	echo "response_code=[$code]<br>";

	// "genearation" can be used on writes to achieve optimistic locking 
	$generation = $response->get_generation();
	echo "generation=[$generation]<br>";

	$read_bin_vals = $response->get_bins();
	echo "bins in key: <br>";
	var_dump($read_bin_vals);
	$response->free_bins();
} else {
	echo "invalid key/value";
}
?>
    <br />
    <br />
    <a href="cl_putmany.php">Next</a>

 </body>
</html>
