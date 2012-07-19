<?php

/**
 * Handling specific to a Drupal 7 source for roles.
 */
class DrupalRole7Migration extends DrupalRoleMigration {
  /**
   * @param array $arguments
   */
  public function __construct(array $arguments) {
    parent::__construct($arguments);

    $this->addSimpleMappings(array('weight'));
  }
}
