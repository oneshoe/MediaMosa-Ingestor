<?php

/**
 * @file
 * Classes of p2g ingestor module.
 */

class P2gIngestor extends Ingestor {

  /**
   * Process a p2g export zip file into a mediamosa ready item.
   * @TODO: parse the archive recusively for files?
   */
  public function parse() {
    watchdog('Ingestor', 'Job @jobId: Start parsing and extracting of @file', array(
      '@jobId' => $this->jobId,
      '@file' => $this->sourceFile->uri,
    ), WATCHDOG_INFO);
    $zip = new ZipArchive();
    $zip_file = $zip->open(drupal_realpath($this->sourceFile->uri));
    if ($zip_file === TRUE) {
      $record = new SimpleXMLElement($zip->getFromName('recordingdetails.xml'));

      $item = new stdClass();
      $item->sourceDir = variable_get('ingestor_work_dir', self::DEFAULT_WORK_DIR) . $this->jobId;

      // Metadata
      // @TODO: All examples had these entries in them. They may or may not be present in the real world, and there may be others.
      // @TODO: Add multilamnguage support. If there are multiple languages, only the first is used.
      $metadata = array();
      if (isset($record->header->identifier)) {
        $metadata['id'] = (string)$record->header->identifier;
      }
      if (isset($record->custom->accessibility->language)) {
        $metadata['language'] = (string)$record->custom->accessibility->language;
      }
      if (isset($record->general->title->langstring->value)) {
        $metadata['title'] = (string)$record->general->title->langstring->value;
      }
      if (isset($record->general->description->langstring->value)) {
        $metadata['description'] = (string)$record->general->description->langstring->value;
      }
      if (!empty($metadata)) {
        $item->metaData = $metadata;
      }

      // Video
      // @TODO: In all example files the video file was called video_post.wmv, in the real world this may be different.
      if ($zip->extractTo($item->sourceDir, 'video_post.wmv')) {
        $item->videoSource = 'video_post.wmv';
      }
      else {
        watchdog('Ingestor', 'Job @jobId: Unable to extract video_post.wmv from @archive due to zip error: @error', array(
          '@jobId' => $this->jobId,
          '@archive' => $zip->filename,
          '@error' => ingestor_zip_error($zip->status),
        ), WATCHDOG_WARNING);
      }

      // Slides
      // @TODO: In all examples the slides were jpegs with the timestamp in the filename. In the real world there may be other cases.
      $slides = self::getIndexFromZip($zip, 'slide_\d+_\d+\.jpg');
      foreach ($slides as $index => $filename) {
        // Extract the image to the working directory
        if ($zip->extractTo($item->sourceDir, $filename)) {
          $item->slideSource = dirname($filename);
          // Get the timestamnp from the image filename.
          $matches = array();
          if (preg_match('/slide_(\d+)_(\d+)\.jpg/', $filename, $matches)) {
            // Get timestamp in milliseconds.
            $timestamp = intval($matches[1] . drupal_substr($matches[2], 0, 3));
          }
          else {
            $timestamp = -1;  // @TODO: may cause problems, but '0' definately will.
            watchdog('Ingestor', 'Job @jobId: Unable to determine timestamp of still @file in @archive', array(
              '@jobId' => $this->jobId,
              '@file' => $filename,
              '@archive' => $zip->filename,
            ), WATCHDOG_WARNING);
          }
          $slide = (object) array(
            'filename' => basename($filename),
            'timestamp' => $timestamp,
          );
          $item->slides[] = $slide;
        }
        else {
          watchdog('Ingestor', 'Job @jobId: Unable to extract @file from @archive due to zip error: @error', array(
            '@jobId' => $this->jobId,
            '@file' => $filename,
            '@archive' => $zip->filename,
            '@error' => ingestor_zip_error($zip->status),
          ), WATCHDOG_WARNING);
        }
      }
      $this->mediamosaItem = $item;
    }
    else {
      watchdog('Ingestor', 'Job @jobId: Unable to parse @file due to zip error: @error', array(
        '@jobId' => $this->jobId,
        '@file' => drupal_realpath($this->sourceFile->uri),
        '@error' => ingestor_zip_error($zip_file),
      ), WATCHDOG_ERROR);
      $this->status = self::STATUS_FAILED;
    }
    watchdog('Ingestor', 'Job @jobId: Parsing @status.', array(
      '@jobId' => $this->jobId,
      '@status' => ($this->status == self::STATUS_FAILED) ? 'failed' : 'succeeded',
    ), WATCHDOG_INFO);
    return $this;
  }
}
