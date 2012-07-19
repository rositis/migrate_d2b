<?php
/**
 * @file
 * Summary
 */

/**
 * Base class for all role migrations - handles commonalities across all
 * supported source Drupal versions.
 */
abstract class DrupalRoleMigration extends DrupalMigration {
  /**
   * Allow the implementor to map roles by name (e.g., map the legacy "Fan Club"
   * role to the destination "fan club member" role).
   *
   * @var array
   */
  protected $roleMappings = array();

  /**
   * @param array $arguments
   * picture_migration - Machine name of a picture migration, used to establish
   *   dependencies and a sourceMigration for the picture mapping.
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);
    if (!empty($arguments['role_mappings'])) {
      $this->roleMappings = $arguments['role_mappings'];
    }

    $this->source = new MigrateSourceSQL($this->query(), $this->sourceFields,
      NULL, $this->sourceOptions);

    $this->map = new MigrateSQLMap($this->machineName,
      array(
        'rid' => array('type' => 'int',
                       'unsigned' => TRUE,
                       'not null' => TRUE,
                       'description' => 'Source role ID',
                      ),
      ),
      MigrateDestinationRole::getKeySchema()
    );

    $this->destination = new MigrateDestinationRole();

    $this->addSimpleMappings(array('name'));
  }

  /**
   * Query for the basic role data.
   *
   * @return QueryConditionInterface
   */
  protected function query() {
    // Do not attempt to migrate the anonymous or authenticated roles.
    $query = Database::getConnection('default', $this->sourceConnection)
             ->select('role', 'r')
             ->fields('r')
             ->condition('rid', 2, '>');
    return $query;
  }

  /**
   * Review a data row after fetch, returning FALSE to skip it.
   *
   * @param $row
   * @return bool
   */
  public function prepare($role, $row) {
    if (isset($this->roleMappings[$role->name])) {
      $destination_name = $this->roleMappings[$role->name];
      $destination_rid = db_select('role', 'r')
                         ->fields('r', array('rid'))
                         ->condition('name', $destination_name)
                         ->execute()
                         ->fetchField();
      if ($destination_rid) {
        $role->rid = $destination_rid;
        // Make sure we don't change the destination name
        $role->name = $destination_name;
      }
    }
  }

  /**
   * Override of Migration::rollback() - make sure we don't delete pre-existing
   * roles we've mapped to.
   */
  protected function rollback() {
    $return = MigrationBase::RESULT_COMPLETED;
    $idlist = $this->getOption('idlist');
    if ($idlist) {
      // Make the IDs keys, to more easily identify them
      $idlist = array_flip(explode(',', $idlist));
    }

    // Easily lookup destination role names
    $roles_to_skip = array_flip($this->roleMappings);

    foreach ($this->map as $destination_key) {
      if ($this->timeOptionExceeded()) {
        break;
      }
      if (($return = $this->checkStatus()) != MigrationBase::RESULT_COMPLETED) {
        break;
      }
      if ($this->itemOptionExceeded()) {
        break;
      }
      $this->currentSourceKey = $this->map->getCurrentKey();

      // If there's an idlist, skip anything not in the list
      if ($idlist && !isset($idlist[$this->currentSourceKey['sourceid1']])) {
        continue;
      }

      try {
        // Skip when the destination key is null
        $skip = FALSE;
        $key_value = reset($destination_key);
        if (is_null($key_value)) {
          $skip = TRUE;
        }
        // Or if the destination is pre-existing (a value in roleMappings).
        $destination_name = db_select('role', 'r')
                            ->fields('r', array('name'))
                            ->condition('rid', $key_value)
                            ->execute()
                            ->fetchField();
        if (isset($roles_to_skip[$destination_name])) {
          $skip = TRUE;
        }
        if (!$skip) {
          migrate_instrument_start('destination rollback');
          $this->destination->rollback((array)$destination_key);
          migrate_instrument_stop('destination rollback');
        }

        migrate_instrument_start('rollback map/message update');
        $this->map->delete($this->currentSourceKey);
        migrate_instrument_stop('rollback map/message update');
        $this->total_successes++;
        $this->successes_since_feedback++;
      }
      catch (Exception $e) {
        // TODO: At least count failures
        continue;
      }
      $this->total_processed++;
      $this->processed_since_feedback++;
    }

    $this->map->clearMessages();

    $this->progressMessage($return);

    return $return;
  }
}
