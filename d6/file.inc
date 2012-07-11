<?php


class DrupalFile6Migration extends DrupalFileMigration {
  protected $legacyPath;

  public function __construct(array $arguments) {
    parent::__construct($arguments);
    $this->addFieldMapping('value', 'filepath')
         ->callbacks(array($this, 'fixUri'));
    $this->addFieldMapping('destination_file', 'filepath')
      ->callbacks(array($this, 'fixUri'));

    $this->legacyPath = unserialize(
      Database::getConnection('default', $this->sourceConnection)
                 ->select('variable', 'v')
                 ->fields('v', array('value'))
                 ->condition('name', 'file_directory_path')
                 ->execute()
                 ->fetchField());
    // Strip ./ from the beginning
    if (substr($this->legacyPath, 0, 2) == './') {
      $this->legacyPath = substr($this->legacyPath, 2);
    }
  }

  protected function fileQuery() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('files', 'f')
             ->fields('f');
    return $query;
  }

  protected function fixUri($uri) {
    // Get the URI relative the the file directory
    $result = str_replace($this->legacyPath, '', $uri);
    return $result;
  }
  public function prepareRow($row) {
    parent::prepareRow($row);
  }
}
