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
    if ($entity_type == 'user') {
      return $this->profileFields();
    }
    elseif ($entity_type != 'node') {
      return array();
    }
    migrate_instrument_start('DrupalVersion6::sourceFieldInfo');
    $fields = array();
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

  /**
   * Retrieve any user profile fields from the core profile module or
   * content_profile.
   *
   * @return array
   */
  protected function profileFields() {
    migrate_instrument_start('DrupalVersion6::userFields');
    $fields = array();
    // Check the core profile...
    if (Database::getConnection('default', $this->arguments['source_connection'])
          ->schema()->tableExists('profile_fields')) {
      $query = Database::getConnection('default', $this->arguments['source_connection'])
               ->select('profile_fields', 'f')
               ->fields('f', array('title', 'name', 'type'));
      $result = $query->execute();
      foreach ($result as $row) {
        $fields[trim($row->name)] = array('label' => $row->title, 'type' => $row->type);
      }
    }
    // ... then any content_profile node types. A variable named content_profile_use_foo
    // with a serialized value of 1 means foo is a node type of interest.
    migrate_instrument_stop('DrupalVersion6::userFields');
    $names = Database::getConnection('default', $this->arguments['source_connection'])
             ->select('variable', 'v')
             ->fields('v', array('name'))
             ->condition('name', 'content_profile_use_%', 'LIKE')
             ->condition('value', 'i:1;')
             ->execute()
             ->fetchCol();
    $index = strlen('content_profile_use_');
    foreach ($names as $name) {
      $type_name = substr($name, $index);
      $fields += $this->sourceFieldInfo('node', $type_name);
    }
    return $fields;
  }
}
