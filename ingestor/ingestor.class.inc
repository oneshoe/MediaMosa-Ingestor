<?php
// $Id: ingestor.class.inc 248 2011-11-25 14:10:03Z joris.vaneijden $

/**
 * @file
 * Classes of ingestor module.
 */

class Ingestor {

  const STATUS_FAILED = 0;
  const STATUS_SUCCESS = 1;
  const STATUS_WORKING = 2;
  const DEFAULT_WORK_DIR  = 'temporary://ingestor_jobs/';

  protected $sourceFile;  // the uploaded file to work with
  public $status;
  protected $jobId;  // int identifying the item in the queue
  protected $mediamosaItem;  // object used to keep track of info on a job
  protected $userId;  // array used to identify with mediamosa.

  public function __construct($item) {
    watchdog('Ingestor', 'Job @jobId: Creating new @class.', array(
      '@jobId' => $item->item_id,
      '@class' => get_class($this),
    ), WATCHDOG_INFO);
    $this->userId = array('user_id' => variable_get('ingestor_user_id', 'ingestor'));
    $this->status = self::STATUS_WORKING;
    $this->jobId = $item->item_id;
    $this->prepareFile($item->data);
  }

  /**
   * Get or create mediamosa connector
   * @param bool $reset_session
   */
  static public function get_connector($reset_session = FALSE) {
    $mediamosa_connector = &drupal_static(__CLASS__ . '::' . __FUNCTION__, FALSE);

    // Create when not found.
    if (empty($mediamosa_connector)) {
      $mediamosa_connector = new mediamosa_connector();
    }

    // Reset session when needed.
    if ($reset_session) {
      $mediamosa_connector->reset_session();
    }

    return $mediamosa_connector;
  }

  static public function get_profiles() {
    $connector = self::get_connector();

    $result = $connector->request_get('transcode/profile');
    $profiles = array();
    foreach ($result->xml->items->item as $item) {
      $profiles[(string)$item->profile_id] = (string)$item->profile;
    }
    return $profiles;
  }

  /**
   * Create a working directoy and move the source file to it
   * @param object $file Source file.
   */
  protected function prepareFile($file) {
    // Get the source file and move it to a directory.
    $directory = variable_get('ingestor_work_dir', self::DEFAULT_WORK_DIR) . $this->jobId;
    if (file_prepare_directory($directory, FILE_CREATE_DIRECTORY | FILE_MODIFY_PERMISSIONS)) {
      $this->sourceFile = file_move($file, $directory, FILE_EXISTS_REPLACE);
      if (!$this->sourceFile) {
        watchdog('Ingestor', 'Job @jobId: Failed to move file @file to directory @dir', array(
          '@jobId' => $this->jobId,
          '@file' => $file,
          '@dir' => $directory,
        ), WATCHDOG_ERROR);
        $this->status = self::STATUS_FAILED;
      }
    }
    else {
      watchdog('Ingestor', 'Job @jobId: Failed to create directory @dir', array(
        '@jobId' => $this->jobId,
        '@dir' => $directory,
      ), WATCHDOG_ERROR);
      $this->status = self::STATUS_FAILED;
    }
    return $this;
  }

  /**
   * Get all indices and entry names from a zip file where the filenames match a pattern.
   * @param resource $zip zip archive
   * @param string $pattern [optional] regex pattern, defauilts to all
   */
  public static function getIndexFromZip($zip, $pattern = '.*') {
    $entries = array();
    for ($index = 0; $index < $zip->numFiles; $index++) {
      $filename = $zip->getNameIndex($index);
      if (preg_match('@' . $pattern . '@', basename($filename))) {
        $entries[$index] = $filename;
      }
    }
    return $entries;
  }

  /**
   * Create an instance of a specific ingestor.
   * @param string $class name of ingestor class
   * @param object $item ingestor job item
   */
  public static function get($class, $item = NULL) {
    $object = new $class($item);
    if (!$object instanceof Ingestor) {
      $object = new Ingestor();
    }
    return $object;
  }

  /**
   * Handle all the mediamosa shizzle.
   */
  public function upload() {
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
    // Done.
    if ($this->status !== self::STATUS_FAILED) {
      $this->status = self::STATUS_SUCCESS;
      // Cleanup.
      file_delete($this->sourceFile);
      $directory = variable_get('ingestor_work_dir', self::DEFAULT_WORK_DIR) . $this->jobId;
      self::rmdirRecusive(drupal_realpath($directory));
    }
  }

