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
  /**
   * Database array for the source Drupal installation.
   *
   * @var array
   */
  protected $database;

  /**
   * Source Drupal version (5, 6, or 7).
   *
   * @var int
   */
  protected $sourceVersion;

  /**
   * Whether we will be importing users.
   *
   * @var bool
   */
  protected $userMigration = FALSE;

  protected $userCount;

  /**
   * Mappings of source vocabulary vids to destination vocabulary machine names.
   *
   * @var array
   */
  protected $vocabularyMappings = array();

  protected $termCounts = array();

  /**
   * Mappings of source content types to destination content types (by machine
   * name).
   *
   * @var array
   */
  protected $typeMappings = array();

  protected $nodeCounts = array();

  /**
   * For each content type, mappings of its fields by machine name.
   *
   * @var array
   */
  protected $fieldMappings = array();

  /**
   * Lay out the basic steps of the wizard.
   */
  public function __construct() {
    parent::__construct();
    $this->addStep(t('Overview'), 'overviewForm');
    $this->addStep(t('Credentials'), 'sourceDataForm');
    $this->addStep(t('Users'), 'userForm');
    $this->addStep(t('Taxonomy'), 'taxonomyForm');
    $this->addStep(t('Content Types'), 'contentTypeForm');
    $this->addStep(t('Review'), 'reviewForm');
  }

  /**
   * Identify ourselves.
   *
   * @return string
   */
  public function getSourceName() {
    return t('Drupal');
  }

  /**
   * The first step of the wizard - explain what we're doing, and get the
   * migration group.
   *
   * @return array
   */
  protected function overviewForm() {
    $form['overview'] = array(
      '#prefix' => '<p>',
      '#markup' => t('This wizard supports importing into your Drupal site ' .
            'from another Drupal site (running Drupal version 5, 6, or 7). To be ' .
            'able to use this wizard, you must have the address and credentials of ' .
            'a database containing the full source Drupal site.'),
      '#suffix' => '</p>',
    );
    $form['description'] = array(
      '#prefix' => '<p>',
      '#markup' => t('You will be led through a series of steps, allowing you to ' .
        'customize what will be imported into Drupal and how it will be mapped. ' .
        'At the end of this process, a set of import steps (called <em>Migrations</em>) '.
        'are generated and you will be left at the Migrate dashboard, from which ' .
        'you can perform the imports, monitor their progress, roll them back, etc.'),
      '#suffix' => '</p>',
    );

    $form['group_name'] = array(
      '#type' => 'textfield',
      '#title' => t('Unique name'),
      '#description' => t('Provide a unique alphanumeric name for this import. It ' .
        'will be used to prefix and group the generated migrations.'),
      '#size' => 16,
      '#maxlength' => 16,
      '#required' => TRUE,
      '#default_value' => $this->groupName,
    );
    return $form;
  }

  /**
  * Make sure the migration group is alphanumeric, and save it.
  *
  * @param array $form_state
  */
  protected function overviewFormValidate(&$form_state) {
    $this->groupName = trim($form_state['values']['group_name']);
    if (!preg_match('|^[[:alnum:]]+$|', $this->groupName)) {
      form_set_error('group_name',
        t('The name for your import, %name, must be alphanumeric.',
          array('%name' => $this->groupName)));
    }
  }

  /**
   * Second step - find out how to get the source data.
   */
  public function sourceDataForm() {
    $form['overview'] = array(
      '#prefix' => '<p>',
      '#markup' => t('External Drupal sites can be imported into Drupal using this form.
        Please provide database credentials for the external Drupal site. It is recommended
        that this not be the same database user as is used by the site, but
        an user with read-only privileges.'),
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
   * Save the database access information, and make sure we can actually connect.
   *
   * @param array $form_state
   */
  protected function sourceDataFormValidate(&$form_state) {
    $this->database = $form_state['values'][$form_state['values']['driver']];
    $this->database['driver'] = $form_state['values']['driver'];
    $form_state['storage']['database'] = $this->database;

    // Make sure we can connect
    try {
      $admin_uid = $this->connection()->select('users', 'u')
                   ->fields('u', array('uid'))
                   ->condition('uid', 1)
                   ->execute()
                   ->fetchField();
      if ($admin_uid) {
        // Format module is required by D6 and D7, table name changed.
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
      form_set_error('driver', t('Failed to connect to source database, message returned: %message',
        array('%message' => $e->getMessage())));
      return;
    }
  }

  /**
   * Present information and options around user migration.
   */
  protected function userForm() {
    $form['overview'] = array(
      '#prefix' => '<p>',
      '#markup' => t('User accounts other than the admin account (user ID 1)
         may be imported to this site.'),
      '#suffix' => '</p>',
    );
    $form['#tree'] = TRUE;
    if (!isset($this->userCount)) {
      $this->userCount = $this->connection()->select('users', 'u')
                  ->condition('uid', 1, '>')
                  ->countQuery()
                  ->execute()
                  ->fetchField();
    }
    $form['users'] = array(
      '#prefix' => '<div>',
      '#markup' => t('Number of users available to be migrated from your Drupal !version site: !count',
                     array('!version' => $this->sourceVersion, '!count' => $this->userCount)),
      '#postfix' => '</div>',
    );
    $form['do_migration'] = array(
      '#type' => 'checkbox',
      '#title' => t('Perform import of users'),
      '#default_value' => TRUE,
    );

    $form['todo'] = array(
      '#prefix' => '<div>',
      '#markup' => t('TODO: Option to import roles, and map source roles to D7 roles'),
      '#postfix' => '</div>',
    );
    return $form;

    $result = $this->connection()->select('role', 'r')
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
    }
    return $form;
  }

  /**
   * Prepare for registration of any user-related migrations.
   *
   * @param array $form_state
   */
  protected function userFormValidate(&$form_state) {
    if ($form_state['values']['do_migration']) {
      $arguments = array(
        'description' => t('Migration of users from Drupal @version',
                 array('@version' => $this->sourceVersion)),
      );
      $this->addMigration($this->groupName . 'User',
        "DrupalUser{$this->sourceVersion}Migration", $arguments);
      $this->userMigration = TRUE;
    }
    else {
      $this->userMigration = FALSE;
    }
  }

  /**
   * Choose what source vocabularies to import into what destination vocabularies.
   */
  protected function taxonomyForm() {
    if ($this->connection()->schema()->tableExists('taxonomy_vocabulary')) {
      $vocab_table = 'taxonomy_vocabulary';
      $term_table = 'taxonomy_term_data';
    }
    elseif ($this->connection()->schema()->tableExists('vocabulary')) {
      $vocab_table = 'vocabulary';
      $term_table = 'term_data';
    }
    else {
      $vocab_table = '';
      $term_table = '';
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

    if (empty($this->termCounts)) {
      $get_term_counts = TRUE;
    }
    else {
      $get_term_counts = FALSE;
    }
    $base_options = array(
      -1 => t('--Do not import--'),
      0 => t('--Create vocabulary--'),
    );
    $vocab_options = array();
    $local_vocabs = taxonomy_get_vocabularies();
    foreach ($local_vocabs as $vocab) {
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

      if ($get_term_counts) {
        $this->termCounts[$vocab->vid] = $this->connection()->select($term_table, 't')
                      ->condition('vid', $vocab->vid)
                      ->countQuery()
                      ->execute()
                      ->fetchField();
      }
      $title = t('@name: @count', array('@name' => $vocab->name,
        '@count' => format_plural($this->termCounts[$vocab->vid], '1 term', '@count terms')));
      $form['vocab'][$vocab->vid] = array(
        '#type' => 'select',
        '#title' => $title,
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
   * Form for mapping source content (node) types to destination types.
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
      '#markup' => t('For each content type on the source site, choose the destination
        site content type to import its content. You may also choose not to import a given
        content type, or automatically create a new content type of the same name if
        one does not already exist.'),
      '#suffix' => '</div>',
    );

    if (empty($this->nodeCounts)) {
      $get_node_counts = TRUE;
    }
    else {
      $get_node_counts = FALSE;
    }
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
      if ($get_node_counts) {
        $this->nodeCounts[$row->type] = $this->connection()->select('node', 'n')
                      ->condition('type', $row->type)
                      ->countQuery()
                      ->execute()
                      ->fetchField();
      }
      $title = t('@name: @count', array('@name' => $row->name,
        '@count' => format_plural($this->nodeCounts[$row->type], '1 node', '@count nodes')));
      $form['type'][$row->type] = array(
        '#type' => 'select',
        '#title' => $title,
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
    /* @todo: Skip field mappings for now
    $after = $this->currentStep;
    foreach ($form_state['values']['type'] as $source_type => $dest_type) {
      // For each content type to be migrated, add a step to the wizard.
      if ($dest_type != '0' && $dest_type != '-1') {
        $context = array('source_type' => $source_type, 'dest_type' => $dest_type);
        $after = $this->addStep(t('Field mappings for @type',
          array('@type' => $source_type)), 'fieldsForm', $after, $context);
      }
    }*/
    $this->typeMappings = $form_state['values']['type'];
  }

  /**
   * Form for mapping fields between two specific content types.
   */
  function fieldsForm() {
    $type_mapping = $this->currentStep->getContext();
    $source_type = $type_mapping['source_type'];
    $dest_type = $type_mapping['dest_type'];

    // Temporarily register a migration, as a lazy way to get the available fields.
    $source_class = "DrupalNode{$this->sourceVersion}Migration";
    $temp_machine_name = md5('temp');
    $arguments = array(
      'source_connection' => $this->groupName . '_legacy',
      'source_database' => $this->database,
      'source_version' => $this->sourceVersion,
      'class_name' => $source_class,
      'description' => t('Temp migration to fetch fields'),
      'machine_name' => $temp_machine_name,
      'source_type' => $source_type,
      'destination_type' => $dest_type,
    );
    Migration::registerMigration($source_class, $temp_machine_name, $arguments);
    $migration = Migration::getInstance($temp_machine_name);
    $source_fields = $migration->getSource()->fields();
    $dest_fields = $migration->getDestination()->fields();
    Migration::deregisterMigration($temp_machine_name);

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
      if (is_array($description)) {
        $description = reset($description);
      }
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

  /**
   * Save away the chosen field mappings.
   *
   * @param array $form_state
   */
  protected function fieldsFormValidate(&$form_state) {
    $type_mapping = $this->currentStep->getContext();
    $source_type = $type_mapping['source_type'];
    $this->fieldMappings[$source_type] = $form_state['values']['field'];
  }

  /**
   * Show the user what they've chosen to migrate, and give them one last chance
   * to say yay or nay.
   */
  protected function reviewForm() {
    $message = t('<p>Please review your migration configuration. When you submit this
      form, migration processes will be created and you will be left at the
      migration dashboard.</p>');

    $form['description'] = array(
      '#prefix' => '<div>',
      '#markup' => $message,
      '#suffix' => '</div>',
    );

    $migrations = '';
    if ($this->userMigration) {
      $admin_name = $this->connection()->select('users', 'u')
                    ->fields('u', array('name'))
                    ->condition('uid', 1)
                    ->execute()
                    ->fetchField();
      $migrations .= t('<li>%count user accounts (omitting the administrative account ' .
        '%admin) will be imported.</li>',
        array('%admin' => $admin_name, '%count' => $this->userCount));
    }
    else {
      $migrations .= t('<li>User accounts will not be migrated</li>');
    }

    if ($this->connection()->schema()->tableExists('taxonomy_vocabulary')) {
      $vocab_table = 'taxonomy_vocabulary';
    }
    elseif ($this->connection()->schema()->tableExists('vocabulary')) {
      $vocab_table = 'vocabulary';
    }

    foreach ($this->vocabularyMappings as $source_vid => $destination_name) {
      $source_vocabulary = $this->connection()->select($vocab_table, 'v')
                           ->fields('v', array('name'))
                           ->condition('vid', $source_vid)
                           ->execute()
                           ->fetchField();
      $count = format_plural($this->termCounts[$source_vid], '1 term', '@count terms');
      if ($destination_name == '-1') {
        $migrations .= t('<li>The %vocabulary vocabulary will not be imported.</li>',
          array('%vocabulary' => $source_vocabulary));
      }
      elseif ($destination_name == '0') {
        $migrations .= t('<li>The %vocabulary vocabulary will be created with %count.</li>',
          array('%vocabulary' => $source_vocabulary, '%count' => $count));
      }
      else {
        $destination_vocabulary = db_select('taxonomy_vocabulary', 'v')
                                  ->fields('v', array('name'))
                                  ->condition('machine_name', $destination_name)
                                  ->execute()
                                  ->fetchField();
        $migrations .= t('<li>%count from %source will be imported into the %destination vocabulary.</li>',
          array('%source' => $source_vocabulary, '%destination' => $destination_vocabulary,
            '%count' => $count));
      }
    }

    foreach ($this->typeMappings as $source_type => $destination_type) {
      $source_name = $this->connection()->select('node_type', 'nt')
                           ->fields('nt', array('name'))
                           ->condition('type', $source_type)
                           ->execute()
                           ->fetchField();
      $count = format_plural($this->nodeCounts[$source_type], '1 node', '@count nodes');
      if ($destination_type == '-1') {
        $migrations .= t('<li>The %type content type will not be imported.</li>',
          array('%type' => $source_name));
      }
      elseif ($destination_type == '0') {
        $migrations .= t('<li>The %type content type will be created with %count.</li>',
          array('%type' => $source_name, '%count' => $count));
      }
      else {
        $destination_type = db_select('node_type', 'nt')
                                  ->fields('nt', array('name'))
                                  ->condition('type', $destination_type)
                                  ->execute()
                                  ->fetchField();
        $migrations .= t('<li>%count from %source will be imported into the %destination content type.</li>',
          array('%count' => $count, '%source' => $source_name, '%destination' => $destination_type));
      }
    }

    $form['migration_list'] = array(
      '#prefix' => '<ol>',
      '#markup' => $migrations,
      '#suffix' => '</ol>',
    );

    return $form;
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

  /**
   * Gets a connection to the source Drupal database, based on the database
   * information saved by the sourceDataForm.
   *
   * @return DatabaseConnection
   */
  protected function connection() {
    // The Wizard class must be serializable so it can be saved between steps.
    // Thus, the connection cannot be a member of the class - we need to
    // keep it in memory.
    static $connection;
    if (!isset($connection)) {
      $connection_key = $this->groupName . '_legacy';
      Database::addConnectionInfo($connection_key, 'default', $this->database);
      $connection = Database::getConnection('default', $connection_key);
    }
    return $connection;
  }

  /**
   * Record all the information necessary to register a migration when this is
   * all over.
   *
   * @param string $machine_name
   *  Machine name for the migration class.
   * @param string $class_name
   *  Name of the Migration class to instantiate.
   * @param array $arguments
   *  Further information configuring the migration.
   */
  protected function addMigration($machine_name, $class_name, $arguments) {
    $arguments += array(
        'source_version' => $this->sourceVersion,
        'source_connection' => $this->groupName . '_legacy',
        'source_database' => $this->database,
      );
    parent::addMigration($machine_name, $class_name, $arguments);
  }
}
