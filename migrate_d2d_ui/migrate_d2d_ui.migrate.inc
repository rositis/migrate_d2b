<?php
/**
 * @file
 * Wizard implementation for Drupal-to-Drupal migrations.
 */

/*
 * Implementation of hook_migrate_api().
 */
function migrate_d2d_ui_migrate_api() {
  $api = array(
    'api' => 2,
    'wizard_classes' => array('MigrateD2DWizard'),
  );
  return $api;
}

class MigrateD2DWizard extends MigrateUIWizard {
  protected $database;
  protected $connection;
  protected $sourceVersion;
  protected $vocabularyMappings = array();
  protected $typeMappings = array();
  protected $fieldMappings = array();

  public function __construct() {
    parent::__construct();
    $this->addStep(t('Credentials'), 'sourceDataForm');
    $this->addStep(t('Users'), 'userForm');
    $this->addStep(t('Taxonomy'), 'taxonomyForm');
    $this->addStep(t('ContentType'), 'contentTypeForm');
    $this->addStep(t('Review'), 'reviewForm');
  }
  public function getSourceName() {
    return t('Drupal');
  }

  public function overviewDescription() {
    return t('This wizard supports importing into your Drupal site ' .
      'from another Drupal site (running Drupal version 5, 6, or 7). To be ' .
      'able to use this wizard, you must have the address and credentials of ' .
      'a database containing the full source Drupal site.');
  }

  public function getSteps() {
    return array(
      1 => 'credentials',
      2 => array(
        'name' => t('Users'),
        'form' => 'migrate_d2d_ui_users',
      ),
      3 => array(
        'name' => t('Taxonomy'),
        'form' => 'migrate_d2d_ui_taxonomy',
      ),
      4 => array(
        'name' => t('Nodes'),
        'form' => 'migrate_d2d_ui_content_types',
      ),
      5 => array(
        'name' => t('Placeholder'),
        'form' => 'migrate_d2d_ui_placeholder',
      )
    );
  }

