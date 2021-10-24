Drupal-to-Backdrop Migration
======================

This is a framework based on the Migrate API to ease building migrations
from a Drupal site to Backdrop. This module ddresses contemporary needs
to migrate to Backdrop and it's intended to help serve as a
proof-of-concept for incorporating the migration approach into Backdrop
core as an upgrade path.

Requirements
------------
This module requires that the following modules are also enabled:

* [Migrate](https://github.com/backdrop-contrib/migrate)

Installation
------------

- Install this module using the official Backdrop CMS instructions at
  https://docs.backdropcms.org/documentation/extend-with-modules.

- Visit the configuration page under Administration > Configuration > Category >
  Foo (admin/config/category/foo) and enter the required information.
- Any additional steps.

How to use this module
------------
###migrate_d2b module

The core framework provided here is used by providing your own module,
which will register instances of the migrate_d2d classes or derivations
of them. See the migrate_d2b_example sub-module for one approach, where
instances are registered when the Backdrop caches are cleared.
Note: Registration will update previously registered classes with any
new argument changes.

###migrate_d2b_ui
This module provides a wizard-based UI for defining your
Drupal-to-Backdrop migrations. The wizard is appropriate for
non-technical users to configure and run the Drupal-to-Backdrop
migration and/or when you don't have to do any special manipulation of
data along the way.

Enabling the migrate_d2b_ui module will add an "Import from Drupal"
sub-tab to the Migrate dashboard. Visit that tab, enter the credentials
for your source Drupal database (versions 5, 6, or 7), and follow the
steps to define how your legacy content maps to the destination Backdrop
site.

Documentation <!-- Do not include if you have not created a wiki page. -->
-------------

Additional documentation is located in [the Wiki](https://github.com/backdrop-contrib/migrate_d2b/wiki/Documentation).

Current Maintainers
-------------------
- [Ryan Ositis](https://github.com/rositis)
<!-- You may also wish to add: -->
- Seeking additional maintainers.

Credits
-------

- Ported to Backdrop CMS by [Ryan Ositis](https://github.com/rositis).
- Originally written for Drupal by [Mike Ryan](https://www.drupal.org/u/mikeryan).
- Based on [Drupal-to-Drupal data migration](https://www.drupal.org/project/migrate_d2d).

License
-------
This project is GPL v2 software.
See the LICENSE.txt file in this directory for complete text.

<!-- If your project includes other libraries that are licensed in a way that is
compatible with GPL v2, you can list that here too, for example: `Foo library is
licensed under the MIT license.` -->
