<?php
/**
 * @file migrate_d2d_example.migrate.inc
 */

/**
 * This is example code for a Backdrop 6 to Backdrop 7 migration. This won't actually
 * work without defining the referenced vocabularies, content types, and fields
 * on each side.
 */

/**
 * Implements hook_migrate_api().
 */
function migrate_d2d_example_migrate_api() {
  /**
   * Declare the api version and migration group.
   */
  $api = array(
    'api' => 2,
    'groups' => array(
      'example_group' => array(
        'title' => t('Backdrop 6 to Backdrop 7 example migrations.'),
      ),
      'migrations' => array(),
    ),
  );

  /**
   * Each migration being registered takes an array of arguments, some required
   * and some optional. Start with the common arguments required by all - the
   * source_connection (connection key, set up in settings.php, pointing to
   * the Backdrop 6 database), source_version (major version of Backdrop), and
   * group_name (a.k.a. import job).
   */
  $common_arguments = array(
    'source_connection' => 'legacy',
    'source_version' => 6,
    'group_name' => 'example_group',
  );

  // Register the user migration.
  // We just use the migrate_d2b D6 migration class as-is.
  $api['migrations']['User'] = $common_arguments + array(
    'description' => t('Migration of users from Backdrop 6'),
    'class_name' => 'BackdropUser6Migration',
  );

  // For vocabulary migrations, source_vocabulary and destination_vocabulary are
  // required arguments. Note that in Backdrop 6 vocabularies did not have machine
  // names, so we use the vocabulary ID to uniquely identify them.
  $vocabulary_arguments = array(
    'Countries' => array(
      'description' => t('Migration of Countries terms from Backdrop 6'),
      'source_vocabulary' => '1', // D6 Vocabulary ID
      'destination_vocabulary' => 'countries',
    ),
    'Topics' => array(
      'description' => t('Migration of Topics terms from Backdrop 6'),
      'source_vocabulary' => '2', // D6 Vocabulary ID
      'destination_vocabulary' => 'topics',
    ),
  );

  // Again, we're using the migrate_d2b class directly.
  // The soft dependency says that while we don't have to run the user migration
  // first, we want to make sure it's listed first so the vocabularies are
  // listed right ahead of the node migrations.
  $common_vocabulary_arguments = $common_arguments + array(
    'class_name' => 'BackdropTerm6Migration',
    'soft_dependencies' => array('User'),
  );
  foreach ($vocabulary_arguments as $migration_name => $arguments) {
    $arguments += $common_vocabulary_arguments;
    $api['migrations'][$migration_name] = $arguments;
  }

  // Node migrations - each has its own class derived from the migrate_d2b class,
  // specifying its particular field mappings and transformations. source_type
  // and destination_type are required arguments.
  $node_arguments = array(
    'Image' => array(
      'class_name' => 'ExampleImageMigration',
      'description' => t('Migration of image nodes from Backdrop 6'),
      'source_type' => 'image',
      'destination_type' => 'image',
    ),
    'Article' => array(
      'class_name' => 'ExampleArticleMigration',
      'description' => t('Migration of article nodes from Backdrop 6'),
      'source_type' => 'my_article',
      'destination_type' => 'new_article',
      'dependencies' => array('Image', 'Countries', 'Topics'),
    ),
  );

  // Tell the node migrations where the users are coming from, so they can
  // set up the dependency and resolve D6->D7 uids.
  $common_node_arguments = $common_arguments + array(
    'user_migration' => 'User',
  );

  foreach ($node_arguments as $migration_name => $arguments) {
    $arguments = array_merge_recursive($arguments, $common_node_arguments);
    $api['migrations'][$migration_name] = $arguments;
  }

  // Menu migrations -- same requirements as users
  $menu_arguments = $common_arguments + array(
    'description' => t('Migration of menus from Backdrop 6'),
    'class_name' => 'BackdropMenu6Migration',
  );

  // We just use the migrate_d2b D6 migration class as-is.
  $api['migrations']['Menu'] = $menu_arguments;

  // Menu links migrations -- basic requirements, plus must set up menu dependency
  $menu_links_arguments = $common_arguments + array(
    'description' => t('Migration of menu links from Backdrop 6'),
    'class_name' => 'BackdropMenuLinks6Migration',
    'menu_migration' => 'Menu',
  );
  $api['migrations']['MenuLinks'] = $menu_links_arguments;

  return $api;
}
