Server

*NOTE* PHP is not suitable for running actual servers. This script is just a
quick and dirty proof of concept and should never be used for anything
important!

Edit server.php to specify desired port number, ingestor location and local
file storage:

$port = 9000;
$ingestor = 'http://localhost/ingestor/upload/';
$file_path = '/var/tmp/p2g';

Note that the $ingestor setting must point to http://your-drupal-install/
followed by 'ingestor/upload/' (note the trailing slash). The path in
$file_path must exist, and must be writable by PHP.

On the machine that should act as communication endpoint for the P2G capture
client run: /path/to/php server.php

It should print that it's listening and after that anything that goes on.
Alternatively run it in the background by using:
nohup php server.php &
the output will then be written to nohup.out

On the Presenstation 2Go Encoder (capture client) enable "Advanced mode". Then
go to "Publish->Server" and fill in the "Encoder Service Address & Port". Use
the hostname of the machine running server.php and the port specified in
server.php. In "General->Project Info" enable the "Publish When Finished"
option.

Now make a recording as usual.
When the recording is stopped the client should start sending files to the
server which should start outputting information. On the ingestor a queue item
should show up that can be manually processed or with cron. That ingestor job
uploads files to MediaMosa and starts transcoding jobs.
