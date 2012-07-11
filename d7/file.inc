<?php


class DrupalFile7Migration extends DrupalFileMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    $this->addFieldMapping('value', 'uri')
         ->callbacks(array($this, 'fixUri'));
    $this->addFieldMapping('destination_file', 'uri')
      ->callbacks(array($this, 'fixUri'));
  }

  protected function fileQuery() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('file_managed', 'f')
             ->fields('f');
    return $query;
  }

  protected function fixUri($uri) {
    $result = str_replace('public://', '', $uri);
    drush_print("changed uri from $uri to $result");
    return $result;
  }
  public function prepareRow($row) {
    parent::prepareRow($row);
  }
}
