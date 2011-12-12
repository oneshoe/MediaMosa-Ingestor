MediaMosa Ingestor
===============================================================================

MediaMosa Ingestor is a Drupal module (Drupal 7) which reads exports/feeds from
Presentations2Go (P2G), Matterhorn- and MediaSite recording clients and uploads
files contained in those lecture recordings to MediaMosa with processing
instructions.

A lecture recording is often more than just a single video. The Ingestor
project uses the information (metadata, slide information, timecodes) embedded
in the recordings and uses it to upload files to MediaMosa and initiate the
necessary transcoding operations.

Presentations2Go is Windows application for recording lectures using camera and
screen capture for detecting slides from a beamer screen. Usually the capture
clients send their lecture recordings to the P2G server for publishing; the
Ingestor project has one proof-of-concept (prototype) which acts as a P2G
server for receiving finished recordings for uploading those to a MediaMosa
backend for transcoding into several formats. Another P2G component provided by
Ingestor is a module which scans configured directories for finished lecture
recordings, and ingests them for uploading to- and processing by MediaMosa.
More information on Presentations 2 Go: http://www.presentations2go.eu/

Matterhorn is an open-source initiative, part of Open Cast, which is intends to
provide educational institutions with an open source alternative for lecture
recording. Matterhorn capture clients usually export zip files, but little is
known about all the different capture clients, as there are many formats
around. Contained in the Ingestor project is a Matterhorn sub-module which
processes the Atom feeds as they are available from the Matterhorn server.
These feed XML-files need to be uploaded to the Ingestor manually, or by
implementing a method which uploads it to the 'ingestor/upload' path.
More information on Matterhorn: http://opencast.org/matterhorn/

MediaSite is a product from Sonic Foundry. The support for MediaSite in the
Ingestor project is based on exports from the MediaSite server. These zip-files
can be uploaded to the Ingestor for processing. All information on slides and
most video metadata will be included in the MediaMosa asset once uploading is
finished.
More information on MediaSite: http://www.sonicfoundry.com/mediasite

===============================================================================

Requirements:
- A Drupal 7 installation. (http://drupal.org/start)
- The mediamosa_connector module from the mediamosa_sdk
  (https://github.com/mediamosa/mediamosa-sdk/zipball/7.x-1.0).
- Valid login details for a MediaMosa server.

Installation:
- Copy the Ingestor folder to your Drupal installation in sites/all/modules/ or
  one of its sub-folders.
- Log in as administrator to your Drupal installation.
- Navigate to http://yoursite/admin/config/media/mediamosa/connector and enter
  your MediaMosa server connection details.
- Go to http://yoursite/admin/modules and enable the Ingestor module and all
  the Ingestor modules you plan to use.
- Navigate to http://yoursite/admin/config/media/ingestor and enter the
  MediaMosa user ID to use.
- If you change the working directory you have to make sure it is writable by
  the webserver.
- On the same page you have to configure which transcoding profiles have to be
  used by MediaMosa on the uploaded content.

Usage:
- Go to http://yoursite/ingestor
- Upload a zip file with a presentation in it or an atom xml file referencing
  content to upload.
- Every time cron runs (http://drupal.org/cron) an item from the queue will be
  processed.
- On the Ingestor page you can also start processing of the queue manually.

Logging:
- Information is logged as type 'ingestor' at
  http://yoursite/admin/reports/dblog

===============================================================================

This project was initiated by the University of Groningen (http://www.rug.nl),
funded by a programme from SURFnet (http://www.surfnet.nl) for MediaMosa and
developed by One Shoe (http://www.oneshoe.nl). More information on MediaMosa
can be found at: http://www.mediamosa.org

You can use this module in tandem with a (separate) front-end application based
on Drupal 7 using the MediaMosa-Player
(https://github.com/oneshoe/MediaMosa-Player) for web lectures.

===============================================================================
Future development of this functionality is intended to provide a more stable,
robust and fault-tolerant version of this functionality. The best-case-scenario
would be full support for the output of the client recorders, directly to
MediaMosa, without the need for other server components.
