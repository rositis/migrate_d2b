<?php

/**
 * Base class for all user migrations - handles commonalities across all
 * supported source Drupal versions.
 */
abstract class DrupalUserMigration extends DrupalMigration {
  /**
   * If there's a user picture migration, keep track of the machine name
   * for dependency management and sourceMigration.
   *
   * @var string
   */
  protected $pictureMigration = '';

  /**
   * @param array $arguments
   * picture_migration - Machine name of a picture migration, used to establish
   *   dependencies and a sourceMigration for the picture mapping.
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    if (!empty($arguments['picture_migration'])) {
      $this->pictureMigration = $arguments['picture_migration'];
      $this->dependencies[] = $this->pictureMigration;
    }

    // Get any CCK/core fields attached to users.
    $this->sourceFields += $this->version->getSourceFields(
      'user', 'user');

    if ($this->moduleExists('path')) {
      $this->sourceFields['path'] = array('label' => t('Node: Path alias'));
    }

    $this->source = new MigrateSourceSQL($this->query(), $this->sourceFields,
      NULL, $this->sourceOptions);

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'uid' => array('type' => 'int',
                       'unsigned' => TRUE,
                       'not null' => TRUE,
                       'description' => 'Source user ID',
                       'alias' => 'u',
                      ),
      ),
      MigrateDestinationUser::getKeySchema()
    );

    // Most mappings are straight-forward
    // @todo: Anything special to do with theme?
    $this->addSimpleMappings(array('pass', 'mail', 'theme', 'signature',
      'created', 'access', 'login', 'status', 'language', 'init', 'timezone'));
    // user_save() expects the data field to be unpacked
    $this->addFieldMapping('data', 'data')
         ->callbacks('unserialize');
    // Dedupe username collisions by appending _1, _2, etc.
    $this->addFieldMapping('name', 'name')
         ->dedupe('users', 'name');
    if (isset($this->pictureMigration)) {
      $this->addFieldMapping('picture', 'picture')
           ->sourceMigration($this->pictureMigration);
    }
    else {
      $this->addFieldMapping('picture', 'picture');
    }
    $this->addFieldMapping('roles')
         ->description(t('TODO: handle roles automatically'))
         ->issuePriority(MigrateFieldMapping::ISSUE_PRIORITY_MEDIUM);
    $this->addFieldMapping('signature_format', 'signature_format')
         ->callbacks(array($this, 'mapFormat'));

    $this->addUnmigratedDestinations(array('is_new', 'role_names'));

    if (module_exists('path') && $this->moduleExists('path')) {
      $this->addFieldMapping('path', 'path')
           ->description('Handled in prepareRow');
    }
    else {
      if (module_exists('path')) {
        $this->addUnmigratedDestinations(array('path'));
      }
      elseif ($this->moduleExists('path')) {
        $this->addUnmigratedSources(array('path'));
      }
    }

    if (module_exists('pathauto')) {
      $this->addFieldMapping('pathauto')
           ->description('By default, disable in favor of migrated paths')
           ->defaultValue(0);
    }
  }

  /**
   * Query for the basic user data. Same query is used for all currently-supported
   * versions of Drupal.
   *
   * @return QueryConditionInterface
   */
  protected function query() {
    // Do not attempt to migrate the anonymous or admin user rows.
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('users', 'u')
             ->fields('u')
             ->condition('uid', 1, '>');
    return $query;
  }

  /**
   * Review a data row after fetch, returning FALSE to skip it.
   *
   * @param $row
   * @return bool
   */
  public function prepareRow($row) {
    if (parent::prepareRow($row) === FALSE) {
      return FALSE;
    }
    /*
    // On initial import, skip accounts with email addresses already existing
    // on the destination side (we assume these are manually-created staff
    // accounts or the like).
    // TODO: Add options to overwrite such accounts, throw errors, etc.
    if (empty($row->migrate_map_sourceid1)) {
      $uid = db_select('users', 'u')
             ->fields('u', array('uid'))
             ->condition('mail', $row->mail)
             ->execute()
             ->fetchField();
      if ($uid) {
        return FALSE;
      }
    }*/

    // Add the path to the source row, if relevant
    if ($this->moduleExists('path')) {
      $path = $this->version->getPath('user/' . $row->uid);
      if ($path) {
        $row->path = $path;
      }
    }

    // Pick up profile data.
    $this->version->getSourceValues($row, $row->uid);
    return TRUE;
  }

  /**
   * Actions to take after a user account has been saved.
   *
   * @param $account
   * @param $row
   */
  public function complete($account, $row) {
    // Unlike nodes and taxonomy terms, core does not automatically save an
    // alias in a user entity, we must do it ourselves.
    if (module_exists('path')) {
      if (isset($account->path['alias'])) {
        $path = array(
          'source' => 'user/' . $account->uid,
          'alias' => $account->path['alias'],
        );
        migrate_instrument_start('path_save');
        path_save($path);
        migrate_instrument_stop('path_save');
      }
    }
    // If we've migrated a picture, update its ownership
    if (isset($this->pictureMigration)) {
      if ($account->picture) {
        db_update('file_managed')
          ->fields(array('uid' => $account->uid))
          ->condition('fid', $account->picture)
          ->execute();
      }
    }
  }
}
