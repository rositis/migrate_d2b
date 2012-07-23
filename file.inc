<?php
/**
 * @file
 * Summary
 */

/**
 * Base class for all file migrations - handles commonalities across all
 * supported source Drupal versions.
 *
 * In addition to the arguments supported by DrupalMigration, the following
 * must be passed in the $arguments array:
 *  source_dir: Path to folder containing files to be migrated.
 *
 * The following optional arguments may be passed:
 * user_migration - Machine name of a user migration, used to establish
 *   dependencies and a sourceMigration for the uid mapping.
 * default_uid - Drupal 7 (destination) uid of the user account to use as
 *   the default.
 */
abstract class DrupalFileMigration extends DrupalMigration {
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    if (!empty($arguments['user_migration'])) {
       $user_migration = $arguments['user_migration'];
       $this->dependencies[] = $user_migration;
    }
    $this->sourceFields += $this->version->getSourceFields('file', 'file');
    // Allow derived classes to override this definition by setting it before
    // calling their parent constructor
    if (!isset($this->map)) {
      $this->map = new MigrateSQLMap($this->machineName,
        array(
          'fid' => array(
            'type' => 'int',
            'unsigned' => TRUE,
            'not null' => TRUE,
            'description' => 'Source file ID',
            'alias' => 'f',
          ),
        ),
        MigrateDestinationFile::getKeySchema()
      );
    }
    $this->source = new MigrateSourceSQL($this->query(),
      $this->sourceFields, NULL, $this->sourceOptions);

    $this->destination = new MigrateDestinationFile();

    $this->highwaterField = array(
      'name' => 'timestamp',
      'alias' => 'f',
      'type' => 'int',
    );

    // Setup common mappings
    $this->addFieldMapping('destination_dir')
         ->defaultValue('public://legacy');
    $this->addFieldMapping('source_dir')
         ->defaultValue($arguments['source_dir']);
    $this->addFieldMapping('timestamp', 'timestamp');
    $this->addFieldMapping('file_replace')
         ->defaultValue(MigrateFile::FILE_EXISTS_REUSE);
    $this->addFieldmapping('preserve_files')
          ->defaultValue(TRUE);

    if (!empty($arguments['default_uid'])) {
      $default_uid = $arguments['default_uid'];
    }
    else {
      $default_uid = 1;
    }
    if (isset($user_migration)) {
      $this->addFieldMapping('uid', 'uid')
           ->sourceMigration($user_migration)
           ->defaultValue($default_uid);
    }
    else {
      $this->addFieldMapping('uid')
           ->defaultValue($default_uid);
    }
    $this->addUnmigratedDestinations(array('path'));
    $this->addUnmigratedSources(array('filename', 'filemime', 'filesize',
                                'status'));
  }
}