  /**
   * Form for specifying where to obtain the Drupal content.
   */
  public function sourceDataForm() {
    $form['overview'] = array(
      '#prefix' => '<p>',
      '#markup' => t('External Drupal sites can be imported into Drupal using this form.
        Please provide database credentials for the external Drupal site. It is recommended
        that this not be the same database account as is used by the site, but
        an account with read-only privileges.'),
      '#suffix' => '</p>',
    );

    $form['description'] = array(
      '#prefix' => '<p>',
      '#markup' => t('To import from an external Drupal site, enter your credentials here.'),
      '#suffix' => '</p>',
    );

    // Stolen from includes/install.core.inc, install_settings_form()
    require_once DRUPAL_ROOT . '/includes/install.inc';
    $drivers = drupal_get_database_types();
    $drivers_keys = array_keys($drivers);

    $form['driver'] = array(
      '#type' => 'radios',
      '#title' => t('Source database type'),
      '#required' => TRUE,
      '#default_value' => !empty($form_state['values']['driver']) ?
        $form_state['values']['driver'] : current($drivers_keys),
      '#description' => t('The type of database your source data is stored in.'),
    );
    if (count($drivers) == 1) {
      $form['driver']['#disabled'] = TRUE;
      $form['driver']['#description'] .= ' ' . st('Your PHP configuration only supports a single database type, so it has been automatically selected.');
    }

    // Add driver specific configuration options.
    foreach ($drivers as $key => $driver) {
      // @todo: For now skip pgsql, causes validation errors
      if ($key == 'pgsql') {
        continue;
      }
      $form['driver']['#options'][$key] = $driver->name();
      $form['settings'][$key] = $driver->getFormOptions(array());
      // Rewrite descriptions, which assume the database is being selected to
      // install Drupal into.
      switch ($key) {
        case 'mysql':
        case 'pgsql':
          $form['settings'][$key]['database']['#description'] =
            t('The name of the database your legacy Drupal data is stored in.');
          break;
        case 'sqlite':
          $form['settings'][$key]['database']['#description'] =
            t('The absolute path to the file where your legacy Drupal data is stored');
          break;
      }
      $form['settings'][$key]['#prefix'] = '<h2 class="js-hide">' . t('@driver_name settings', array('@driver_name' => $driver->name())) . '</h2>';
      $form['settings'][$key]['#type'] = 'container';
      $form['settings'][$key]['#tree'] = TRUE;
      $form['settings'][$key]['advanced_options']['#parents'] = array($key);
      $form['settings'][$key]['#states'] = array(
        'visible' => array(
          ':input[name=driver]' => array('value' => $key),
        )
      );
    }

    return $form;
  }
  /**
   * Attempt to authenticate using the provided credentials.
   *
   * @param unknown_type $form
   * @param unknown_type $form_state
   */
  protected function sourceDataFormValidate(&$form_state) {
    $this->database = $form_state['values'][$form_state['values']['driver']];
    $this->database['driver'] = $form_state['values']['driver'];
    $form_state['storage']['database'] = $this->database;
    unset($form_state['values']['pgsql']);

    // Make sure we can connect
    try {
      $admin_uid = $this->connection()->select('users', 'u')
                   ->fields('u', array('uid'))
                   ->condition('uid', 1)
                   ->execute()
                   ->fetchField();
      if ($admin_uid) {
        // Format module is required by D6 and D7, table name changed
        // @todo: Support D5
        if ($this->connection()->schema()->tableExists('filter_format')) {
          $this->sourceVersion = '7';
        }
        else {
          $this->sourceVersion = '6';
        }
      }
      else {
        form_set_error('driver', t('The source database does not appear to be a Drupal database.'));
      }
    }
    catch (Exception $e) {
      form_set_error('driver', t('Failed to connect to source database, message returned: !message',
        array('!message' => $e->getMessage())));
      return;
    }
  }

  /**
    * Form for specifying where to obtain the Drupal content.
    */
   protected function userForm() {
     $form['#tree'] = TRUE;
     $num_users = $this->connection()->select('users', 'u')
                 ->condition('uid', 1, '>')
                 ->countQuery()
                 ->execute()
                 ->fetchField();
     $form['users'] = array(
       '#prefix' => '<div>',
       '#markup' => t('Number of users to be migrated from your Drupal !version site: !count',
                      array('!version' => $this->sourceVersion, '!count' => $num_users)),
       '#postfix' => '</div>',
     );

     //@todo: Select whether to import users
   /*
     $result = $connection->select('role', 'r')
               ->fields('r', array('rid', 'name'))
               ->condition('name', array('anonymous user', 'authenticated user'), 'NOT IN')
               ->execute();
     $candidates = array();
     foreach ($result as $row) {
       $d7_rid = db_select('role', 'r')
                 ->fields('r', array('rid'))
                 ->condition('name', $row->name)
                 ->execute()
                 ->fetchField();
       if (!$d7_rid) {
         $candidates[$row->rid] = $row->name;
       }
     }

     if (!empty($candidates)) {
       // Description
       $form['role'] = array(
         '#type' => 'fieldset',
         '#description' => t('Select each user role you want created in the destination site:'),
       );
       foreach ($candidates as $rid => $name) {
         $form['role'][$rid] = array(
           '#type' => 'checkbox',
           '#title' => $name,
           '#default_value' => 1,
         );
       }
     }
     else {
       $form['role'] = array(
         '#prefix' => '<div>',
         '#markup' =>t('There are no user roles in the source site that are not already
           in the destination site'),
         '#suffix' => '</div>',
       );
     }*/
     return $form;
   }

   protected function userFormValidate(&$form_state) {
   //  $form_state['storage']['role'] = $form_state['values']['role'];
   // @todo: Do this at review time?
     $arguments = array(
       'description' => t('Migration of users from Drupal @version',
               array('@version' => $this->sourceVersion)),
     );
     $this->addMigration($this->groupName . 'User',
       "DrupalUser{$this->sourceVersion}Migration", $arguments);
   }

  /**
   * Choose what source vocabularies to import into what destination vocabularies.
   */
  protected function taxonomyForm() {
    if ($this->connection()->schema()->tableExists('taxonomy_vocabulary')) {
      $vocab_table = 'taxonomy_vocabulary';
    }
    elseif ($this->connection()->schema()->tableExists('vocabulary')) {
      $vocab_table = 'vocabulary';
    }
    else {
      $vocab_table = '';
      $vocab_count = 0;
    }

    if ($vocab_table) {
      $vocab_count = $this->connection()->select($vocab_table, 'v')
                     ->fields('v', array('vid'))
                     ->countQuery()
                     ->execute()
                     ->fetchField();
    }
    if (!$vocab_count) {
      $form['description'] = array(
        '#prefix' => '<div>',
        '#markup' => t('There is no vocabulary data to be migrated from the source database.'),
        '#suffix' => '</div>',
      );
      return $form;
    }
    $form['#tree'] = TRUE;
    $form['description'] = array(
      '#prefix' => '<div>',
      '#markup' => t('For each vocabulary on the source site, choose the destination
        site vocabulary to import its terms. You may also choose not to import a given
        vocabulary, or automatically create a new vocabulary of the same name if
        one does not already exist.'),
      '#suffix' => '</div>',
    );

    $base_options = array(
      -1 => t('--Do not import--'),
      0 => t('--Create vocabulary--'),
    );
    $vocab_options = array();
    $local_vocabs = taxonomy_get_vocabularies();
    foreach ($local_vocabs as $vid => $vocab) {
      $vocab_options[$vocab->machine_name] = $vocab->name;
    }
    $result = $this->connection()->select($vocab_table, 'v')
              ->fields('v', array('vid', 'name'))
              ->execute();
    foreach ($result as $vocab) {
      $options = $base_options + $vocab_options;
      // If we have a match on vocabulary name, default the mapping to that match
      // and remove the option to create a new vocabulary of that name.
      if ($vid = array_search($vocab->name, $vocab_options)) {
        $default_value = $vid;
        unset($options[0]);
      }
      else {
        $default_value = -1;
      }
      $form['vocab'][$vocab->vid] = array(
        '#type' => 'select',
        '#title' => $vocab->name,
        '#options' => $options,
        '#default_value' => $default_value,
      );
    }

    return $form;
  }

  /**
   * Save the selected vocabulary mappings, as an array keyed by source vid and
   * containing the destination machine_name (bundle).
   */
  protected function taxonomyFormValidate(&$form_state) {
    $this->vocabularyMappings = $form_state['values']['vocab'];
  }

  /**
   * Form for specifying where to obtain the Drupal content.
   */
  protected function contentTypeForm() {
    $type_count = $this->connection()->select('node_type', 't')
                    ->fields('t', array('type', 'name'))
                    ->countQuery()
                    ->execute()
                    ->fetchField();
    if (!$type_count) {
      $form['description'] = array(
        '#prefix' => '<div>',
        '#markup' => t('There is no node data to be migrated from the source database.'),
        '#suffix' => '</div>',
      );
      return $form;
    }
    $form['#tree'] = TRUE;
    $form['description'] = array(
      '#prefix' => '<div>',
      '#markup' => t('For each node type on the source site, choose the destination
        site node type to import its content. You may also choose not to import a given
        node type, or automatically create a new node type of the same name if
        one does not already exist.'),
      '#suffix' => '</div>',
    );

    $base_options = array(
      '-1' => t('--Do not import--'),
      '0' => t('--Create content type--'),
    );
    $node_options = array();
    $local_types = node_type_get_types();
    foreach ($local_types as $type => $info) {
      $node_options[$type] = $info->name;
    }
    $result = $this->connection()->select('node_type', 't')
              ->fields('t', array('type', 'name'))
              ->execute();
    foreach ($result as $row) {
      $options = $base_options + $node_options;
      // If we have a match on type name, default the mapping to that match
      // and remove the option to create a new type of that name.
      if (isset($node_options[$row->type])) {
        $default_value = $row->type;
        unset($options['0']);
      }
      else {
        $default_value = '-1';
      }
      $form['type'][$row->type] = array(
        '#type' => 'select',
        '#title' => $row->name,
        '#options' => $options,
        '#default_value' => $default_value,
      );
    }

    return $form;
  }

  /**
   * Save the selected type mappings, as an array keyed by source type and
   * containing the destination type (bundle).
   */
  protected function contentTypeFormValidate(&$form_state) {
    // Start with ourselves
    $after = $this->currentStep;
    foreach ($form_state['values']['type'] as $source_type => $dest_type) {
      // For each content type to be migrated, add a step
      if ($dest_type != '0' && $dest_type != '-1') {
        $after = $this->addStep(t('Field mappings for !type',
          array('!type' => $source_type)), 'fieldsForm', $after);
        $after->setData(array('source_type' => $source_type, 'dest_type' => $dest_type));
      }
    }
    $this->typeMappings = $form_state['values']['type'];
  }

  /**
   * Form for mapping fields between two specific content types.
   */
  function fieldsForm() {
    $type_mapping = $this->currentStep->getData();
    $source_type = $type_mapping['source_type'];
    $dest_type = $type_mapping['dest_type'];

    // Temporarily register a migration, as a lazy way to get the available fields.
    $source_class = "DrupalNode{$this->sourceVersion}Migration";
    $arguments = array(
      'source_connection' => $this->groupName . '_legacy',
      'source_database' => $this->database,
      'source_version' => $this->sourceVersion,
      'class_name' => $source_class,
      'description' => t('Temp migration to fetch fields'),
      'machine_name' => 'temp',
      'source_type' => $source_type,
      'destination_type' => $dest_type,
    );
    Migration::registerMigration($source_class, 'temp', $arguments);
    $migration = Migration::getInstance('temp');
    $source_fields = $migration->getSource()->fields();
    $dest_fields = $migration->getDestination()->fields();
    Migration::deregisterMigration('temp');

    $form['#tree'] = TRUE;
    $form['description'] = array(
      '#prefix' => '<div>',
      '#markup' => t('For each field in the source type %source, choose the destination
        site field from the destination type %destination to import its content.
        You may also choose not to import a given field, or automatically create
        the field with the same name if it does not already exist.',
        array('%source' => $source_type, '%destination' => $dest_type)),
      '#suffix' => '</div>',
    );

    $base_options = array(
      '-1' => t('--Do not import--'),
      '0' => t('--Create field--'),
    );
    $field_options = array();
    foreach ($dest_fields as $name => $description) {
      $field_options[$name] = strip_tags($description);
    }
    foreach ($source_fields as $name => $description) {
      $options = $base_options + $field_options;
      // If we have a match on field name, default the mapping to that match
      // and remove the option to create a new field of that name.
      if (isset($field_options[$name])) {
        $default_value = $name;
        unset($options['0']);
      }
      else {
        $default_value = '-1';
      }
      $form['field'][$name] = array(
        '#type' => 'select',
        '#title' => $description,
        '#options' => $options,
        '#default_value' => $default_value,
      );
    }

    return $form;
  }

  protected function fieldsFormValidate(&$form_state) {
    $type_mapping = $this->currentStep->getData();
    $source_type = $type_mapping['source_type'];
    $this->fieldMappings[$source_type] = $form_state['values']['field'];
  }

  /**
   * Form for mapping fields between two specific content types.
   */
  protected function reviewForm() {
    $message = t('<p>Please review your migration configuration. When you submit this
      form, migration processes will be created and you will be deposited at the
      migration dashboard.</p>');

    $form['description'] = array(
      '#prefix' => '<div>',
      '#markup' => $message,
      '#suffix' => '</div>',
    );
  /*
    $result = $connection->select('role', 'r')
              ->fields('r', array('name'))
              ->condition('rid', array_keys(array_filter($form_state['storage']['role'])), 'IN')
              ->execute();
    $first = TRUE;
    foreach ($result as $row) {
      if ($first) {
        $message .= t('<p>The following roles will be created:</p>');
        $first = FALSE;
      }
      $message .= '<p><em>' . $row->name . '</em></p>';
    }
    if ($first) {
      $message .= t('<p>No roles will be created.</p>');
    }*/
    $admin_name = $this->connection()->select('users', 'u')
                  ->fields('u', array('name'))
                  ->condition('uid', 1)
                  ->execute()
                  ->fetchField();
    $message .= t('<p>All user accounts except the administrative account (%admin)
      will be imported.</p>', array('%admin' => $admin_name));

    $vocabs_created = $vocabs_ignored = $vocabs_mapped = '';
    if ($this->connection()->schema()->tableExists('taxonomy_vocabulary')) {
      $vocab_table = 'taxonomy_vocabulary';
    }
    elseif ($this->connection()->schema()->tableExists('vocabulary')) {
      $vocab_table = 'vocabulary';
    }
    return $form;
    foreach ($form_state['storage']['vocab'] as $source_vid => $destination_vid) {
      $source_vocabulary = $connection->select($vocab_table, 'v')
                           ->fields('v', array('name'))
                           ->condition('vid', $source_vid)
                           ->execute()
                           ->fetchField();
      if ($destination_vid == -1) {
        $vocabs_ignored .= '<p><em>' . $source_vocabulary . '</em></p>';
      }
      elseif ($destination_vid == 0) {
        $vocabs_created .= '<p><em>' . $source_vocabulary . '</em></p>';
      }
      else {
        $destination_vocabulary = db_select('taxonomy_vocabulary', 'v')
                                  ->fields('v', array('name'))
                                  ->condition('vid', $destination_vid)
                                  ->execute()
                                  ->fetchField();
        $vocabs_mapped .= t('<p>Terms from %source will be imported into the %destination vocabulary.</p>',
          array('%source' => $source_vocabulary, '%destination' => $destination_vocabulary));
      }
    }
    if (count($vocabs_ignored) > 0) {
      $message .= t('<p>The following vocabularies will not be imported:</p>');
      $message .= $vocabs_ignored;
    }
    if (count($vocabs_created) > 0) {
      $message .= t('<p>The following vocabularies will be created:</p>');
      $message .= $vocabs_created;
    }
    $message .= $vocabs_mapped;

    $types_created = $types_ignored = $types_mapped = '';
    foreach ($form_state['storage']['type'] as $source_type => $destination_type) {
      $source_name = $this->connection()->select('node_type', 't')
                     ->fields('t', array('name'))
                     ->condition('type', $source_type)
                     ->execute()
                     ->fetchField();
      if ($destination_type == '-1') {
        $types_ignored .= '<p><em>' . $source_name . '</em></p>';
      }
      elseif ($destination_type == '0') {
        $types_created .= '<p><em>' . $source_name . '</em></p>';
      }
      else {
        $destination_name = db_select('node_type', 't')
                            ->fields('t', array('name'))
                            ->condition('type', $destination_type)
                            ->execute()
                            ->fetchField();
        $types_mapped .= t('<p>Nodes of source type %source will be imported as nodes of destination type %destination.</p>',
          array('%source' => $source_name, '%destination' => $destination_name));
      }
    }
    if (count($types_ignored) > 0) {
      $message .= t('<p>The following node types will not be imported:</p>');
      $message .= $types_ignored;
    }
    if (count($types_created) > 0) {
      $message .= t('<p>The following node types will be created:</p>');
      $message .= $types_created;
    }
    $message .= $types_mapped;

    return $form;
  }

  protected function connection() {
    static $connection;
    if (!isset($connection)) {
      $connection_key = $this->groupName . '_legacy';
      Database::addConnectionInfo($connection_key, 'default', $this->database);
      $connection = Database::getConnection('default', $connection_key);
    }
    return $connection;
  }

  protected function addMigration($machine_name,
      $class_name, $arguments) {
    $arguments += array(
        'source_version' => $this->sourceVersion,
        'source_connection' => $this->groupName . '_legacy',
        'source_database' => $this->database,
      );
    parent::addMigration($machine_name, $class_name, $arguments);
  }

}