  /**
   * Recursively delete files and directories.
   * @param string $dir Directory to delete.
   */
  protected static function rmdirRecusive($dir) {
    if (empty($dir)) {
      return FALSE;
    }
    if (drupal_substr($dir, -1) != '/') {
      $dir .= '/';
    }
    $files = glob($dir . '*', GLOB_MARK);

    foreach ($files as $file) {
      if (drupal_substr($file, -1) == '/') {
        self::rmdirRecusive($file);
      }
      else {
        unlink($file);
      }
    }
    if (is_dir($dir)) {
      rmdir($dir);
    }
  }

  /**
   * Transfer metadata to the asset in mediamosa.
   * Warning: if any one of the metadata values does not meet mediamosa's strict standards then the call fails.
   * For example the matterhorn atom feed has <dc:language>"en"</dc:language> which is rejected.
   */
  public function applyMetaData() {
    watchdog('Ingestor', 'Job @jobId: Starting application of metadata.', array(
      '@jobId' => $this->jobId,
    ), WATCHDOG_INFO);
    $connector = self::get_connector();
    if (isset($this->mediamosaItem->assetId) && isset($this->mediamosaItem->metaData)) {
      $options = array_merge($this->userId, array('data' => $this->mediamosaItem->metaData));
      $response = $connector->request_post('asset/' . $this->mediamosaItem->assetId . '/metadata', $options);
      if (!self::checkResponse($response)) {
        $this->status = self::STATUS_FAILED;
      }
    }
    watchdog('Ingestor', 'Job @jobId: Application of metadata @status.', array(
      '@jobId' => $this->jobId,
      '@status' => ($this->status == self::STATUS_FAILED) ? 'failed' : 'succeeded',
    ), WATCHDOG_INFO);
    return $this;
  }

  /**
   * Check if the request to mediamosa succeeded and log the message if it didn't.
   * @param object $response
   */
  public static function checkResponse($response) {
    if ((string)$response->xml->header->request_result_id != '601') {
      watchdog('Ingestor', 'MediaMosa error @nr: %error', array(
        '@nr' => (string)$response->xml->header->request_result_id,
        '%error' => (string)$response->xml->header->request_result_description,
      ), WATCHDOG_ERROR);
      return FALSE;
    }
    return TRUE;
  }

