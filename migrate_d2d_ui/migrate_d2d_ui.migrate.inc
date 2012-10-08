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
  static function wizardConfiguration() {
    return array(
      'source_title' => t('Drupal'),
      'filename' => drupal_get_path('module', 'migrate_d2d_ui') . '/migrate_d2d_ui.migrate.inc',
      'overview' => t('This wizard supports importing into your Drupal site ' .
        'from another Drupal site (running Drupal version 5, 6, or 7). To be ' .
        'able to use this wizard, you must have the address and credentials of ' .
        'a database containing the full source Drupal site.'),
      'steps' => array(
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
      ),
    );
  }

  /**
   * Form for specifying where to obtain the Drupal content.
   */
  public function credentials($form, &$form_state) {
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
    // @todo: MySQL database name, SQLite database file descriptions should be
    // rewritten
    foreach ($drivers as $key => $driver) {
      $form['driver']['#options'][$key] = $driver->name();

      $form['settings'][$key] = $driver->getFormOptions(array());
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
}

/**
 * Attempt to authenticate using the provided credentials.
 *
 * @param unknown_type $form
 * @param unknown_type $form_state
 */
function migrate_d2d_ui_credentials_validate($form, &$form_state) {
  $form_state['storage']['database'] = $form_state['values'][$form_state['values']['driver']];
  $form_state['storage']['database']['driver'] = $form_state['values']['driver'];

  $connection = _migrate_d2d_ui_connection($form_state);

  // Make sure we can connect
  try {
    $admin_uid = $connection->select('users', 'u')
                 ->fields('u', array('uid'))
                 ->condition('uid', 1)
                 ->execute()
                 ->fetchField();
    if (!$admin_uid) {
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
function migrate_d2d_ui_users($form, &$form_state) {
  $connection = _migrate_d2d_ui_connection($form_state);

  // Format module is required by D6 and D7, table name changed
  // @todo: Support D5
  if ($connection->schema()->tableExists('filter_format')) {
    $version = '7';
  }
  else {
    $version = '6';
  }
  $form_state['storage']['source_version'] = $version;
  $form['#tree'] = TRUE;
  $num_users = $connection->select('users', 'u')
              ->condition('uid', 1, '>')
              ->countQuery()
              ->execute()
              ->fetchField();
  $form['users'] = array(
    '#prefix' => '<div>',
    '#markup' => t('Number of users to be migrated: !count', array('!count' => $num_users)),
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

function migrate_d2d_ui_users_validate($form, &$form_state) {
//  $form_state['storage']['role'] = $form_state['values']['role'];
// @todo: Do this at review time?
  $group_name = $form_state['storage']['group_name'];
  $version = $form_state['storage']['source_version'];
  $arguments = array(
    'description' => t('Migration of users from Drupal @version',
            array('@version' => $version)),
  );
  migrate_d2d_ui_add_migration($form_state, "DrupalUser{$version}Migration",
                               $group_name . 'User', $arguments);
}

/**
 * Choose what source vocabularies to import into what destination vocabularies.
 */
function migrate_d2d_ui_taxonomy($form, &$form_state) {
  $connection = _migrate_d2d_ui_connection($form_state);

  if ($connection->schema()->tableExists('taxonomy_vocabulary')) {
    $vocab_table = 'taxonomy_vocabulary';
  }
  elseif ($connection->schema()->tableExists('vocabulary')) {
    $vocab_table = 'vocabulary';
  }
  else {
    $vocab_table = '';
    $vocab_count = 0;
  }

  if ($vocab_table) {
    $vocab_count = $connection->select($vocab_table, 'v')
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
    $vocab_options[$vid] = $vocab->name;
  }
  $result = $connection->select($vocab_table, 'v')
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
function migrate_d2d_ui_taxonomy_validate($form, &$form_state) {
  $form_state['storage']['vocab'] = $form_state['values']['vocab'];
}

/**
 * Form for specifying where to obtain the Drupal content.
 */
function migrate_d2d_ui_content_types($form, &$form_state) {
  $connection = _migrate_d2d_ui_connection($form_state);

  $type_count = $connection->select('node_type', 't')
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
    '0' => t('--Create node type--'),
  );
  $node_options = array();
  $local_types = node_type_get_types();
  foreach ($local_types as $type => $info) {
    $node_options[$type] = $info->name;
  }
  $result = $connection->select('node_type', 't')
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
function migrate_d2d_ui_content_types_validate($form, &$form_state) {
  // Start with the placeholder
  $step = 5;
  foreach ($form_state['values']['type'] as $source_type => $dest_type) {
    // For each content type to be migrated, add a step
    if ($dest_type != '0' && $dest_type != '-1') {
      $form_state['step_information'][$step++] = array(
        'form' => 'migrate_d2d_ui_fields',
        'source_type' => $source_type,
        'dest_type' => $dest_type,
        'name' => t('@type nodes', array('@type' => $source_type)),
      );
    }
  }
  $form_state['step_information'][$step] =
    array('form' => 'migrate_d2d_ui_review', 'name' => t('Final review'));
  $form_state['storage']['type'] = $form_state['values']['type'];
}

/**
 * Form for mapping fields between two specific content types.
 */
function migrate_d2d_ui_fields($form, &$form_state) {
  $step = $form_state['step'];
  $step_info = $form_state['step_information'][$step];
  $source_type = $step_info['source_type'];
  $dest_type = $step_info['dest_type'];
  $connection = _migrate_d2d_ui_connection($form_state);

  // Temporarily register a migration, as a lazy way to get the available fields.
  $source_version = $form_state['storage']['source_version'];
  $source_class = "DrupalNode{$source_version}Migration";
  $arguments = array(
    'source_connection' => $form_state['storage']['group_name'] . '_legacy',
    'source_version' => $source_version,
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

function migrate_d2d_ui_fields_validate($form, &$form_state) {
  $step = $form_state['step'];
  $step_info = $form_state['step_information'][$step];
  $source_type = $step_info['source_type'];
  $form_state['storage']['field'][$source_type] = $form_state['values']['field'];
}

/**
 * Form for mapping fields between two specific content types.
 */
function migrate_d2d_ui_review($form, &$form_state) {
  $connection = _migrate_d2d_ui_connection($form_state);

  $message = t('<p>Please review your migration configuration. When you submit this
    form, migration processes will be created and you will be deposited at the
    migration dashboard.</p>');
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
  $admin_name = $connection->select('users', 'u')
                ->fields('u', array('name'))
                ->condition('uid', 1)
                ->execute()
                ->fetchField();
  $message .= t('<p>All user accounts except the administrative account (%admin)
    will be imported.</p>', array('%admin' => $admin_name));

  $vocabs_created = $vocabs_ignored = $vocabs_mapped = '';
  if ($connection->schema()->tableExists('taxonomy_vocabulary')) {
    $vocab_table = 'taxonomy_vocabulary';
  }
  elseif ($connection->schema()->tableExists('vocabulary')) {
    $vocab_table = 'vocabulary';
  }
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
    $source_name = $connection->select('node_type', 't')
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

  $form['description'] = array(
    '#prefix' => '<div>',
    '#markup' => $message,
    '#suffix' => '</div>',
  );
  return $form;
}

/**
 * Create a connection for the legacy database, and return it.
 *
 * @param array $form_state
 *
 * @return DatabaseConnection
 */
function _migrate_d2d_ui_connection($form_state) {
  $database = $form_state['storage']['database'];
  $group_name = $form_state['storage']['group_name'];
  $connection_key = $group_name . '_legacy';
  Database::addConnectionInfo($connection_key, 'default', $database);
  return Database::getConnection('default', $connection_key);
}

function migrate_d2d_ui_add_migration(&$form_state, $machine_name,
    $class_name, $arguments) {
  $group_name = $form_state['storage']['group_name'];
  $version = $form_state['storage']['source_version'];
  $arguments += array(
      'source_version' => $version,
      'source_connection' => $group_name . '_legacy',
      'source_database' => $form_state['storage']['database'],
    );
  migrate_ui_wizard_add_migration($form_state, $class_name,
                                  $machine_name, $arguments);
}
