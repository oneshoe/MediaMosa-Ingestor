<?php

/**
 * @file
 * Classes of mediasite ingestor module.
 */

class MediasiteIngestor extends Ingestor {

  /**
   * Process a mediasite export zip file into a mediamosa ready item.
   */
  public function parse() {
    watchdog('Ingestor', 'Job @jobId: Start parsing and extracting of @file', array(
      '@jobId' => $this->jobId,
      '@file' => $this->sourceFile->uri,
    ), WATCHDOG_INFO);
    $zip = new ZipArchive();
    $zip_file = $zip->open(drupal_realpath($this->sourceFile->uri));
    if ($zip_file === TRUE) {
      $entries = self::getIndexFromZip($zip, 'MediasitePresentation_40.xml');

      $manifest = new SimpleXMLElement($zip->getFromName(reset($entries)));

      $item = new stdClass();
      $item->sourceDir = variable_get('ingestor_work_dir', self::DEFAULT_WORK_DIR) . $this->jobId;

      // Metadata
      // @TODO: All examples had these entries in them. They may or may not be present in the real world, and there may be others.
      $metadata = array();
      if (isset($manifest->Properties->PresentationId)) {
        $metadata['id'] = (string)$manifest->Properties->PresentationId;
      }
      if (isset($manifest->Properties->Title)) {
        $metadata['title'] = (string)$manifest->Properties->Title;
      }
      if (isset($manifest->Properties->Start)) {
        $metadata['created'] = date('Y-m-d H:i:s', strtotime((string)$manifest->Properties->Start));
      }
      if (!empty($metadata)) {
        $item->metaData = $metadata;
      }


      // Video
      $entries = self::getIndexFromZip($zip, (string)$manifest->MediaMetaData->ReplayMediaFileName);
      $filename = reset($entries);
      $video_dir = dirname($filename);
      if ($zip->extractTo($item->sourceDir, $filename)) {
        $item->videoSource = $filename;
      }
      else {
        watchdog('Ingestor', 'Job @jobId: Unable to extract @video from @archive due to zip error: @error', array(
          '@jobId' => $this->jobId,
          '@video' => $filename,
          '@archive' => $zip->filename,
          '@error' => ingestor_zip_error($zip->status),
        ), WATCHDOG_WARNING);
      }

      // Slides
      foreach ($manifest->SlideList->Slide as $slide) {
        $filename = $video_dir . '/' . (string)$slide->FileName;
        // Extract the image to the working directory
        if ($zip->extractTo($item->sourceDir, $filename)) {
          // Store some info
          $slide = (object)array(
            'filename' => basename($filename),
            'timestamp' => (string)$slide->PresentationTime,
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
      if (!empty($item->slides)) {
        $item->slideSource = $video_dir;
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