  /**
   * Transfer stills or a video for stills to the asset in mediamosa.
   */
  public function uploadSlides() {
    watchdog('Ingestor', 'Job @jobId: Starting upload of slides.', array(
      '@jobId' => $this->jobId,
    ), WATCHDOG_INFO);
    $connector = self::get_connector();
    if (isset($this->mediamosaItem->assetId) && isset($this->mediamosaItem->slideSource)) {

      // Ensure there is a mediafile id
      if (!isset($this->mediamosaItem->mediaIds['slideSource'])) {
        // No slide mediafile, create a new one
        $options = array_merge($this->userId, array(
          'data' => array(
            'asset_id' => $this->mediamosaItem->assetId,
            'is_downloadable' => 'TRUE',
            'transcode_inherits_acl' => 'TRUE',
            'tag' => 'slideSource',
          ),
        ));
        $response = $connector->request_post('mediafile/create', $options);
        if (!self::checkResponse($response)) {
          $this->status = self::STATUS_FAILED;
        }
        if ($mediafile_id = (string) $response->xml->items->item->mediafile_id) {
          $this->mediamosaItem->mediaIds['slideSource'] = $mediafile_id;
        }
        else {
          watchdog('Ingestor', 'Job @jobId: Failed to create mediafile for asset @asset', array(
            '@jobId' => $this->jobId,
            '@asset' => $this->mediamosaItem->assetId,
          ), WATCHDOG_ERROR);
          $this->status = self::STATUS_FAILED;
        }
      }

      // Upload slide files if there are any.
      // In this case the slideSource is a directory.
      if (!empty($this->mediamosaItem->slides)) {
        $dir = $this->mediamosaItem->sourceDir . DIRECTORY_SEPARATOR . $this->mediamosaItem->slideSource . DIRECTORY_SEPARATOR;
        if (isset($this->mediamosaItem->mediaIds['slideSource'])) {
          // Loop through all the slides
          foreach ($this->mediamosaItem->slides as $slide) {
            // Create upload ticket.
            $response = $connector->request_post('mediafile/' . $this->mediamosaItem->mediaIds['slideSource'] . '/uploadticket/create', $this->userId);
            if (!self::checkResponse($response)) {
              $this->status = self::STATUS_FAILED;
            }
            if ($ticket = (string) $response->xml->items->item->ticket_id) {
              // Set the data to transfer.
              $options = array(
                'upload_ticket' => $ticket,
                'mediafile_id' => $this->mediamosaItem->mediaIds['slideSource'],
                'still_time_code' => $slide->timestamp,  // is not picked up and even if it worked only uses (int)seconds
                'order' => $slide->timestamp,  // abusing this field to store the slide time in milliseconds
              );
              // Perform upload.
              $this->transferFile($dir . $slide->filename, $options, '/asset/' . $this->mediamosaItem->assetId . '/still/upload');
            }
            else {
              watchdog('Ingestor', 'Job @jobId: Failed to create uploadticket for mediafile @mediafile', array(
                '@jobId' => $this->jobId,
                '@mediafile' => $this->mediamosaItem->mediaIds['slideSource'],
              ), WATCHDOG_ERROR);
              $this->status = self::STATUS_FAILED;
            }
          }
        }
      }
      else {
        // Make slides from a video
        // In this case the slideSource is a video file.
        if (isset($this->mediamosaItem->mediaIds['slideSource'])) {
          $response = $connector->request_post('mediafile/' . $this->mediamosaItem->mediaIds['slideSource'] . '/uploadticket/create', $this->userId);
          if (!self::checkResponse($response)) {
            $this->status = self::STATUS_FAILED;
          }
          if ($ticket = (string) $response->xml->items->item->ticket_id) {
            $options = array(
              'upload_ticket' => $ticket,
              'create_still' => 'TRUE',
              'still_type' => 'SCENE',
              'tag' => 'slideSource',
            );
            $this->transferFile($this->mediamosaItem->sourceDir . DIRECTORY_SEPARATOR . $this->mediamosaItem->slideSource, $options);
          }
          else {
            watchdog('Ingestor', 'Job @jobId: Failed to create uploadticket for mediafile @mediafile', array(
              '@jobId' => $this->jobId,
              '@mediafile' => $this->mediamosaItem->mediaIds['slideSource'],
            ), WATCHDOG_ERROR);
            $this->status = self::STATUS_FAILED;
          }
        }
      }
    }
    watchdog('Ingestor', 'Job @jobId: Upload of slides @status.', array(
      '@jobId' => $this->jobId,
      '@status' => ($this->status == self::STATUS_FAILED) ? 'failed' : 'succeeded',
    ), WATCHDOG_INFO);
    return $this;
  }

