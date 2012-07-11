<?php

class DrupalVersion7 implements DrupalVersionInterface {
  protected $arguments;

  public function __construct($arguments) {
    $this->arguments = $arguments;
  }

  /**
   * Generate default format mappings based on matching names.
   */
  public function getDefaultFormatMappings() {
    $format_mappings = array();
    $result = Database::getConnection('default', $this->arguments['source_connection'])
              ->select('filter_format', 'f')
              ->fields('f', array('format'))
              ->execute();
    foreach ($result as $format_row) {
      $format_mappings[$format_row->format] = $format_row->format;
    }
    return $format_mappings;
  }

  public function getPath($source) {
    $path = Database::getConnection('default', $this->arguments['source_connection'])
                  ->select('url_alias', 'ua')
                  ->fields('ua', array('alias'))
                  ->condition('source', $source)
                  ->execute()
                  ->fetchField();
    return $path;
  }

  public function sourceFieldInfo($entity_type, $bundle) {
    migrate_instrument_start('DrupalVersion7::sourceFieldInfo');
    $fields = array();

    // Get each field attached to this type.
    if (Database::getConnection('default', $this->arguments['source_connection'])
          ->schema()->tableExists('field_config_instance')) {
      $query = Database::getConnection('default', $this->arguments['source_connection'])
               ->select('field_config_instance', 'i')
               ->fields('i', array('data'))
               ->condition('entity_type', $entity_type)
               ->condition('bundle', $bundle)
               ->condition('i.deleted', 0);
      $query->innerJoin('field_config', 'f', 'i.field_name = f.field_name');
      $query->fields('f', array('field_name', 'type'));
      $result = $query->execute();
      foreach ($result as $row) {
        $data = unserialize($row->data);
        $fields[trim($row->field_name)] = array('label' => $data['label'], 'type' => $row->type);
      }
    }
    migrate_instrument_stop('DrupalVersion7::sourceFieldInfo');
    return $fields;
  }
}
