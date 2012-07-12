<?php

/**
 * Drupal 6 implementations of functions shared among multiple types of objects.
 */
class DrupalVersion6 implements DrupalVersionInterface {
  protected $arguments;

  public function __construct($arguments) {
    $this->arguments = $arguments;
  }

  /**
   * Generate default format mappings based on matching names. E.g., if the
   * Drupal 6 database has format 5 with name 'Filtered HTML', and the Drupal 7
   * databas has format filtered_html with name 'Filtered HTML', the resulting
   * array will contain the row '5' => 'filtered_html'.
   */
  public function getDefaultFormatMappings() {
    migrate_instrument_start('DrupalVersion6::getDefaultFormatMappings');
    $format_mappings = array();
    $result = Database::getConnection('default', $this->arguments['source_connection'])
              ->select('filter_formats', 'f')
              ->fields('f', array('format', 'name'))
              ->execute();
    foreach ($result as $format_row) {
      $format = db_select('filter_format', 'f')
                ->fields('f', array('format'))
                ->condition('name', $format_row->name)
                ->execute()
                ->fetchField();
      if ($format) {
        $format_mappings[$format_row->format] = $format;
      }
    }
    migrate_instrument_stop('DrupalVersion6::getDefaultFormatMappings');
    return $format_mappings;
  }

  /**
   * Given a source path (e.g, 'node/123'), return the first alias for that path.
   *
   * @param $source
   * @return string
   */
  public function getPath($source) {
    migrate_instrument_start('DrupalVersion6::getPath');
    if (Database::getConnection('default', $this->arguments['source_connection'])
          ->schema()->tableExists('url_alias')) {
      $path = Database::getConnection('default', $this->arguments['source_connection'])
                    ->select('url_alias', 'ua')
                    ->fields('ua', array('dst'))
                    ->condition('src', $source)
                    ->execute()
                    ->fetchField();
    }
    else {
      $path = $source;
    }
    migrate_instrument_stop('DrupalVersion6::getPath');
    return $path;
  }

  public function sourceFieldInfo($entity_type, $bundle) {
    migrate_instrument_start('DrupalVersion6::sourceFieldInfo');
    $fields = array();
    if ($entity_type != 'node') {
      return $fields;
    }
    // Get each CCK field attached to this type.
    if (Database::getConnection('default', $this->arguments['source_connection'])
          ->schema()->tableExists('content_node_field_instance')) {
      $query = Database::getConnection('default', $this->arguments['source_connection'])
               ->select('content_node_field_instance', 'i')
               ->fields('i', array('label'))
               ->condition('i.type_name', $bundle);
      $query->innerJoin('content_node_field', 'f', 'i.field_name = f.field_name');
      $query->fields('f', array('field_name', 'type'));
      $result = $query->execute();
      foreach ($result as $row) {
        $fields[trim($row->field_name)] = array('label' => $row->label, 'type' => $row->type);
      }
    }
    // Get each vocabulary attached to this type.
    $query = Database::getConnection('default', $this->arguments['source_connection'])
             ->select('vocabulary_node_types', 'vnt')
             ->fields('vnt', array('vid'));
    $query->innerJoin('vocabulary', 'v', 'vnt.vid=v.vid');
    $query->addField('v', 'name');
    $query->condition('vnt.type', $bundle);
    $result = $query->execute();
    foreach ($result as $row) {
      $fields[$row->vid] = array('label' => $row->name, 'type' => 'taxonomy_term');
    }
    migrate_instrument_stop('DrupalVersion6::sourceFieldInfo');
    return $fields;
  }
}