  /**
   * Upload a video to mediamosa.
   * @TODO: de-duplicate code between this and uploadSlides()
   */
  public function uploadVideo() {
    watchdog('Ingestor', 'Job @jobId: Starting upload of video.', array(
      '@jobId' => $this->jobId,
    ), WATCHDOG_INFO);
    $connector = self::get_connector();
    if (!$this->mediamosaItem->assetId) {
      $this->createAsset();
    }
    if (isset($this->mediamosaItem->assetId) && isset($this->mediamosaItem->videoSource)) {
      $options = array_merge($this->userId, array(
        'data' => array(
          'asset_id' => $this->mediamosaItem->assetId,
          'is_downloadable' => 'TRUE',
          'transcode_inherits_acl' => 'TRUE',
        ),
      ));
      $response = $connector->request_post('mediafile/create', $options);
      if (!self::checkResponse($response)) {
        $this->status = self::STATUS_FAILED;
      }
      if ($mediafile_id = (string) $response->xml->items->item->mediafile_id) {
        $this->mediamosaItem->mediaIds['videoSource'] = $mediafile_id;
        $response = $connector->request_post('mediafile/' . $mediafile_id . '/uploadticket/create', $this->userId);
        if (!self::checkResponse($response)) {
          $this->status = self::STATUS_FAILED;
        }
        if ($ticket = (string) $response->xml->items->item->ticket_id) {
          $options = array(
            'upload_ticket' => $ticket,
            'create_still' => 'TRUE',
            'still_type' => 'NORMAL',
            'still_per_mediafile' => 1,
            'tag' => 'videoSource',
          );
          // Apply optional transcode profiles to the request
          $transcode_profiles = array_keys(array_filter(variable_get('ingestor_' . get_class($this) . '_profiles', array())));
          if (!empty($transcode_profiles)) {
            for ($index = 0; $index < count($transcode_profiles); $index++) {
              $options['transcode[' . $index . ']'] = $transcode_profiles[$index];
            }
          }
          // Upload.
          $this->transferFile($this->mediamosaItem->sourceDir . '/' . $this->mediamosaItem->videoSource, $options);
        }
        else {
          watchdog('Ingestor', 'Job @jobId: Failed to create uploadticket for mediafile @mediafile', array(
            '@jobId' => $this->jobId,
            '@mediafile' => $mediafile_id,
          ), WATCHDOG_ERROR);
          $this->status = self::STATUS_FAILED;
        }
      }
      else {
        watchdog('Ingestor', 'Job @jobId: Failed to create mediafile for asset @asset', array(
          '@jobId' => $this->jobId,
          '@asset' => $this->mediamosaItem->assetId,
        ), WATCHDOG_ERROR);
        $this->status = self::STATUS_FAILED;
      }
    }
    watchdog('Ingestor', 'Job @jobId: Uploading video @status.', array(
      '@jobId' => $this->jobId,
      '@status' => ($this->status == self::STATUS_FAILED) ? 'failed' : 'succeeded',
    ), WATCHDOG_INFO);
    return $this;
  }

  /**
   * Transfers a file to mediamosa.
   * @param string $filename
   * @param array $options [optional] Options to send along with the file upload.
   * @param string $rest_path [optional] Rest call path. defaults to /mediafile/upload
   */
  protected function transferFile($filename, $options = array(), $rest_path = '/mediafile/upload') {
    $url = variable_get('mediamosa_connector_url', NULL);
    $cookies = variable_get('mediamosa_connector_cookies', array());
    $cookie = $cookies[$url][variable_get('mediamosa_connector_username', NULL)][variable_get('mediamosa_connector_password', NULL)];
    $postdata = array_merge($this->userId, array('file' => '@' . drupal_realpath($filename)), $options);

    $curl = curl_init();
    curl_setopt($curl, CURLOPT_URL, $url . $rest_path);
    curl_setopt($curl, CURLOPT_CONNECTTIMEOUT, 2);
    curl_setopt($curl, CURLOPT_RETURNTRANSFER, TRUE);
    curl_setopt($curl, CURLOPT_POST, TRUE);
    curl_setopt($curl, CURLOPT_COOKIE, $cookie);
    curl_setopt($curl, CURLOPT_POSTFIELDS, $postdata);
    $result = curl_exec($curl);
    if (!$result) {
      watchdog('Ingestor', 'Job @jobId: Failed to upload file @file to mediamosa url @url: @error', array(
        '@jobId' => $this->jobId,
        '@file' => $filename,
        '@url' => $url . $rest_path,
        '@error' => curl_error($curl),
      ), WATCHDOG_ERROR);
      $this->status = self::STATUS_FAILED;
    }
    curl_close($curl);
    return $this;
  }

  /**
   * Create an asset at mediamosa.
   */
  public function createAsset() {
    $connector = self::get_connector();
    // Create asset
    $response = $connector->request_post('asset/create', $this->userId);
    if (!self::checkResponse($response)) {
      $this->status = self::STATUS_FAILED;
    }
    if ($asset_id = (string) $response->xml->items->item->asset_id) {
      $this->mediamosaItem->assetId = $asset_id;
      watchdog('Ingestor', 'Job @jobId: Created MediaMosa asset: @asset_id', array(
        '@jobId' => $this->jobId,
        '@asset_id' => $asset_id,
      ), WATCHDOG_INFO);
    }
    else {
      watchdog('Ingestor', 'Job @jobId: Failed to create asset.', array(
        '@jobId' => $this->jobId,
      ), WATCHDOG_ERROR);
      $this->status = self::STATUS_FAILED;
    }
    return $this;
  }

}
