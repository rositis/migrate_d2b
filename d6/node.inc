<?php

/**
 * Handling specific to a Drupal 6 source for nodes.
 */
class DrupalNode6Migration extends DrupalNodeMigration {
  /**
   * @param array $arguments
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->addFieldMapping('body:summary', 'teaser');
    $this->addFieldMapping('body:format', 'format')
         ->callbacks(array($this, 'mapFormat'));
    $this->addFieldMapping('body:language');
    $this->addFieldMapping(NULL, 'moderate');
    /** @todo Prevent stub creation when tnid == 0
    $this->addFieldMapping('tnid', 'tnid', FALSE)
         ->sourceMigration($arguments['machine_name']);
     */
    $this->addFieldMapping('tnid', NULL);
    $this->addFieldMapping(NULL, 'tnid');

    $this->addFieldMapping('translate', 'translate');
  }

  /**
   * Query for basic node fields from Drupal 6.
   *
   * @return QueryConditionInterface
   */
  protected function query() {
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('node', 'n')
             ->fields('n', array('nid', 'vid', 'language', 'title',
                 'uid', 'status', 'created', 'changed', 'comment', 'promote',
                 'moderate', 'sticky', 'tnid', 'translate'))
             ->condition('type', $this->sourceType);
    $query->innerJoin('node_revisions', 'nr', 'n.vid=nr.vid');
    $query->fields('nr', array('body', 'teaser', 'format'));
    // Pick up simple CCK fields
    $cck_table = 'content_type_' . $this->sourceType;
    if (Database::getConnection('default', $this->sourceConnection)
          ->schema()->tableExists($cck_table)) {
      $query->leftJoin($cck_table, 'f', 'n.vid=f.vid');
      // The main column for the field should be rendered with
      // the field name, not the column name (e.g., field_foo rather
      // than field_foo_value).
      $field_info = $this->version->getSourceFieldInfo();
      foreach ($field_info as $field_name => $info) {
        if (isset($info['columns']) && !$info['multiple'] && $info['db_storage']) {
          $i = 0;
          foreach ($info['columns'] as $display_name => $column_name) {
            if ($i++ == 0) {
              $query->addField('f', $column_name, $field_name);
            }
            else {
              $query->addField('f', $column_name, $display_name);
            }
          }
        }
      }
    }
    return $query;
  }
}
