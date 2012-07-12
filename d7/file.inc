<?php

/**
 * Handling specific to a Drupal 7 source for files.
 *
 * The following optional arguments may be passed:
 *
 * skip_pictures - Set to TRUE if the file migration should skip any file_managed
 *  entries being used as user pictures. You would do this if you are migrating
 *  user pictures in a separate step (DrupalPicture7Migration).
 */
class DrupalFile7Migration extends DrupalFileMigration {
  protected $skipPictures = FALSE;
  public function __construct(array $arguments) {
    if (isset($arguments['skip_pictures']) && $arguments['skip_pictures']) {
      $this->skipPictures = TRUE;
    }
    parent::__construct($arguments);
    $this->addFieldMapping('value', 'uri')
         ->callbacks(array($this, 'fixUri'));
    $this->addFieldMapping('destination_file', 'uri')
      ->callbacks(array($this, 'fixUri'));
  }

  protected function fileQuery() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('file_managed', 'f')
             ->fields('f')
             ->orderBy('timestamp');
    if ($this->skipPictures) {
      $query->leftJoin('users', 'u', 'f.fid=u.picture');
      $query->isNull('u.uid');
    }
    return $query;
  }

  protected function fixUri($uri) {
    $result = str_replace('public://', '', $uri);
    return $result;
  }
}

/**
 * Pull user pictures in their own migration class, based on normal file migration.
 */
class DrupalPicture7Migration extends DrupalFile7Migration {
  /**
   * We override the file query to identify which files are user pictures.
   *
   * @return SelectQueryInterface
   */
  protected function fileQuery() {
    $query = parent::fileQuery();
    $query->innerJoin('users', 'u', 'f.fid=u.picture');
    return $query;
  }
}
