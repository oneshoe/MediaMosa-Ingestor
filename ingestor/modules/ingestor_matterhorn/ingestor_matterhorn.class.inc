<?php

/**
 * @file
 * Classes of matterhorn ingestor module.
 */

class MatterhornIngestor extends Ingestor {

  /**
   * Download a file.
   * @param string $uri The full url of the file to download.
   * @param string $destination The file location to download to.
   */
  private static function downloadFile($uri, $destination) {
    watchdog('Ingestor', 'Starting download of @uri to @destination', array(
      '@uri' => $uri,
      '@destination' => $destination,
    ), WATCHDOG_INFO);
    $filename = basename($uri);
    $fp = fopen($destination . '/' . $filename , 'w');
    $ch = curl_init($uri);
    curl_setopt($ch, CURLOPT_FILE, $fp);
    curl_setopt($ch, CURLOPT_TIMEOUT, 300);  // No longer than 5 minutes.
    curl_exec($ch);
    $status = curl_error($ch);
    curl_close($ch);
    fclose($fp);
    watchdog('Ingestor', 'Completed download of @uri to @destination: @status', array(
      '@uri' => $uri,
      '@destination' => $destination,
      '@status' => $status,
    ), WATCHDOG_INFO);
    return array($status, $filename);
  }

  /**
   * Process a matterhorn atom feed file into one or more mediamosa ready items.
   */
  public function parse() {
    watchdog('Ingestor', 'Job @jobId: Start parsing of @file', array(
      '@jobId' => $this->jobId,
      '@file' => $this->sourceFile->uri,
    ), WATCHDOG_INFO);

    // This should always succeeed because the file was already checked by ingestor_matterhorn_ingestor_compatible_item()
    $feed = new SimpleXMLElement($this->sourceFile->uri, NULL, TRUE);

    foreach ($feed->entry as $entry) {
      $item = new stdClass();
      $item->sourceDir = variable_get('ingestor_work_dir', self::DEFAULT_WORK_DIR) . $this->jobId;
      // Download videos from the links.
      foreach ($entry->link as $link) {
        switch ((string)$link['type']) {
          case 'video/avi':  // @TODO: This was the case in all examples. In real life there may or may not be other types.
            list($status, $filename) = self::downloadFile((string)$link['href'], $item->sourceDir);
            if (!empty($status)) {
              watchdog('Ingestor', 'Download of @uri to @destination failed: @status', array(
                '@uri' => (string)$link['href'],
                '@destination' => $item->sourceDir,
                '@status' => $status,
              ), WATCHDOG_WARNING);
            }
            if (!isset($item->videoSource)) {
              $item->videoSource = $filename;
            }
            else {
              $item->slideSource = $filename;
            }
            break;
        }
      }
      // Apply metadata.
      // This will take all dublin core elements from the xml and take them as metadata.
      // Mediamosa may complain when it decides it doesn't like a certain value.
      $metadata = array();
      foreach ($entry->children('dc', TRUE) as $term) {
        $metadata[$term->getName()] = (string)$term;
      }
      if (!empty($metadata)) {
        $item->metaData = $metadata;
      }
      $this->mediamosaItem[] = $item;
    }
    watchdog('Ingestor', 'Job @jobId: Parsing @status.', array(
      '@jobId' => $this->jobId,
      '@status' => ($this->status == self::STATUS_FAILED) ? 'failed' : 'succeeded',
    ), WATCHDOG_INFO);
    return $this;
  }

  /**
   * Handle all the mediamosa shizzle.
   */
  public function upload() {
    // Do custom handling here if it's an array of items.
    if (is_array($this->mediamosaItem)) {
      $this->status = self::STATUS_WORKING;
      $mediamosa_items = $this->mediamosaItem;
      foreach ($mediamosa_items as $mediamosa_item) {
        $this->mediamosaItem = $mediamosa_item;
        if (!empty($this->mediamosaItem->metaData) || !empty($this->mediamosaItem->videoSource) || !empty($this->mediamosaItem->slideSource)) {
          // Workflow.
          $this->createAsset();
          if ($this->status !== self::STATUS_FAILED && !empty($this->mediamosaItem->videoSource)) {
            $this->uploadVideo();
          }
          if ($this->status !== self::STATUS_FAILED && !empty($this->mediamosaItem->metaData)) {
            $this->applyMetaData();
          }
          if ($this->status !== self::STATUS_FAILED && !empty($this->mediamosaItem->slideSource)) {
            $this->uploadSlides();
          }
        }
        else {
          watchdog('Ingestor', 'Job @jobId: No mediamosaitem data, skipping upload.', array(
            '@jobId' => $this->jobId,
          ), WATCHDOG_INFO);
        }
      }
    }
    else {
      // Let the normal function handle it if it's a single item.
      parent::upload();
    }

    // Done.
    if ($this->status !== self::STATUS_FAILED) {
      $this->status = self::STATUS_SUCCESS;
      // Cleanup.
      file_delete($this->sourceFile);
      $directory = variable_get('ingestor_work_dir', self::DEFAULT_WORK_DIR) . $this->jobId;
      self::rmdirRecusive(drupal_realpath($directory));
    }
  }

}
