<?php

/**
 * Handling specific to a Drupal 7 source for nodes.
 */
class DrupalNode7Migration extends DrupalNodeMigration {
  /**
   * @param array $arguments
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->addFieldMapping('tnid', 'tnid', FALSE)
         ->sourceMigration($arguments['machine_name']);
    $this->addFieldMapping('translate', 'translate');

    // Basic field suffixes
    // @todo: handle multiple values (e.g., summaries on text_with_summary)
    $this->fieldSuffixes = array(
      'image' => 'fid',
      'link_field' => 'url',
      'media' => 'fid',
      'node_reference' => 'nid',
      'taxonomy_term_reference' => 'tid',
    );
  }

  /**
   * Query for basic node fields from Drupal 7.
   *
   * @return QueryConditionInterface
   */
  protected function nodeQuery() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('node', 'n')
             ->fields('n', array('nid', 'vid', 'language', 'title', 'uid',
               'status', 'created', 'changed', 'comment', 'promote', 'sticky',
               'tnid', 'translate'))
             ->condition('type', $this->sourceType);
    return $query;
  }

  /**
   * Called after the basic query data is fetched - we'll use this to populate
   * the source row with the fields.
   */
  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }

    // Load up field data for dynamically mapped fields
    foreach ($this->sourceFieldTypes as $field_name => $field_type) {
      // Find the data in field_data_$field_name.
      // TODO: Handle multi-value fields
      $table = "field_data_$field_name";
      if (isset($this->fieldSuffixes[$field_type])) {
        $suffix = $this->fieldSuffixes[$field_type];
      }
      else {
        $suffix = 'value';
      }
      $field_field = $field_name .'_'. $suffix;
      $row->$field_name = Database::getConnection('default', $this->sourceConnection)
                          ->select($table, 'f')
                          ->fields('f', array($field_field))
                          ->condition('vid', $row->vid)
                          ->execute()
                          ->fetchField();
      // Allow for field-type translation
      $method = "handle_type_$field_type";
      if (method_exists($this, $method)) {
        $row->$field_name = $this->$method($row->$field_name);
      }
      // Some fields will need translation (e.g., from a text string to a boolean)
      $method = "handle_$field_name";
      if (method_exists($this, $method)) {
        $row->$field_name = $this->$method($row->$field_name);
      }
      if (!empty($field_name) && empty($row->$field_name)) {
        unset($row->$field_name);
      }
    }
  }
}
