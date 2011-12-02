<?php
// $Id$

/**
 * @file
 * Classes of ingestor module.
 */

class IngestorFolder {

  const FOLDER_UNKNOWN = 0;
  const FOLDER_DIR = 1;
  const FOLDER_ZIP = 2;
  const FOLDER_XML = 3; // for possible future use

  public $folderType;
  protected $foldername;
  protected $zip;
  protected $dir;
  protected $xml;

  public function __construct($filename) {
    if (is_dir($filename)) {
      $this->dir = dir($filename);
      if ($this->dir !== FALSE) {
        $this->$folderType = self::FOLDER_DIR;
      }
    }
    else {
      $extension = pathinfo($filename, PATHINFO_EXTENSION);
      switch ($extension) {
        case 'zip':
          $this->zip = new ZipArchive();
          $status = $this->zip->open($filename);
          if ($status === TRUE) {
            $this->$folderType = self::FOLDER_ZIP;
          }
          else {
            watchdog('Ingestor', 'IngestorFolder: Unable to open @file due to zip error: @error', array(
              '@file' => $filename,
              '@error' => ingestor_zip_error($status),
            ), WATCHDOG_WARNING);
          }
          break;
        case 'xml':
          $this->$folderType = self::FOLDER_XML;
          break;
        default:
          $this->$folderType = self::FOLDER_UNKNOWN;
          break;
      }
    }
    $this->foldername = $filename;
  }

  public function getFromName($filename) {
    switch ($this->folderType) {
      case self::FOLDER_DIR:
        $filepath = $this->foldername . $filename;
        if (file_exists($filepath)) {
          return file_get_contents($filepath);
        }
        break;
      case self::FOLDER_ZIP:
        return $this->zip->getFromName($filename);
        break;
      case self::FOLDER_XML:
        break;
      default:
        break;
    }
  }

}

