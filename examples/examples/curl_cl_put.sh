#!/bin/sh
bin="bin"
value="value"
for i in p {0..10000}
do
	key=$i
	bin=$bin
	value=$i
	curl --data "key=$key&binName1=$bin&value1=$value" http://localhost/examples/cl_put.php
done
