<?php
// $Id: server.php 240 2011-11-24 12:47:01Z thijs $

/**
 * @file
 * Proof-of-concept of 'server' script posing as Presentations2Go server instance.
 * NOTE! PHP is not suitable for running actual servers. This script is just a
 * quick and dirty proof of concept and should never be used for anything
 * important!
 */
print 'Presentations2Go faked server' . PHP_EOL;

/**
 * Configuration.
 */
$port = 9000;
$ingestor = 'http://mediamosa.tiny/ingestor/upload/';
$file_path = '/tmp/p2g-ingestor';

// Checking the file path.
if (!is_dir($file_path)) {
  trigger_error('Path ' . $file_path . ' does not exist or is not a directory.', E_USER_ERROR);
  die('Exit.');
}

if (!is_writable($file_path)) {
  trigger_error('Directory ' . $file_path . ' is not writable.', E_USER_ERROR);
  die('Exit.');
}

// Initiate listening on port $port.
$socket = socket_create_listen($port);
print 'Listening on port ' . $port . PHP_EOL;

// Endless loop
while (true) {

  $data = '';
  // Wait for client connection.
  $client = socket_accept($socket);
  // Find out what it has to say.
  $data = socket_read($client, 10240) . PHP_EOL;
  // Fix wrong encoding identifier
  $data = str_replace('utf-16', 'utf-8', $data);
  $xml = simplexml_load_string($data);
  // What command is being sent?
  $command = $xml->children()->getName();

  print 'Command = ' . $command . PHP_EOL;

  switch ($command) {

    // This is used by the client to check if the server is reachable and support the requesting encoding.
    case 'checkstatus':
      // Tell the client I'm okay.
      $message = 'IAmOk';
      socket_write($client, $message, strlen($message));
      break;

    // This is used by the client to tell the server to prepare for a new recording and expects the folder path back.
    case 'newarchivedrecording':
      // Tell the client what path I want to use.
      $message = $file_path;
      socket_write($client, $message, strlen($message));
      break;

    // This is used by the client to tell the server it has a file to upload.
    case 'sendfile':
      // Tell the client I can receive their file.
      $message = 'IAmReadyToReceiveAFile';
      socket_write($client, $message, strlen($message));
      // Get more info about the file.
      $filename = (string) $xml->dumplocation;

      // Check the filename.
      if (substr($filename, 0, strlen($file_path)) == $file_path) {
        // Replace backslashes in the path to forward slashes.
        $filename = str_replace('\\', '/', $filename);

        $size = (int) $xml->filesize;
        // Create a directory if needed
        $directory = dirname($filename);
        if (!is_dir($directory)) {
          mkdir($directory, 0777, TRUE);
        }

        $data = '';
        print "Trying to receive $size bytes of $filename" . PHP_EOL;

        do {
          // Reading data in 4K chunks.
          $buffer = socket_read($client, 4096);
          if ($buffer === FALSE) {
            print 'Error: ' . socket_strerror(socket_last_error($client)) . PHP_EOL;
          }
          else {
            $data .= $buffer;
          }
        } while (strlen($data) < $size || socket_last_error($client));

        // Dump the file to disk.
        file_put_contents($filename, $data);

        $data = '';
      }
      else {
        print "File $filename skipped because path check failed." . PHP_EOL;
      }

      // Check for the xml file which seems to be the last always.
      if (basename($filename) == 'recordingdetails.xml') {
        $destination = $file_path . DIRECTORY_SEPARATOR . 'presentation.zip';
        $ok = zip($file_path, $destination);
        if ($ok === TRUE) {
          print 'Trying to upload to ingestor' . PHP_EOL;
          // upload zip to ingestor
          $id = 'p2g-server';
          $postdata = array('files[' . $id . ']' => '@' . $destination);
          $curl = curl_init();
          curl_setopt($curl, CURLOPT_URL, $ingestor . $id);
          curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 2);
          curl_setopt($curl, CURLOPT_RETURNTRANSFER, TRUE);
          curl_setopt($curl, CURLOPT_POST, TRUE);
          //curl_setopt($curl, CURLOPT_COOKIE, $cookie);
          curl_setopt($curl, CURLOPT_POSTFIELDS, $postdata);
          $result = curl_exec($curl);
          if ($result) {
            print $result . PHP_EOL;
          }
          else {
            print curl_error($curl). PHP_EOL;
          }
          curl_close($curl);
        }
      }
      break;
    default:
      break;
  }

  // Finish request
  socket_close($client);
}

/**
 * Create a zipfile from a directory.
 * @param string $source Directory
 * @param string $destination Zipfile
 */
function zip($source, $destination) {
  if (!extension_loaded('zip') || !file_exists($source)) {
    return FALSE;
  }
  $zip = new ZipArchive();
  if (! $zip->open($destination, ZIPARCHIVE::CREATE)) {
    return FALSE;
  }

  $source = str_replace('\\', '/', realpath($source));
  if (is_dir($source) === TRUE) {
    $files = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($source), RecursiveIteratorIterator::SELF_FIRST);
    foreach ($files as $file) {
      $file = str_replace('\\', '/', realpath($file));

      if (is_dir($file) === TRUE) {
        $zip->addEmptyDir(str_replace($source . '/', '', $file . '/'));
      }
      elseif (is_file($file) === TRUE) {
        $zip->addFromString(str_replace($source . '/', '', $file), file_get_contents($file));
      }
    }
  }
  elseif (is_file($source) === TRUE) {
    $zip->addFromString(basename($source), file_get_contents($source));
  }
  return $zip->close();
}
